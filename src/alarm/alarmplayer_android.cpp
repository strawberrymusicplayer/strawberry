// SPDX-License-Identifier: GPL-3.0-or-later

#include <jni.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFutureWatcher>
#include <QJniEnvironment>
#include <QJniObject>
#include <QJsonDocument>
#include <QList>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtConcurrentRun>

#include "core/logging.h"
#include "core/player.h"
#include "core/song.h"
#ifdef HAVE_NAS
#  include "core/nasfileengine.h"
#endif
#include "collection/collectionbackend.h"
#include "engine/gstenginepipeline.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistmanager.h"
#include "alarmclock_android.h"
#include "alarmplayer_android.h"
#include "alarmschedule_android.h"
#include "alarmtracks_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kBridgeClass[] = "nl/dewitweb/strawberryflavor/AlarmBridge";
// Often enough for an even fade-in and to notice soon that the player plays.
constexpr int kStepMsec = 200;

void CallBridge(const char *method, const char *signature, const QString &token, const QString &reason = QString()) {

  const QJniObject java_token = QJniObject::fromString(token);
  if (reason.isNull()) {
    QJniObject::callStaticMethod<void>(kBridgeClass, method, signature, java_token.object<jstring>());
  }
  else {
    QJniObject::callStaticMethod<void>(kBridgeClass, method, signature, java_token.object<jstring>(), QJniObject::fromString(reason).object<jstring>());
  }
  QJniEnvironment env;
  if (env.checkAndClearExceptions()) qLog(Error) << "Could not call AlarmBridge." << method;

}

QString FromJava(JNIEnv *env, jstring text) {

  if (!text) return QString();
  const char *chars = env->GetStringUTFChars(text, nullptr);
  const QString result = QString::fromUtf8(chars);
  env->ReleaseStringUTFChars(text, chars);
  return result;

}

bool IsNasPath(const QString &file) {

#ifdef HAVE_NAS
  return Nas::IsPath(file);
#else
  Q_UNUSED(file)
  return false;
#endif

}

quint64 NasFailureSerial() {

#ifdef HAVE_NAS
  return Nas::FailureSerial();
#else
  return 0;
#endif

}

}  // namespace

QPointer<AlarmPlayer> AlarmPlayer::instance_;

AlarmPlayer::AlarmPlayer(SharedPtr<Player> player, SharedPtr<PlaylistManager> playlist_manager, SharedPtr<CollectionBackend> collection_backend, SharedPtr<PlaylistBackend> playlist_backend, QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      collection_backend_(collection_backend),
      playlist_backend_(playlist_backend),
      timer_(new QTimer(this)) {

  instance_ = this;
  timer_->setInterval(kStepMsec);
  QObject::connect(timer_, &QTimer::timeout, this, &AlarmPlayer::Step);
  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &AlarmPlayer::EngineStateChanged);
  QObject::connect(&*player_, &Player::PlaylistFinished, this, &AlarmPlayer::PlaylistFinished);

  const JNINativeMethod native_methods[] = {
    {const_cast<char *>("nativeStartAlarm"), const_cast<char *>("(Ljava/lang/String;Ljava/lang/String;JJ)V"), reinterpret_cast<void *>(NativeStartAlarm)},
    {const_cast<char *>("nativeStopAlarm"), const_cast<char *>("(Ljava/lang/String;)V"), reinterpret_cast<void *>(NativeStopAlarm)},
  };
  QJniEnvironment env;
  if (!env.registerNativeMethods(kBridgeClass, native_methods, static_cast<int>(sizeof(native_methods) / sizeof(native_methods[0])))) {
    qLog(Error) << "Could not register AlarmBridge native methods";
  }

}

AlarmPlayer::~AlarmPlayer() {

  QJniObject::callStaticMethod<void>(kBridgeClass, "setPlayerReady", "(Z)V", jboolean(false));
  QJniEnvironment env;
  env.checkAndClearExceptions();
  // AlarmService.java plays the alarm itself when the app closes while it plays here.
  if (session_.phase() != AlarmPlaybackSession::Phase::Idle) Fail(u"the app closed"_s);

}

void AlarmPlayer::SetReady() {

  QJniObject::callStaticMethod<void>(kBridgeClass, "setPlayerReady", "(Z)V", jboolean(true));
  QJniEnvironment env;
  if (env.checkAndClearExceptions()) qLog(Error) << "Could not call AlarmBridge.setPlayerReady";

}

void AlarmPlayer::NativeStartAlarm(JNIEnv *env, jclass, jstring token, jstring alarm, jlong fade_start_msec, jlong fade_msec) {

  const QString token_string = FromJava(env, token);
  const QByteArray json = FromJava(env, alarm).toUtf8();
  QCoreApplication *application = QCoreApplication::instance();
  if (application && QMetaObject::invokeMethod(application, [token_string, json, fade_start_msec, fade_msec]() {
        if (instance_) instance_->StartAlarm(token_string, json, fade_start_msec, fade_msec);
        else CallBridge("alarmFailed", "(Ljava/lang/String;Ljava/lang/String;)V", token_string, u"the player is gone"_s);
      }, Qt::QueuedConnection)) {
    return;
  }
  CallBridge("alarmFailed", "(Ljava/lang/String;Ljava/lang/String;)V", token_string, u"the app is closing"_s);

}

void AlarmPlayer::NativeStopAlarm(JNIEnv *env, jclass, jstring token) {

  const QString token_string = FromJava(env, token);
  QCoreApplication *application = QCoreApplication::instance();
  if (!application) return;
  QMetaObject::invokeMethod(application, [token_string]() {
    if (instance_) instance_->StopAlarm(token_string);
  }, Qt::QueuedConnection);

}

void AlarmPlayer::StartAlarm(const QString &token, const QByteArray &json, const qint64 fade_start_msec, const qint64 fade_msec) {

  // AlarmService.java already stopped a previous alarm, but its request may arrive later.
  if (session_.Start(token)) Release();
  fade_start_msec_ = fade_start_msec;
  fade_msec_ = fade_msec;

  const Alarm alarm = Alarm::FromJson(QJsonDocument::fromJson(json).object());
  if (alarm.source == Alarm::Source::Sound) {
    Fail(u"the alarm plays the default sound"_s);
    return;
  }

  const SharedPtr<CollectionBackend> collection_backend = collection_backend_;
  const SharedPtr<PlaylistBackend> playlist_backend = playlist_backend_;
  // Reading the database and checking that the NAS can be reached may take a while, so it is done on a worker thread.
  QFutureWatcher<Items> *watcher = new QFutureWatcher<Items>(this);
  QObject::connect(watcher, &QFutureWatcher<Items>::finished, this, [this, watcher, token]() {
    watcher->deleteLater();
    ItemsFound(token, watcher->result());
  });
  watcher->setFuture(QtConcurrent::run([alarm, collection_backend, playlist_backend]() {
    Items items;
    QStringList files;
    if (alarm.source == Alarm::Source::Song || alarm.source == Alarm::Source::Radio) {
      const QUrl url(alarm.source_url);
      if (!url.isValid() || url.isEmpty()) return items;
      items.urls << url;
      files << AlarmTracks::CopySource(url);
    }
    else {
      const SongList songs = AlarmClock::Songs(alarm, collection_backend, playlist_backend, AlarmPlayback::kMaximumSongs);
      for (const Song &song : songs) files << AlarmTracks::CopySource(song.url());
      const QList<int> reachable = AlarmPlayback::ReachableFiles(files, AlarmPlayback::kMaximumSongs, AlarmPlayback::kCheckedSongs, [](const QString &file) { return QFile::exists(file); }, IsNasPath, NasFailureSerial);
      for (const int i : reachable) items.songs << songs[i];
      return items;
    }
    if (!AlarmPlayback::ReachableFiles(files, 1, 1, [](const QString &file) { return QFile::exists(file); }, IsNasPath, NasFailureSerial).isEmpty()) return items;
    items.urls.clear();
    return items;
  }));

}

void AlarmPlayer::ItemsFound(const QString &token, const Items &items) {

  if (!session_.SongsReady(token)) return;
  if (items.songs.isEmpty() && items.urls.isEmpty()) {
    Fail(u"no song can be reached"_s);
    return;
  }

  // Playback that was paused for the alarm makes way for it.
  player_->Stop(true);
  if (!playing_) {
    previous_volume_ = player_->GetVolume();
    previous_muted_ = player_->is_muted();
  }
  playing_ = true;
  // The alarm follows the alarm volume and sounds in silent mode, as with AlarmService.java.
  GstEnginePipeline::SetAndroidStreamType("alarm");
  player_->SetVolume(AlarmPlayback::FadeVolume(QDateTime::currentMSecsSinceEpoch(), fade_start_msec_, fade_msec_));

  playlist_manager_->SelectNowPlaying();
  Playlist *playlist = playlist_manager_->current();
  playlist->Clear();
  if (items.songs.isEmpty()) {
    playlist->InsertUrls(items.urls, -1, true);
  }
  else {
    playlist->InsertSongsOrCollectionItems(items.songs, QString(), -1, true);
  }
  timer_->start();

}

void AlarmPlayer::StopAlarm(const QString &token) {

  if (session_.Stop(token)) Release();

}

void AlarmPlayer::Step() {

  if (!playing_) {
    timer_->stop();
    return;
  }
  // The engine reports playing before a stream fails to load, so the alarm is claimed once the position moves.
  if (session_.phase() == AlarmPlaybackSession::Phase::Starting && player_->GetState() == EngineBase::State::Playing && player_->engine()->position_nanosec() > 0) {
    HandleAction(session_.HandleEvent(AlarmPlaybackSession::Event::Playing));
    if (!playing_) return;
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (player_->GetVolume() != AlarmPlayback::FadeVolume(now, fade_start_msec_, fade_msec_)) player_->SetVolume(AlarmPlayback::FadeVolume(now, fade_start_msec_, fade_msec_));
  if (session_.phase() == AlarmPlaybackSession::Phase::Playing && now - fade_start_msec_ >= fade_msec_) timer_->stop();

}

void AlarmPlayer::EngineStateChanged(const EngineBase::State state) {

  if (!playing_) return;
  switch (state) {
    case EngineBase::State::Paused:
      HandleAction(session_.HandleEvent(AlarmPlaybackSession::Event::Paused));
      break;
    case EngineBase::State::Empty:
    case EngineBase::State::Idle:
      HandleAction(session_.HandleEvent(AlarmPlaybackSession::Event::Stopped));
      break;
    case EngineBase::State::Playing:
    case EngineBase::State::Error:
      break;
  }

}

void AlarmPlayer::PlaylistFinished() {

  if (playing_) HandleAction(session_.HandleEvent(AlarmPlaybackSession::Event::Finished));

}

void AlarmPlayer::HandleAction(const AlarmPlaybackSession::Action action) {

  switch (action) {
    case AlarmPlaybackSession::Action::Claim:
      CallBridge("alarmClaimed", "(Ljava/lang/String;)V", session_.token());
      break;
    case AlarmPlaybackSession::Action::Fail:
      Fail(u"the player stopped"_s);
      break;
    case AlarmPlaybackSession::Action::None:
      break;
  }

}

void AlarmPlayer::Fail(const QString &reason) {

  const QString token = session_.token();
  session_.Reset();
  Release();
  qLog(Info) << "Not playing alarm" << token << "here:" << reason;
  CallBridge("alarmFailed", "(Ljava/lang/String;Ljava/lang/String;)V", token, reason);

}

void AlarmPlayer::Release() {

  timer_->stop();
  if (!playing_) return;
  playing_ = false;
  player_->Stop(true);
  GstEnginePipeline::SetAndroidStreamType(QByteArray());
  player_->SetVolume(previous_volume_);
  if (previous_muted_) player_->Mute();

}
