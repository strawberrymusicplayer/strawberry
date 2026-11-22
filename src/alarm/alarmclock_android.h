// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMCLOCK_ANDROID_H
#define ALARMCLOCK_ANDROID_H

#include <functional>

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "alarmschedule_android.h"
#include "alarmtracks_android.h"

class CollectionBackend;
class PlaylistBackend;

// Keeps local copies of the songs each alarm plays, so AlarmService.java can play them without the Qt app, the network or the NAS.
// When copying is switched off, the alarms refer to the original songs instead, which play only when AlarmService.java can reach them.
class AlarmClock : public QObject {
  Q_OBJECT

 public:
  explicit AlarmClock(SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, QObject *parent = nullptr);

  // Passes the translated texts to the Java side, schedules the alarms and later refreshes outdated song copies.
  void Start();

  // Copies the songs of the alarm's current music source again in the background.
  void UpdateTracks(const QString &id);
  // Removes the song copies of a deleted alarm.
  void RemoveTracks(const QString &id);

  static AlarmTrackSettings TrackSettings();
  // Saves the settings; switching copying off deletes the copies, and switching it on or off updates the songs of the alarms.
  void SetTrackSettings(const AlarmTrackSettings &settings);
  // Returns the total size of the copies of all alarms in bytes.
  static qint64 CopiesSize();
  // Deletes the copies of all alarms; while copying is on, they are copied again by the next update, for example when the app starts.
  void DeleteCopies();

  SharedPtr<CollectionBackend> collection_backend() const { return collection_backend_; }
  SharedPtr<PlaylistBackend> playlist_backend() const { return playlist_backend_; }

  // Whether the alarm plays songs that are copied for it.
  static bool UsesTracks(const Alarm &alarm);

 Q_SIGNALS:
  void TracksUpdated(const QString &id);

 private:
  void UpdateOutdatedTracks();
  static QString TracksRoot();
  static QString TracksDirectory(const QString &id);
  // Calls interim from the copying thread with songs the alarm can play before all copies are made: the local originals, or else the copies so far.
  static QStringList CopyTracks(const Alarm &alarm, SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, const QString &directory, const QString &prefix, const AlarmTrackSettings &settings, const std::function<void(const QStringList &tracks)> &interim);
  // Lets an alarm without songs, or with interim songs, play the interim songs of the running update.
  void SaveInterimTracks(const Alarm &alarm, const int generation, const QStringList &tracks);
  // Removes the files in the directory for which remove returns true, and the directory once it is empty.
  static void RemoveCopies(const QString &directory, const std::function<bool(const QString &file)> &remove);

 private:
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<PlaylistBackend> playlist_backend_;
  // Increased for every update, so the result of an older, slower update is dropped.
  QHash<QString, int> generations_;
  // The alarms whose stored songs are interim songs of an update that is still copying.
  QSet<QString> interim_;
};

#endif  // ALARMCLOCK_ANDROID_H
