// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMTRACKS_ANDROID_H
#define ALARMTRACKS_ANDROID_H

#include <QList>
#include <QString>
#include <QUrl>
#include <QtGlobal>

// The user's choices for the songs an alarm rings with: local copies made for offline alarms, or the original songs.
struct AlarmTrackSettings {
  static constexpr int kDefaultTracksPerAlarm = 5;
  static constexpr int kDefaultMaximumTotalMb = 200;

  bool copy = true;
  int tracks_per_alarm = kDefaultTracksPerAlarm;
  // The maximum size of the copies of one alarm.
  int maximum_total_mb = kDefaultMaximumTotalMb;

  qint64 maximum_total_bytes() const { return static_cast<qint64>(maximum_total_mb) * 1024 * 1024; }
  // Replaces values that are not one of the choices, for example from a hand-edited configuration, by the defaults.
  AlarmTrackSettings Normalized() const;
};

namespace AlarmTracks {

QList<int> TracksPerAlarmChoices();
QList<int> MaximumTotalMbChoices();

// Returns a file name QFile can copy from: a local path, which includes NAS paths, or a content:// URL; otherwise empty.
QString CopySource(const QUrl &url);
// Returns what AlarmService.java may try to play directly when copying is off: a local path, a content:// URL or an http(s) URL; otherwise empty.
QString OriginalSource(const QUrl &url);
// Whether a song of the size may be copied after count songs with a total size of total_size were copied.
bool CopyFits(const qint64 size, const int count, const qint64 total_size, const AlarmTrackSettings &settings);
// Returns the total size of the files in the directory and its subdirectories.
qint64 DirectorySize(const QString &directory);

}  // namespace AlarmTracks

#endif  // ALARMTRACKS_ANDROID_H
