// SPDX-License-Identifier: GPL-3.0-or-later

#include "nasfileengine.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <poll.h>
#include <utility>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>
#include <private/qabstractfileengine_p.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>

#include "logging.h"

#ifdef Q_OS_ANDROID
#  include <QJniEnvironment>
#  include <QJniObject>
#  include <QJsonArray>
#  include <QJsonDocument>
#  include <QJsonObject>
#  include <QCoreApplication>
#  include <android/log.h>
#endif

using namespace Qt::Literals::StringLiterals;

namespace {

const QString prefix = QStringLiteral("/strawberry-nas/");
constexpr int kTimeoutSeconds = 8;
QReadWriteLock locations_lock;
QList<Nas::Location> locations;
quint64 locations_revision = 0;
thread_local quint64 failure_serial = 0;
struct DirectorySnapshot {
  quint64 revision;
  QHash<QString, smb2_stat_64> entries;
};
// Directory-listing metadata is valid only while its iterator is alive, not across scans.
thread_local QList<const DirectorySnapshot*> directory_snapshots;

struct ContextDeleter {
  void operator()(smb2_context *context) const { smb2_destroy_context(context); }
};

constexpr int kWatchSliceMs = 100;

// Waits up to slice_ms for the connection, also called without events so that libsmb2 expires timed-out requests.
bool ServiceWatch(smb2_context *context, const int slice_ms) {
  pollfd descriptor = {smb2_get_fd(context), static_cast<short>(smb2_which_events(context)), 0};
  if (poll(&descriptor, 1, slice_ms) < 0 && errno != EINTR) return false;
  return smb2_service(context, descriptor.revents) >= 0;
}

struct AsyncResult {
  bool finished = false;
  int status = 0;
  void *data = nullptr;
};

void AsyncFinished(smb2_context*, int status, void *command_data, void *private_data) {
  AsyncResult *result = static_cast<AsyncResult*>(private_data);
  result->finished = true;
  result->status = status;
  result->data = command_data;
}

// Services the connection until the request finished, returns false if *cancel became true, checked every 100 ms, or the connection failed.
// Like libsmb2's synchronous calls, connecting the socket has the timeout, the requests have their own.
bool WaitForResult(smb2_context *context, const AsyncResult &result, std::atomic_bool *cancel) {
  const QDeadlineTimer connect_deadline(kTimeoutSeconds * 1000);
  while (!result.finished) {
    if (cancel && *cancel) return false;
    if (!(smb2_which_events(context) & POLLIN) && connect_deadline.hasExpired()) {
      smb2_set_error(context, "Timeout expired and no connection exists");
      return false;
    }
    if (!ServiceWatch(context, kWatchSliceMs)) return false;
  }
  return true;
}

class Connection {
 public:
  explicit Connection(const Nas::Location &location, std::atomic_bool *cancel = nullptr) : context_(smb2_init_context()) {
    if (!context_) {
      Fail(QObject::tr("Could not allocate NAS connection"));
      return;
    }
    smb2_set_timeout(context_.get(), kTimeoutSeconds);
    smb2_set_version(context_.get(), SMB2_VERSION_ANY);
    smb2_set_security_mode(context_.get(), SMB2_NEGOTIATE_SIGNING_ENABLED | SMB2_NEGOTIATE_SIGNING_REQUIRED);
    smb2_set_sign(context_.get(), 1);
    smb2_set_user(context_.get(), location.user.toUtf8().constData());
    smb2_set_password(context_.get(), location.password.toUtf8().constData());
    smb2_set_domain(context_.get(), location.domain.toUtf8().constData());
    if (smb2_connect_share_async(context_.get(), location.server.toUtf8().constData(), location.share.toUtf8().constData(), location.user.toUtf8().constData(), AsyncFinished, &connected_) < 0 || !WaitForResult(context_.get(), connected_, cancel) || connected_.status < 0) {
      if (cancel && *cancel) {
        Fail(QObject::tr("Connecting to the NAS was cancelled"));
        return;
      }
      // libsmb2 connect errors hold NT status names, strerror text, the server and the share, never the password.
      const QString detail = QString::fromUtf8(smb2_get_error(context_.get()));
      qLog(Error) << "libsmb2:" << detail;
#ifdef Q_OS_ANDROID
      __android_log_write(ANDROID_LOG_ERROR, "StrawberryNas", detail.toUtf8().constData());
#endif
      Fail(Nas::ConnectionError(detail));
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
  // Declared before the context: destroying the context reports a still pending connect to the callback.
  AsyncResult connected_;
  std::unique_ptr<smb2_context, ContextDeleter> context_;
  QString error_;
};

std::optional<Nas::Location> Resolve(const QString &path, QString *remote, quint64 *revision = nullptr) {
  QReadLocker locker(&locations_lock);
  if (revision) *revision = locations_revision;
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
  NasIterator(const QString &path, const QStringList &entries, DirectorySnapshot metadata, QDir::Filters filters, const QStringList &names)
      : QAbstractFileEngineIterator(filters, names), path_(path), entries_(entries), metadata_(std::move(metadata)) {
    directory_snapshots.append(&metadata_);
  }
  ~NasIterator() override { directory_snapshots.removeOne(&metadata_); }
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
  DirectorySnapshot metadata_;
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
    location_ = Resolve(path_, &remote_, &location_revision_);
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
    DirectorySnapshot metadata{location_revision_, {}};
    if (Stat() && Connect()) {
      smb2dir *dir = smb2_opendir(connection_->get(), remote_.toUtf8().constData());
      if (!dir) Fail(QFile::ReadError, QStringLiteral("Could not list NAS folder. Existing collection entries will be retained."));
      else {
        while (const smb2dirent *entry = smb2_readdir(connection_->get(), dir)) {
          const QString name = QString::fromUtf8(entry->name);
          if (name != QStringLiteral(".") && name != QStringLiteral("..") && !name.contains(u'/') && !name.contains(u'\\') && entry->st.smb2_type != SMB2_TYPE_LINK) {
            entries.append(name);
            metadata.entries.insert(path_ + u'/' + name, entry->st);
          }
        }
        smb2_closedir(connection_->get(), dir);
      }
    }
    return new NasIterator(path_, entries, std::move(metadata), filters, names);
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
    for (auto it = directory_snapshots.crbegin(); it != directory_snapshots.crend(); ++it) {
      if ((*it)->revision != location_revision_) continue;
      const auto entry = (*it)->entries.constFind(path_);
      if (entry != (*it)->entries.constEnd()) {
        stat_ = entry.value();
        return true;
      }
    }
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
  quint64 location_revision_ = 0;
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

constexpr quint32 kStatusNotifyEnumDir = 0x0000010C;
constexpr qint64 kWatchEchoMs = 15000;

struct WatchState {
  bool opened = false;
  bool echo_pending = false;
  quint32 status = SMB2_STATUS_SUCCESS;
  std::optional<Nas::WatchResult> result;
  smb2_file_id file_id = {};
};

void WatchCreated(smb2_context*, int status, void *command_data, void *private_data) {
  WatchState *state = static_cast<WatchState*>(private_data);
  if (state->result) return;
  if (static_cast<quint32>(status) != SMB2_STATUS_SUCCESS) {
    // A missing or unreadable folder cannot be watched; the caller reconciles it like a lost connection.
    state->status = static_cast<quint32>(status);
    state->result = Nas::WatchResult::Disconnected;
    return;
  }
  std::memcpy(state->file_id, static_cast<smb2_create_reply*>(command_data)->file_id, SMB2_FD_SIZE);
  state->opened = true;
}

void WatchNotified(smb2_context*, int status, void*, void *private_data) {
  WatchState *state = static_cast<WatchState*>(private_data);
  if (state->result) return;
  state->status = static_cast<quint32>(status);
  state->result = Nas::NotifyResult(state->status);
}

void WatchEchoed(smb2_context*, int status, void*, void *private_data) {
  WatchState *state = static_cast<WatchState*>(private_data);
  state->echo_pending = false;
  if (status < 0 && !state->result) state->result = Nas::WatchResult::Disconnected;
}

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
QString ConnectionError(const QString &detail) {
  const auto has = [&detail](const QStringList &needles) {
    return std::any_of(needles.cbegin(), needles.cend(), [&detail](const QString &needle) { return detail.contains(needle, Qt::CaseInsensitive); });
  };
  if (has({u"LOGON_FAILURE"_s, u"ACCESS_DENIED"_s, u"WRONG_PASSWORD"_s, u"PASSWORD_EXPIRED"_s, u"ACCOUNT_"_s, u"Authentication failed"_s})) {
    return QObject::tr("Could not connect to NAS: wrong user name or password, or no access to the share.");
  }
  if (has({u"BAD_NETWORK_NAME"_s})) {
    return QObject::tr("Could not connect to NAS: the share does not exist.");
  }
  if (has({u"Can not resolve"_s, u"Invalid address"_s})) {
    return QObject::tr("Could not connect to NAS: the address could not be found.");
  }
  if (has({u"refused"_s, u"Timeout"_s, u"timed out"_s, u"unreachable"_s, u"Socket connect failed"_s, u"Connect failed with errno"_s, u"POLLHUP"_s})) {
    return QObject::tr("Could not connect to NAS: it cannot be reached. Check the network and that port 445 is open.");
  }
  if (has({u"Signing"_s, u"signature"_s, u"Negotiate"_s, u"NOT_SUPPORTED"_s})) {
    return QObject::tr("Could not connect to NAS: SMB signing or the SMB version is not supported. Enable SMB2 or later with signing on the NAS.");
  }
  return QObject::tr("Could not connect to NAS. Check the address, share, account, network and SMB signing support.");
}
void Install() { static const Handler handler; }
QList<Location> Locations() {
  QReadLocker locker(&locations_lock);
  return locations;
}
void SetLocations(const QList<Location> &value) {
  QWriteLocker locker(&locations_lock);
  locations = value;
  ++locations_revision;
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
QString Check(const Location &location, std::atomic_bool *cancel) {
  const QString invalid = Validate(location);
  if (!invalid.isEmpty()) return invalid;
  // Declared before the connection, which reports a still pending request to it when it's destroyed.
  AsyncResult opened;
  Connection connection(location, cancel);
  if (!connection.get()) return connection.error();
  if (smb2_opendir_async(connection.get(), location.folder.toUtf8().constData(), AsyncFinished, &opened) < 0 || !WaitForResult(connection.get(), opened, cancel) || opened.status < 0) {
    if (cancel && *cancel) return QObject::tr("Connecting to the NAS was cancelled");
    return QObject::tr("Could not open the NAS folder. Check the folder and read permissions.");
  }
  smb2_closedir(connection.get(), static_cast<smb2dir*>(opened.data));
  return {};
}
WatchResult NotifyResult(const quint32 status) {
  switch (status) {
    case SMB2_STATUS_SUCCESS: return WatchResult::Changed;
    case kStatusNotifyEnumDir: return WatchResult::Overflow;
    case SMB2_STATUS_NOT_SUPPORTED:
    case SMB2_STATUS_INVALID_DEVICE_REQUEST:
    case SMB2_STATUS_NOT_IMPLEMENTED:
    case SMB2_STATUS_INVALID_PARAMETER: return WatchResult::Unsupported;
    // Lost sessions, closed handles, request timeouts, shutdown and any other failure.
    default: return WatchResult::Disconnected;
  }
}
struct ChangeWatcher::Private {
  explicit Private(const Location &l) : location(l) {}
  Location location;
  // Declared before the connection: destroying the context reports still pending requests to the callbacks.
  WatchState state;
  std::unique_ptr<Connection> connection;
  bool notifying = false;
  QDeadlineTimer echo;
};
ChangeWatcher::ChangeWatcher(const Location &location) : d_(std::make_unique<Private>(location)) {}
ChangeWatcher::~ChangeWatcher() = default;
bool ChangeWatcher::IsOpen() const { return d_->connection && d_->state.opened; }
void ChangeWatcher::Close() {
  d_->connection.reset();
  d_->state = WatchState();
  d_->notifying = false;
}
bool ChangeWatcher::Open(std::atomic_bool *cancel) {
  const auto cancelled = [cancel]() { return cancel && cancel->load(); };
  Close();
  if (cancelled() || !Validate(d_->location).isEmpty()) return false;
  d_->connection = std::make_unique<Connection>(d_->location, cancel);
  smb2_context *context = d_->connection->get();
  if (cancelled() || !context) {
    Close();
    return false;
  }
  WatchState &state = d_->state;
  const QByteArray folder = d_->location.folder.toUtf8();
  smb2_create_request create = {};
  create.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
  create.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
  create.desired_access = SMB2_FILE_LIST_DIRECTORY | SMB2_FILE_READ_ATTRIBUTES;
  create.file_attributes = SMB2_FILE_ATTRIBUTE_DIRECTORY;
  create.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE | SMB2_FILE_SHARE_DELETE;
  create.create_disposition = SMB2_FILE_OPEN;
  create.create_options = SMB2_FILE_DIRECTORY_FILE;
  create.name = folder.constData();
  smb2_pdu *pdu = smb2_cmd_create_async(context, &create, WatchCreated, &state);
  if (!pdu) {
    Close();
    return false;
  }
  smb2_queue_pdu(context, pdu);
  // The create request keeps the 8 s timeout, libsmb2 reports its expiry to WatchCreated.
  while (!state.opened && !state.result) {
    if (cancelled()) {
      Close();
      return false;
    }
    if (!ServiceWatch(context, kWatchSliceMs)) state.result = WatchResult::Disconnected;
  }
  if (!state.opened) {
    qLog(Warning) << "NAS change notification folder could not be opened, status" << QStringLiteral("0x%1").arg(state.status, 8, 16, QLatin1Char('0'));
    Close();
    return false;
  }
  return true;
}
WatchResult ChangeWatcher::Wait(const int timeout_ms, std::atomic_bool *cancel) {
  const auto cancelled = [cancel]() { return cancel && cancel->load(); };
  if (cancelled()) return WatchResult::Cancelled;
  if (!IsOpen()) return WatchResult::Disconnected;
  const QDeadlineTimer deadline(std::max(0, timeout_ms));
  smb2_context *context = d_->connection->get();
  WatchState &state = d_->state;
  while (!state.result) {
    // A pending request is kept on Cancelled and Timeout, the next call continues waiting for it.
    if (cancelled()) return WatchResult::Cancelled;
    if (deadline.hasExpired()) return WatchResult::Timeout;
    if (!d_->notifying) {
      smb2_change_notify_request request = {};
      request.flags = SMB2_CHANGE_NOTIFY_WATCH_TREE;
      const uint32_t max_read = smb2_get_max_read_size(context);
      request.output_buffer_length = max_read == 0 ? 65536 : std::min<uint32_t>(65536, max_read);
      std::memcpy(request.file_id, state.file_id, SMB2_FD_SIZE);
      request.completion_filter = SMB2_CHANGE_NOTIIFY_FILE_NOTIFY_CHANGE_FILE_NAME | SMB2_CHANGE_NOTIIFY_FILE_NOTIFY_CHANGE_DIR_NAME | SMB2_CHANGE_NOTIIFY_FILE_NOTIFY_CHANGE_ATTRIBUTES | SMB2_CHANGE_NOTIIFY_FILE_NOTIFY_CHANGE_SIZE | SMB2_CHANGE_NOTIIFY_FILE_NOTIFY_CHANGE_LAST_WRITE | SMB2_CHANGE_NOTIIFY_FILE_NOTIFY_CHANGE_CREATION;
      // Only the notification may outlive the request timeout; periodic echoes keep the 8 s rule for detecting a lost connection.
      smb2_set_timeout(context, 0);
      smb2_pdu *pdu = smb2_cmd_change_notify_async(context, &request, WatchNotified, &state);
      smb2_set_timeout(context, kTimeoutSeconds);
      if (!pdu) {
        state.result = WatchResult::Disconnected;
        break;
      }
      smb2_queue_pdu(context, pdu);
      d_->notifying = true;
      d_->echo.setRemainingTime(kWatchEchoMs);
    }
    if (!state.echo_pending && d_->echo.hasExpired()) {
      if (smb2_echo_async(context, WatchEchoed, &state) < 0) {
        state.result = WatchResult::Disconnected;
        break;
      }
      state.echo_pending = true;
      d_->echo.setRemainingTime(kWatchEchoMs);
    }
    if (!ServiceWatch(context, static_cast<int>(std::min<qint64>(kWatchSliceMs, deadline.remainingTime())))) {
      state.result = WatchResult::Disconnected;
    }
  }
  const WatchResult result = *state.result;
  if (result != WatchResult::Changed) {
    qLog(Warning) << "NAS change notification ended with result" << static_cast<int>(result) << "and status" << QStringLiteral("0x%1").arg(state.status, 8, 16, QLatin1Char('0'));
  }
  // The handle stays open, the next call requests the following changes on it, which the server queues meanwhile.
  state.result.reset();
  state.status = SMB2_STATUS_SUCCESS;
  d_->notifying = false;
  if (result == WatchResult::Disconnected || result == WatchResult::Unsupported) Close();
  return result;
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
