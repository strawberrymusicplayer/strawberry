// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NASFILEENGINE_H
#define NASFILEENGINE_H

#include <atomic>
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
QString Check(const Location &location);
// Blocks until the server reports a change anywhere below the location's folder, timeout_ms elapses or *cancel becomes true.
// Each call uses its own connection, so Overflow and Disconnected mean changes may have been missed and the folder needs a full reconcile.
// Cancellation is checked every 100 ms while waiting; connection setup keeps the 8 s timeout.
WatchResult WaitForChange(const Location &location, int timeout_ms, std::atomic_bool *cancel = nullptr);
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
