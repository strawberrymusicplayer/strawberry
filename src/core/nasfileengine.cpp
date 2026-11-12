// SPDX-License-Identifier: GPL-3.0-or-later

#include "nasfileengine.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <utility>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>
#include <private/qabstractfileengine_p.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "logging.h"

#ifdef Q_OS_ANDROID
#  include <QJniEnvironment>
#  include <QJniObject>
#  include <QJsonArray>
#  include <QJsonDocument>
#  include <QJsonObject>
#  include <QCoreApplication>
#endif

using namespace Qt::Literals::StringLiterals;

namespace {

const QString prefix = QStringLiteral("/strawberry-nas/");
QReadWriteLock locations_lock;
QList<Nas::Location> locations;
thread_local quint64 failure_serial = 0;

struct ContextDeleter {
  void operator()(smb2_context *context) const { smb2_destroy_context(context); }
};

class Connection {
 public:
  explicit Connection(const Nas::Location &location) : context_(smb2_init_context()) {
    if (!context_) {
      Fail(QObject::tr("Could not allocate NAS connection"));
      return;
    }
    smb2_set_timeout(context_.get(), 8);
    smb2_set_version(context_.get(), SMB2_VERSION_ANY);
    smb2_set_security_mode(context_.get(), SMB2_NEGOTIATE_SIGNING_ENABLED | SMB2_NEGOTIATE_SIGNING_REQUIRED);
    smb2_set_sign(context_.get(), 1);
    smb2_set_user(context_.get(), location.user.toUtf8().constData());
    smb2_set_password(context_.get(), location.password.toUtf8().constData());
    smb2_set_domain(context_.get(), location.domain.toUtf8().constData());
    if (smb2_connect_share(context_.get(), location.server.toUtf8().constData(), location.share.toUtf8().constData(), location.user.toUtf8().constData()) < 0) {
      // The library error may contain authentication details; never log it.
      Fail(QObject::tr("Could not connect to NAS. Check the address, share, account, network and SMB signing support."));
    }
  }
  smb2_context *get() const { return error_.isEmpty() ? context_.get() : nullptr; }
  QString error() const { return error_; }
  void Fail(const QString &message) {
    error_ = message;
    ++failure_serial;
    qLog(Error) << message;
  }

 private:
  std::unique_ptr<smb2_context, ContextDeleter> context_;
  QString error_;
};

std::optional<Nas::Location> Resolve(const QString &path, QString *remote) {
  QReadLocker locker(&locations_lock);
  for (const Nas::Location &location : std::as_const(locations)) {
    const QString root = location.path();
    if (path == root || path.startsWith(root + u'/')) {
      *remote = location.folder;
      if (path != root) {
        if (!remote->isEmpty()) *remote += u'/';
        *remote += path.mid(root.size() + 1);
      }
      return location;
    }
  }
  return std::nullopt;
}

class NasIterator : public QAbstractFileEngineIterator {
 public:
  NasIterator(const QString &path, const QStringList &entries, QDir::Filters filters, const QStringList &names)
      : QAbstractFileEngineIterator(filters, names), path_(path), entries_(entries) {}
  bool hasNext() const override { return index_ + 1 < entries_.size(); }
  QString next() override {
    ++index_;
    return path_ + u'/' + currentFileName();
  }
  QString currentFileName() const override { return entries_.value(index_); }
  QFileInfo currentFileInfo() const override { return QFileInfo(path_ + u'/' + currentFileName()); }

 private:
  QString path_;
  QStringList entries_;
  qsizetype index_ = -1;
};

class Engine : public QAbstractFileEngine {
 public:
  explicit Engine(const QString &path) { setFileName(path); }
  ~Engine() override { close(); }
  void setFileName(const QString &path) override {
    close();
    path_ = QDir::cleanPath(path);
    connection_.reset();
    location_ = Resolve(path_, &remote_);
    stat_.reset();
  }
  bool open(QIODevice::OpenMode mode, std::optional<QFile::Permissions>) override {
    if (!(mode & QIODevice::ReadOnly) || (mode & (QIODevice::WriteOnly | QIODevice::Append | QIODevice::Truncate))) {
      setError(QFile::PermissionsError, QStringLiteral("NAS locations are read-only"));
      return false;
    }
    if (!Connect()) return false;
    file_ = smb2_open(connection_->get(), remote_.toUtf8().constData(), O_RDONLY);
    if (!file_) return Fail(QFile::OpenError, QStringLiteral("Could not open NAS file"));
    position_ = 0;
    return true;
  }
  bool close() override {
    if (!file_) return true;
    const int result = smb2_close(connection_->get(), file_);
    file_ = nullptr;
    return result >= 0 || Fail(QFile::ReadError, QStringLiteral("Could not close NAS file"));
  }
  bool remove() override { return Fail(QFile::PermissionsError, QStringLiteral("NAS locations are read-only")); }
  bool rename(const QString &) override { return Fail(QFile::PermissionsError, QStringLiteral("NAS locations are read-only")); }
  qint64 size() const override {
    return Stat() ? static_cast<qint64>(stat_->smb2_size) : -1;
  }
  qint64 pos() const override { return position_; }
  bool seek(qint64 position) override {
    if (position < 0 || !file_) return Fail(QFile::PositionError, QStringLiteral("Invalid NAS seek"));
    position_ = position;
    return true;
  }
  qint64 read(char *data, qint64 count) override {
    if (!file_ || count < 0) {
      Fail(QFile::ReadError, QStringLiteral("NAS file is not open"));
      return -1;
    }
    qint64 total = 0;
    while (total < count) {
      const uint32_t length = static_cast<uint32_t>(std::min<qint64>(count - total, 64 * 1024));
      const int result = smb2_pread(connection_->get(), file_, reinterpret_cast<uint8_t*>(data + total), length, static_cast<uint64_t>(position_));
      if (result < 0) {
        Fail(QFile::ReadError, QStringLiteral("Could not read NAS file. Reconnect and retry playback. Error %1").arg(result));
        return -1;
      }
      if (result == 0) break;
      position_ += result;
      total += result;
    }
    return total;
  }
  bool isSequential() const override { return false; }
  bool isRelativePath() const override { return false; }
  bool caseSensitive() const override { return true; }
  FileFlags fileFlags(FileFlags flags) const override {
    if (!Stat()) return {};
    FileFlags result = ExistsFlag | ReadOwnerPerm | ReadUserPerm | ReadGroupPerm | ReadOtherPerm;
    if (stat_->smb2_type == SMB2_TYPE_DIRECTORY) result |= DirectoryType;
    else if (stat_->smb2_type == SMB2_TYPE_FILE) result |= FileType;
    else return {};
    if (path_.section(u'/', -1).startsWith(u'.')) result |= HiddenFlag;
    return result & flags;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  QDateTime fileTime(QFile::FileTime time) const override {
    if (!Stat()) return {};
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time == QFile::FileModificationTime ? stat_->smb2_mtime : stat_->smb2_btime));
  }
#else
  QDateTime fileTime(FileTime time) const override {
    if (!Stat()) return {};
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time == ModificationTime ? stat_->smb2_mtime : stat_->smb2_btime));
  }
#endif
  QString fileName(FileName type) const override {
    switch (type) {
      case BaseName: return path_.section(u'/', -1);
      case PathName:
      case AbsolutePathName:
      case CanonicalPathName: return path_.left(path_.lastIndexOf(u'/'));
      case DefaultName:
      case AbsoluteName:
      case CanonicalName: return path_;
      default: return {};
    }
  }
  Iterator *beginEntryList(QDir::Filters filters, const QStringList &names) override {
    QStringList entries;
    if (Stat()) {
      smb2dir *dir = smb2_opendir(connection_->get(), remote_.toUtf8().constData());
      if (!dir) Fail(QFile::ReadError, QStringLiteral("Could not list NAS folder. Existing collection entries will be retained."));
      else {
        while (const smb2dirent *entry = smb2_readdir(connection_->get(), dir)) {
          const QString name = QString::fromUtf8(entry->name);
          if (name != QStringLiteral(".") && name != QStringLiteral("..") && !name.contains(u'/') && !name.contains(u'\\') && entry->st.smb2_type != SMB2_TYPE_LINK) entries.append(name);
        }
        smb2_closedir(connection_->get(), dir);
      }
    }
    return new NasIterator(path_, entries, filters, names);
  }

 private:
  bool Fail(QFile::FileError code, const QString &message) const {
    const_cast<Engine*>(this)->setError(code, message);
    ++failure_serial;
    qLog(Error) << message;
    return false;
  }
  bool Connect() const {
    if (!location_) return Fail(QFile::OpenError, QStringLiteral("NAS location is not configured"));
    if (!connection_) connection_ = std::make_unique<Connection>(*location_);
    if (!connection_->get()) {
      const_cast<Engine*>(this)->setError(QFile::OpenError, connection_->error());
      return false;
    }
    return true;
  }
  bool Stat() const {
    if (stat_) return true;
    if (!Connect()) return false;
    smb2_stat_64 value = {};
    const int result = smb2_stat(connection_->get(), remote_.toUtf8().constData(), &value);
    if (result == -ENOENT || result == -ENOTDIR) return false;
    if (result < 0) return Fail(QFile::ReadError, QStringLiteral("Could not inspect NAS file or folder. Error %1").arg(result));
    stat_ = value;
    return true;
  }
  QString path_;
  QString remote_;
  std::optional<Nas::Location> location_;
  mutable std::unique_ptr<Connection> connection_;
  mutable std::optional<smb2_stat_64> stat_;
  smb2fh *file_ = nullptr;
  qint64 position_ = 0;
};

class Handler : public QAbstractFileEngineHandler {
 public:
  QAbstractFileEngine *create(const QString &path) const override {
    return Nas::IsPath(path) ? new Engine(path) : nullptr;
  }
};

}  // namespace

namespace Nas {

QString Location::path() const { return prefix + id; }
QString Location::label() const { return QStringLiteral("smb://") + server + u'/' + share + (folder.isEmpty() ? QString() : u'/' + folder); }
QString DisplayName(const QString &path) {
  QString remote;
  const auto location = Resolve(path, &remote);
  return location ? location->label() : path;
}
bool IsPath(const QString &path) { return path.startsWith(prefix); }
quint64 FailureSerial() { return failure_serial; }
void Install() { static const Handler handler; }
QList<Location> Locations() {
  QReadLocker locker(&locations_lock);
  return locations;
}
void SetLocations(const QList<Location> &value) {
  QWriteLocker locker(&locations_lock);
  locations = value;
}
QString Validate(const Location &location) {
  const QRegularExpression id(QStringLiteral("^[a-f0-9-]{36}$"));
  if (!id.match(location.id).hasMatch() || QUuid(location.id).isNull()) return QObject::tr("Invalid NAS location identifier");
  const QUrl address(QStringLiteral("smb://") + location.server, QUrl::StrictMode);
  if (!address.isValid() || address.host().isEmpty() || address.port(445) < 1 || location.server.contains(QRegularExpression(QStringLiteral("[\\s/@\\\\?#]"))) || location.server.contains(u'\0')) return QObject::tr("Enter a server name or IP address, not a URL");
  if (location.share.isEmpty() || location.share == QStringLiteral(".") || location.share == QStringLiteral("..") || location.share.contains(u'/') || location.share.contains(u'\\') || location.share.contains(u'\0')) return QObject::tr("Enter a single share name");
  if (location.folder.startsWith(u'/') || location.folder.contains(u'\\') || location.folder.contains(u'\0') || location.folder.split(u'/').contains(QStringLiteral(".."))) return QObject::tr("Enter a folder relative to the share, without '..'");
  if (location.user.isEmpty() || location.user.contains(u'\0') || location.password.contains(u'\0') || location.domain.contains(u'\0')) return QObject::tr("Enter a NAS account. Anonymous access is not supported.");
  return {};
}
QString Check(const Location &location) {
  const QString invalid = Validate(location);
  if (!invalid.isEmpty()) return invalid;
  Connection connection(location);
  if (!connection.get()) return connection.error();
  smb2dir *dir = smb2_opendir(connection.get(), location.folder.toUtf8().constData());
  if (!dir) return QObject::tr("Could not open the NAS folder. Check the folder and read permissions.");
  smb2_closedir(connection.get(), dir);
  return {};
}

#ifdef Q_OS_ANDROID
namespace {
constexpr char vault[] = "nl/dewitweb/strawberryflavor/NasCredentials";
}

bool Load(QString *error) {
  Install();
  QJniEnvironment env;
  const QJniObject result = QJniObject::callStaticObjectMethod(vault, "load", "(Landroid/content/Context;)Ljava/lang/String;", QNativeInterface::QAndroidApplication::context().object());
  if (env.checkAndClearExceptions() || !result.isValid()) {
    *error = QObject::tr("Could not unlock stored NAS credentials");
    qLog(Error) << *error;
    return false;
  }
  QJsonParseError parse;
  const QJsonDocument document = QJsonDocument::fromJson(result.toString().toUtf8(), &parse);
  if (parse.error != QJsonParseError::NoError || !document.isArray()) {
    *error = QObject::tr("Stored NAS locations are invalid");
    qLog(Error) << *error;
    return false;
  }
  QList<Location> value;
  for (const QJsonValue &item : document.array()) {
    const QJsonObject o = item.toObject();
    Location location{o["id"_L1].toString(), o["server"_L1].toString(), o["share"_L1].toString(), o["folder"_L1].toString(), o["user"_L1].toString(), o["password"_L1].toString(), o["domain"_L1].toString()};
    if (!Validate(location).isEmpty()) {
      *error = QObject::tr("Stored NAS location is invalid");
      qLog(Error) << *error;
      return false;
    }
    value.append(location);
  }
  SetLocations(value);
  return true;
}

bool Save(const QList<Location> &value, QString *error) {
  QJsonArray array;
  for (const Location &location : value) {
    const QString invalid = Validate(location);
    if (!invalid.isEmpty()) { *error = invalid; return false; }
    array.append(QJsonObject{{u"id"_s, location.id}, {u"server"_s, location.server}, {u"share"_s, location.share}, {u"folder"_s, location.folder}, {u"user"_s, location.user}, {u"password"_s, location.password}, {u"domain"_s, location.domain}});
  }
  QJniEnvironment env;
  const jboolean saved = QJniObject::callStaticMethod<jboolean>(vault, "save", "(Landroid/content/Context;Ljava/lang/String;)Z", QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact))).object<jstring>());
  if (env.checkAndClearExceptions() || !saved) {
    *error = QObject::tr("Could not securely save NAS credentials. Collection locations were not changed.");
    qLog(Error) << *error;
    return false;
  }
  SetLocations(value);
  return true;
}
#endif

}  // namespace Nas
