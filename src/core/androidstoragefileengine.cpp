// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>
#include <optional>

#include <QDateTime>
#include <QDirIterator>
#include <QFileInfo>
#include <private/qabstractfileengine_p.h>

#include "androidstorage.h"

namespace {

class DocumentIterator : public QAbstractFileEngineIterator {
 public:
  DocumentIterator(const QString &path, const QUrl &uri, QDir::Filters filters, const QStringList &names)
      : QAbstractFileEngineIterator(filters, names),
        path_(path),
        // The outer iterator applies filters; filtering here would hide directories needed for recursive traversal.
        iterator_(uri.isEmpty() ? path : uri.toString(QUrl::FullyEncoded), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot) {}

  bool hasNext() const override { return iterator_.hasNext(); }
  QString next() override {
    iterator_.next();
    return path_ + u'/' + currentFileName();
  }
  QString currentFileName() const override { return iterator_.fileName(); }
  QFileInfo currentFileInfo() const override { return QFileInfo(path_ + u'/' + currentFileName()); }

 private:
  const QString path_;
  QDirIterator iterator_;
};

// Keep stable local paths in collection/playlist URLs, but perform reads and enumeration through the granted document URI.
class DocumentFileEngine : public QAbstractFileEngine {
 public:
  DocumentFileEngine(const QString &path, const QUrl &uri)
      : path_(path), uri_(uri), engine_(QAbstractFileEngine::create(uri.toString(QUrl::FullyEncoded))) {}

  void setFileName(const QString &path) override {
    path_ = QDir::cleanPath(path);
    uri_ = AndroidStorage::DocumentUrlForPath(path_);
    engine_.reset(QAbstractFileEngine::create(uri_.isEmpty() ? path_ : uri_.toString(QUrl::FullyEncoded)));
  }

  bool open(QIODevice::OpenMode mode, std::optional<QFile::Permissions> permissions) override {
    if (mode & (QIODevice::WriteOnly | QIODevice::Append | QIODevice::Truncate)) {
      setError(QFile::PermissionsError, QStringLiteral("Android document path access is read-only"));
      return false;
    }
    const bool opened = engine_->open(mode, permissions);
    if (!opened) setError(engine_->error(), engine_->errorString());
    return opened;
  }
  bool close() override { return engine_->close(); }
  qint64 size() const override { return engine_->size(); }
  qint64 pos() const override { return engine_->pos(); }
  bool seek(qint64 position) override { return engine_->seek(position); }
  bool isSequential() const override { return engine_->isSequential(); }
  int handle() const override { return engine_->handle(); }
  qint64 read(char *data, qint64 count) override {
    const qint64 result = engine_->read(data, count);
    if (result < 0) setError(engine_->error(), engine_->errorString());
    return result;
  }
  bool caseSensitive() const override { return true; }
  bool isRelativePath() const override { return false; }
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  QDateTime fileTime(QFile::FileTime time) const override { return engine_->fileTime(time); }
#else
  QDateTime fileTime(FileTime time) const override { return engine_->fileTime(time); }
#endif
  FileFlags fileFlags(FileFlags flags) const override {
    FileFlags result = engine_->fileFlags(flags);
    result &= ~(WriteOwnerPerm | WriteUserPerm | WriteGroupPerm | WriteOtherPerm);
    if (path_.section(u'/', -1).startsWith(u'.')) result |= HiddenFlag;
    return result & flags;
  }
  QString fileName(FileName type) const override {
    switch (type) {
      case BaseName:
        return path_.section(u'/', -1);
      case PathName:
      case AbsolutePathName:
      case CanonicalPathName:
        return path_.left(path_.lastIndexOf(u'/'));
      case DefaultName:
      case AbsoluteName:
      case CanonicalName:
        return path_;
      default:
        return QString();
    }
  }
  Iterator *beginEntryList(QDir::Filters filters, const QStringList &names) override {
    return new DocumentIterator(path_, uri_, filters, names);
  }

 private:
  QString path_;
  QUrl uri_;
  std::unique_ptr<QAbstractFileEngine> engine_;
};

class DocumentFileEngineHandler : public QAbstractFileEngineHandler {
 public:
  QAbstractFileEngine *create(const QString &path) const override {
    const QUrl uri = AndroidStorage::DocumentUrlForPath(path);
    return uri.isEmpty() ? nullptr : new DocumentFileEngine(QDir::cleanPath(path), uri);
  }
};

}  // namespace

namespace AndroidStorage {

void InstallDocumentFileEngine() {
  static const DocumentFileEngineHandler handler;
}

}  // namespace AndroidStorage
