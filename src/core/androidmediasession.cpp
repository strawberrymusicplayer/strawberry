/*
 * Strawberry Music Player
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <jni.h>

#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>
#include <QJniEnvironment>
#include <QJniObject>
#include <QString>
#include <QUrl>
#include <QtCore/qcoreapplication_platform.h>

#include "constants/timeconstants.h"
#include "core/logging.h"
#include "core/player.h"
#include "core/song.h"
#include "playlist/playlistmanager.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"

#include "androidmediasession.h"

namespace {

constexpr char kBridgeClass[] = "nl/dewitweb/strawberryflavor/MediaSessionBridge";

// Must match the STATE_ constants in MediaSessionBridge.java.
constexpr jint kStateStopped = 0;
constexpr jint kStatePlaying = 1;
constexpr jint kStatePaused = 2;

// Calls a static void method of MediaSessionBridge; QJniObject would only print a failure as a generic warning.
template<typename... Args>
void CallBridge(const char *method, const char *signature, Args... args) {

  QJniEnvironment env;
  jclass bridge_class = env.findClass(kBridgeClass);
  jmethodID method_id = bridge_class ? env.findStaticMethod(bridge_class, method, signature) : nullptr;
  if (method_id) {
    env->CallStaticVoidMethod(bridge_class, method_id, args...);
  }
  if (!method_id || env.checkAndClearExceptions()) {
    qLog(Error) << "Could not call MediaSessionBridge." << method;
  }

}

QString CoverPath(const Song &song, const AlbumCoverLoaderResult &result) {

  // The current album cover loader saves every loaded cover, including embedded covers, as a temporary JPEG file.
  for (const QUrl &url : {result.temp_cover_url, result.album_cover.cover_url, song.art_manual(), song.art_automatic()}) {
    if (url.isValid() && url.isLocalFile() && QFile::exists(url.toLocalFile())) {
      return url.toLocalFile();
    }
  }
  return QString();

}

}  // namespace

QPointer<AndroidMediaSession> AndroidMediaSession::instance_;

AndroidMediaSession::AndroidMediaSession(const SharedPtr<Player> player,
                                         const SharedPtr<PlaylistManager> playlist_manager,
                                         const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                         QObject *parent)
    : QObject(parent),
      player_(player) {

  instance_ = this;
  const JNINativeMethod native_methods[] = {
    {const_cast<char *>("nativePlay"), const_cast<char *>("()V"), reinterpret_cast<void *>(NativePlay)},
    {const_cast<char *>("nativePause"), const_cast<char *>("()V"), reinterpret_cast<void *>(NativePause)},
    {const_cast<char *>("nativeNext"), const_cast<char *>("()V"), reinterpret_cast<void *>(NativeNext)},
    {const_cast<char *>("nativePrevious"), const_cast<char *>("()V"), reinterpret_cast<void *>(NativePrevious)},
    {const_cast<char *>("nativeSeekTo"), const_cast<char *>("(J)V"), reinterpret_cast<void *>(NativeSeekTo)},
    {const_cast<char *>("nativeStop"), const_cast<char *>("()V"), reinterpret_cast<void *>(NativeStop)},
  };
  QJniEnvironment env;
  if (!env.registerNativeMethods(kBridgeClass, native_methods, static_cast<int>(sizeof(native_methods) / sizeof(native_methods[0])))) {
    qLog(Error) << "Could not register MediaSessionBridge native methods";
  }

  CallBridge("init", "(Landroid/content/Context;Ljava/lang/String;)V", QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(tr("Playback")).object<jstring>());

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &AndroidMediaSession::EngineStateChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &AndroidMediaSession::Seeked);
  QObject::connect(&*playlist_manager, &PlaylistManager::CurrentSongChanged, this, &AndroidMediaSession::CurrentSongChanged);
  QObject::connect(&*current_albumcover_loader, &CurrentAlbumCoverLoader::AlbumCoverLoaded, this, &AndroidMediaSession::AlbumCoverLoaded);

}

void AndroidMediaSession::QueuePlayerCommand(PlayerCommand command) {

  QCoreApplication *application = QCoreApplication::instance();
  if (!application || !QMetaObject::invokeMethod(application, [command = std::move(command)]() {
        if (instance_) command(instance_->player_.get());
      }, Qt::QueuedConnection)) {
    qLog(Error) << "Could not queue Android media session command";
  }

}

void AndroidMediaSession::NativePlay(JNIEnv *, jclass) {
  QueuePlayerCommand([](Player *player) { player->Play(); });
}

void AndroidMediaSession::NativePause(JNIEnv *, jclass) {
  QueuePlayerCommand([](Player *player) { player->Pause(); });
}

void AndroidMediaSession::NativeNext(JNIEnv *, jclass) {
  QueuePlayerCommand([](Player *player) { player->Next(); });
}

void AndroidMediaSession::NativePrevious(JNIEnv *, jclass) {
  QueuePlayerCommand([](Player *player) { player->Previous(); });
}

void AndroidMediaSession::NativeSeekTo(JNIEnv *, jclass, const jlong position_msec) {
  if (position_msec < 0) return;
  QueuePlayerCommand([position_msec](Player *player) { player->SeekToMsec(static_cast<quint64>(position_msec)); });
}

void AndroidMediaSession::NativeStop(JNIEnv *, jclass) {
  QueuePlayerCommand([](Player *player) { player->Stop(); });
}

void AndroidMediaSession::EngineStateChanged(const EngineBase::State state) {

  UpdatePlaybackState(state);

}

void AndroidMediaSession::Seeked() {

  UpdatePlaybackState(player_->GetState());

}

// The metadata is published as soon as the song changes, and again with the cover when it has been loaded.
void AndroidMediaSession::CurrentSongChanged(const Song &song) {

  AlbumCoverLoaded(song, AlbumCoverLoaderResult());
  UpdatePlaybackState(player_->GetState());

}

void AndroidMediaSession::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  const QString artist = song.artist().isEmpty() ? song.albumartist() : song.artist();
  const jlong duration_msec = song.length_nanosec() > 0 ? song.length_nanosec() / kNsecPerMsec : 0;

  CallBridge("setMetadata", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JLjava/lang/String;)V",
             QJniObject::fromString(song.PrettyTitle()).object<jstring>(),
             QJniObject::fromString(artist).object<jstring>(),
             QJniObject::fromString(song.album()).object<jstring>(),
             duration_msec,
             QJniObject::fromString(CoverPath(song, result)).object<jstring>());

}

void AndroidMediaSession::UpdatePlaybackState(const EngineBase::State state) {

  jint bridge_state = kStateStopped;
  switch (state) {
    case EngineBase::State::Playing:
      bridge_state = kStatePlaying;
      break;
    case EngineBase::State::Paused:
      bridge_state = kStatePaused;
      break;
    case EngineBase::State::Empty:
    case EngineBase::State::Idle:
    case EngineBase::State::Error:
      bridge_state = kStateStopped;
      break;
  }

  const jlong position_msec = bridge_state == kStateStopped ? 0 : player_->engine()->position_nanosec() / kNsecPerMsec;
  CallBridge("setPlaybackState", "(IJ)V", bridge_state, position_msec);

}
