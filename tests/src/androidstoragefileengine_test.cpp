// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <private/qabstractfileengine_p.h>
#include <private/qfsfileengine_p.h>

#include "core/androidstorage.h"

namespace {

QString backing_path;
const QString kGrantedPath = QStringLiteral("/__strawberry_document_test__");

class MockIterator : public QAbstractFileEngineIterator {
 public:
  MockIterator(const QString &path, QDir::Filters filters, const QStringList &names)
      : QAbstractFileEngineIterator(filters, names), iterator_(path, names, filters) {}
  bool hasNext() const override { return iterator_.hasNext(); }
  QString next() override { return iterator_.next(); }
  QString currentFileName() const override { return iterator_.fileName(); }
  QFileInfo currentFileInfo() const override { return iterator_.fileInfo(); }
 private:
  QDirIterator iterator_;
};

class MockDocumentEngine : public QFSFileEngine {
 public:
  explicit MockDocumentEngine(const QString &path) : QFSFileEngine(path), path_(path) {}
  FileFlags fileFlags(FileFlags flags) const override { return QFSFileEngine::fileFlags(flags) & ~LocalDiskFlag; }
  Iterator *beginEntryList(QDir::Filters filters, const QStringList &names) override { return new MockIterator(path_, filters, names); }
 private:
  const QString path_;
};

class MockProvider : public QAbstractFileEngineHandler {
 public:
  QAbstractFileEngine *create(const QString &name) const override {
    const QUrl uri(name);
    return uri.scheme() == QStringLiteral("content") && uri.host() == QStringLiteral("test")
               ? new MockDocumentEngine(uri.path())
               : nullptr;
  }
};

}  // namespace

namespace AndroidStorage {

QUrl DocumentUrlForPath(const QString &path) {
  const QString cleaned = QDir::cleanPath(path);
  if (backing_path.isEmpty() || (cleaned != kGrantedPath && !cleaned.startsWith(kGrantedPath + u'/'))) return QUrl();
  QUrl uri;
  uri.setScheme(QStringLiteral("content"));
  uri.setHost(QStringLiteral("test"));
  uri.setPath(backing_path + cleaned.mid(kGrantedPath.size()));
  return uri;
}

}  // namespace AndroidStorage

TEST(AndroidStorageFileEngineTest, ReadsEnumeratesAndPreservesLocalIdentityWithoutWriteAccess) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  ASSERT_TRUE(QDir(directory.path()).mkdir(QStringLiteral("nested")));
  const QString name = QStringLiteral("nested/space % #\u97f3.wv");
  QFile original(directory.filePath(name));
  ASSERT_TRUE(original.open(QIODevice::WriteOnly));
  ASSERT_EQ(original.write("wvpk-test-data"), 14);
  original.close();
  MockProvider provider;
  AndroidStorage::InstallDocumentFileEngine();
  backing_path = directory.path();

  const QString logical_path = kGrantedPath + u'/' + name;
  {
    QFile file(logical_path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), QByteArray("wvpk-test-data"));
    EXPECT_TRUE(file.seek(4));
    EXPECT_EQ(file.read(5), QByteArray("-test"));
    EXPECT_GE(file.handle(), 0);
    file.close();
    EXPECT_FALSE(file.open(QIODevice::WriteOnly));
    EXPECT_EQ(file.error(), QFile::PermissionsError);
    QFileInfo info(logical_path);
    EXPECT_TRUE(info.exists());
    EXPECT_TRUE(info.isFile());
    EXPECT_EQ(info.size(), 14);
    EXPECT_EQ(info.absoluteFilePath(), logical_path);
    EXPECT_EQ(info.canonicalFilePath(), logical_path);
    EXPECT_FALSE(info.isWritable());
    EXPECT_TRUE(info.lastModified().isValid());
    EXPECT_EQ(QDir(AndroidStorage::DocumentUrlForPath(kGrantedPath).toString()).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{QStringLiteral("nested")});
    EXPECT_TRUE(QFileInfo(kGrantedPath + QStringLiteral("/nested")).isDir());
    QDirIterator iterator(kGrantedPath, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    ASSERT_TRUE(iterator.hasNext());
    EXPECT_EQ(iterator.next(), logical_path);
    EXPECT_FALSE(iterator.hasNext());
    EXPECT_FALSE(QFileInfo::exists(kGrantedPath + QStringLiteral("/absent.wv")));
    EXPECT_TRUE(AndroidStorage::DocumentUrlForPath(kGrantedPath + QStringLiteral("-other/file.wv")).isEmpty());
    EXPECT_TRUE(AndroidStorage::DocumentUrlForPath(kGrantedPath + QStringLiteral("/../outside.wv")).isEmpty());
  }
  backing_path.clear();
}
