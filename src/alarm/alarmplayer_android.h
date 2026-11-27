// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMPLAYER_ANDROID_H
#define ALARMPLAYER_ANDROID_H

#include <jni.h>

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "engine/enginebase.h"
#include "alarmplayback_android.h"

class QTimer;
class Player;
class PlaylistManager;
class CollectionBackend;
class PlaylistBackend;

// Plays an alarm through Strawberry's own player when AlarmService.java asks for it, so songs on the NAS, streams and the current songs of the alarm's source play without copies.
// AlarmService.java stays in charge: it decides who plays, plays the local copies or the default sound itself when this player can't, and handles Snooze, Stop and the automatic stop.
// See AlarmHandover.java for the handover and alarmplayback_android.h for the session.
class AlarmPlayer : public QObject {
  Q_OBJECT

 public:
  explicit AlarmPlayer(SharedPtr<Player> player, SharedPtr<PlaylistManager> playlist_manager, SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, QObject *parent = nullptr);
  ~AlarmPlayer() override;

  // Tells AlarmService.java that alarms can be played here, once the playlists are loaded.
  void SetReady();

 private:
  struct Items {
    SongList songs;
    QList<QUrl> urls;
  };

  static void NativeStartAlarm(JNIEnv *env, jclass, jstring token, jstring alarm, jlong fade_start_msec, jlong fade_msec);
  static void NativeStopAlarm(JNIEnv *env, jclass, jstring token);

  void StartAlarm(const QString &token, const QByteArray &json, const qint64 fade_start_msec, const qint64 fade_msec);
  void StopAlarm(const QString &token);
  void ItemsFound(const QString &token, const Items &items);
  void EngineStateChanged(const EngineBase::State state);
  void PlaylistFinished();
  void Step();
  void HandleAction(const AlarmPlaybackSession::Action action);
  void Fail(const QString &reason);
  // Stops the alarm's playback and restores the player volume.
  void Release();

  static QPointer<AlarmPlayer> instance_;

  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<CollectionBackend> collection_backend_;
  const SharedPtr<PlaylistBackend> playlist_backend_;
  QTimer *timer_;
  AlarmPlaybackSession session_;
  qint64 fade_start_msec_ = 0;
  qint64 fade_msec_ = 0;
  // Whether the player plays the alarm, so Release() must stop it and restore its volume.
  bool playing_ = false;
  uint previous_volume_ = 100;
  bool previous_muted_ = false;
};

#endif  // ALARMPLAYER_ANDROID_H
