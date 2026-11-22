// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMCLOCK_ANDROID_H
#define ALARMCLOCK_ANDROID_H

#include <functional>

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "alarmschedule_android.h"

class CollectionBackend;
class PlaylistBackend;

// Keeps local copies of the songs each alarm plays, so AlarmService.java can ring them without the Qt app, the network or the NAS.
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

  SharedPtr<CollectionBackend> collection_backend() const { return collection_backend_; }
  SharedPtr<PlaylistBackend> playlist_backend() const { return playlist_backend_; }

  // Whether the alarm plays songs that are copied for it.
  static bool UsesTracks(const Alarm &alarm);

 Q_SIGNALS:
  void TracksUpdated(const QString &id);

 private:
  void UpdateOutdatedTracks();
  static QString TracksDirectory(const QString &id);
  static QStringList CopyTracks(const Alarm &alarm, SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, const QString &directory, const QString &prefix);
  // Removes the files in the directory for which remove returns true, and the directory once it is empty.
  static void RemoveCopies(const QString &directory, const std::function<bool(const QString &file)> &remove);

 private:
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<PlaylistBackend> playlist_backend_;
  // Increased for every update, so the result of an older, slower update is dropped.
  QHash<QString, int> generations_;
};

#endif  // ALARMCLOCK_ANDROID_H
