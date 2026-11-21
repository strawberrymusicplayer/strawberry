// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NASFILEENGINE_H
#define NASFILEENGINE_H

#include <atomic>
#include <memory>
#include <QList>
#include <QString>

namespace Nas {

enum class WatchResult { Changed, Timeout, Unsupported, Overflow, Disconnected, Cancelled };

struct Location {
  QString id;
  QString server;
  QString share;
  QString folder;
  QString user;
  QString password;
  QString domain;
  QString path() const;
  QString label() const;
};

void Install();
bool IsPath(const QString &path);
quint64 FailureSerial();
// Maps a libsmb2 connect error to a fixed user message that never contains the library text.
QString ConnectionError(const QString &detail);
QString Validate(const Location &location);
// Connects and opens the location's folder, returns an empty string on success.
// Connecting and opening stop within about 100 ms when *cancel becomes true; resolving a server name can't be cancelled.
QString Check(const Location &location, std::atomic_bool *cancel = nullptr);
// Watches a location's folder for changes anywhere below it, on its own connection.
// The folder handle and connection stay open between waits, so the server queues changes made while no request is pending and none are lost between two waits.
// Overflow and Disconnected mean changes may have been missed and the folder needs a full reconcile; after Disconnected and Unsupported the watcher is closed.
class ChangeWatcher {
 public:
  explicit ChangeWatcher(const Location &location);
  ~ChangeWatcher();
  // Connects and opens the folder, returns false if it can't be opened or *cancel becomes true, checked every 100 ms; connection setup keeps the 8 s timeout.
  // Changes made before the folder was opened are not reported.
  bool Open(std::atomic_bool *cancel = nullptr);
  bool IsOpen() const;
  // Blocks until the server reports a change, timeout_ms elapses or *cancel becomes true; cancellation is checked every 100 ms.
  // After Timeout and Cancelled the request stays pending, so the next call continues waiting for it.
  WatchResult Wait(int timeout_ms, std::atomic_bool *cancel = nullptr);

 private:
  Q_DISABLE_COPY_MOVE(ChangeWatcher)
  void Close();
  struct Private;
  std::unique_ptr<Private> d_;
};
// Maps a CHANGE_NOTIFY completion status (an NT status code) to a watch result.
WatchResult NotifyResult(quint32 status);
QString DisplayName(const QString &path);
void SetLocations(const QList<Location> &locations);
QList<Location> Locations();

#ifdef Q_OS_ANDROID
bool Load(QString *error);
bool Save(const QList<Location> &locations, QString *error);
#endif

}  // namespace Nas
#endif
