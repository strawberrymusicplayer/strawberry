/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <stdlib.h>
#include <memory>
#include <algorithm>

#include <QApplication>
#include <QCoreApplication>
#include <QObject>
#include <QFile>
#include <QList>
#include <QJsonArray>
#include <QVariant>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QtDebug>

#include "core/logging.h"

#include "mpris_common.h"
#include "mpris2.h"

#include "timeconstants.h"
#include "song.h"
#include "application.h"
#include "player.h"
#include "engine/enginebase.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "covermanager/currentalbumcoverloader.h"

#include <core/mpris2_player.h>
#include <core/mpris2_playlists.h>
#include <core/mpris2_root.h>
#include <core/mpris2_tracklist.h>

using std::reverse;

QDBusArgument &operator<<(QDBusArgument &arg, const MprisPlaylist &playlist) {
  arg.beginStructure();
  arg << playlist.id << playlist.name << playlist.icon;
  arg.endStructure();
  return arg;
}

const QDBusArgument &operator>> (const QDBusArgument &arg, MprisPlaylist &playlist) {
  arg.beginStructure();
  arg >> playlist.id >> playlist.name >> playlist.icon;
  arg.endStructure();
  return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const MaybePlaylist &playlist) {
  arg.beginStructure();
  arg << playlist.valid;
  arg << playlist.playlist;
  arg.endStructure();
  return arg;
}

const QDBusArgument &operator>> (const QDBusArgument &arg, MaybePlaylist &playlist) {
  arg.beginStructure();
  arg >> playlist.valid >> playlist.playlist;
  arg.endStructure();
  return arg;
}

namespace mpris {

const char *Mpris2::kMprisObjectPath = "/org/mpris/MediaPlayer2";
const char *Mpris2::kServiceName = "org.mpris.MediaPlayer2.strawberry";
const char *Mpris2::kFreedesktopPath = "org.freedesktop.DBus.Properties";

Mpris2::Mpris2(Application *app, QObject *parent)
  : QObject(parent),
  app_(app),
  app_name_(QCoreApplication::applicationName())
  {

  new Mpris2Root(this);
  new Mpris2TrackList(this);
  new Mpris2Player(this);
  new Mpris2Playlists(this);

  if (!QDBusConnection::sessionBus().registerService(kServiceName)) {
    qLog(Warning) << "Failed to register" << QString(kServiceName) << "on the session bus";
    return;
  }

  if (!QDBusConnection::sessionBus().registerObject(kMprisObjectPath, this)) {
    qLog(Warning) << "Failed to register" << QString(kMprisObjectPath) << "on the session bus";
    return;
  }

  connect(app_->current_albumcover_loader(), SIGNAL(AlbumCoverLoaded(Song, QUrl, QImage)), SLOT(AlbumCoverLoaded(Song, QUrl, QImage)));

  connect(app_->player()->engine(), SIGNAL(StateChanged(Engine::State)), SLOT(EngineStateChanged(Engine::State)));
  connect(app_->player(), SIGNAL(VolumeChanged(int)), SLOT(VolumeChanged()));
  connect(app_->player(), SIGNAL(Seeked(qlonglong)), SIGNAL(Seeked(qlonglong)));

  connect(app_->playlist_manager(), SIGNAL(PlaylistManagerInitialized()), SLOT(PlaylistManagerInitialized()));
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), SLOT(CurrentSongChanged(Song)));
  connect(app_->playlist_manager(), SIGNAL(PlaylistChanged(Playlist*)), SLOT(PlaylistChanged(Playlist*)));
  connect(app_->playlist_manager(), SIGNAL(CurrentChanged(Playlist*)), SLOT(PlaylistCollectionChanged(Playlist*)));

  app_name_[0] = app_name_[0].toUpper();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
  if (!QGuiApplication::desktopFileName().isEmpty())
    desktop_files_ << QGuiApplication::desktopFileName();
#endif
  QStringList domain_split = QCoreApplication::organizationDomain().split(".");
  std::reverse(domain_split.begin(), domain_split.end());
  desktop_files_ << QStringList() << domain_split.join(".") + "." + QCoreApplication::applicationName().toLower();
  desktop_files_ << QCoreApplication::applicationName().toLower();
  desktop_file_ = desktop_files_.first();

  data_dirs_ = QString(getenv("XDG_DATA_DIRS")).split(":");
  data_dirs_.append("/usr/local/share");
  data_dirs_.append("/usr/share");

  for (const QString &directory : data_dirs_) {
    for (const QString &desktop_file : desktop_files_) {
      QString path = QString("%1/applications/%2.desktop").arg(directory, desktop_file);
      if (QFile::exists(path)) {
        desktop_file_ = desktop_file;
      }
    }
  }

}

// when PlaylistManager gets it ready, we connect PlaylistSequence with this
void Mpris2::PlaylistManagerInitialized() {
  connect(app_->playlist_manager()->sequence(), SIGNAL(ShuffleModeChanged(PlaylistSequence::ShuffleMode)), SLOT(ShuffleModeChanged()));
  connect(app_->playlist_manager()->sequence(), SIGNAL(RepeatModeChanged(PlaylistSequence::RepeatMode)), SLOT(RepeatModeChanged()));
}

void Mpris2::EngineStateChanged(Engine::State newState) {

  if (newState != Engine::Playing && newState != Engine::Paused) {
    last_metadata_ = QVariantMap();
    EmitNotification("Metadata");
  }

  EmitNotification("CanPlay");
  EmitNotification("CanPause");
  EmitNotification("PlaybackStatus", PlaybackStatus(newState));
  if (newState == Engine::Playing) EmitNotification("CanSeek", CanSeek(newState));

}

void Mpris2::VolumeChanged() { EmitNotification("Volume"); }

void Mpris2::ShuffleModeChanged() { EmitNotification("Shuffle"); }

void Mpris2::RepeatModeChanged() {

  EmitNotification("LoopStatus");
  EmitNotification("CanGoNext", CanGoNext());
  EmitNotification("CanGoPrevious", CanGoPrevious());

}

void Mpris2::EmitNotification(const QString &name, const QVariant &val) {
  EmitNotification(name, val, "org.mpris.MediaPlayer2.Player");
}

void Mpris2::EmitNotification(const QString &name, const QVariant &val, const QString &mprisEntity) {

  QDBusMessage msg = QDBusMessage::createSignal(kMprisObjectPath, kFreedesktopPath, "PropertiesChanged");
  QVariantMap map;
  map.insert(name, val);
  QVariantList args = QVariantList() << mprisEntity << map << QStringList();
  msg.setArguments(args);
  QDBusConnection::sessionBus().send(msg);

}

void Mpris2::EmitNotification(const QString &name) {

  QVariant value;
  if (name == "PlaybackStatus") value = PlaybackStatus();
  else if (name == "LoopStatus") value = LoopStatus();
  else if (name == "Shuffle") value = Shuffle();
  else if (name == "Metadata") value = Metadata();
  else if (name == "Volume") value = Volume();
  else if (name == "Position") value = Position();
  else if (name == "CanPlay") value = CanPlay();
  else if (name == "CanPause") value = CanPause();
  else if (name == "CanSeek") value = CanSeek();
  else if (name == "CanGoNext") value = CanGoNext();
  else if (name == "CanGoPrevious") value = CanGoPrevious();

  if (value.isValid()) EmitNotification(name, value);

}

//------------------Root Interface--------------------------//

bool Mpris2::CanQuit() const { return true; }

bool Mpris2::CanRaise() const { return true; }

bool Mpris2::HasTrackList() const { return true; }

QString Mpris2::Identity() const { return app_name_; }

QString Mpris2::DesktopEntryAbsolutePath() const {

  for (const QString &directory : data_dirs_) {
    for (const QString &desktop_file : desktop_files_) {
      QString path = QString("%1/applications/%2.desktop").arg(directory, desktop_file);
      if (QFile::exists(path)) {
        return path;
      }
    }
  }
  return QString();

}

QString Mpris2::DesktopEntry() const { return desktop_file_; }

QStringList Mpris2::SupportedUriSchemes() const {

  static QStringList res = QStringList() << "file"
                                         << "http"
                                         << "cdda"
                                         << "smb"
                                         << "sftp";
  return res;

}

QStringList Mpris2::SupportedMimeTypes() const {

  static QStringList res = QStringList() << "x-content/audio-player"
                                         << "application/ogg"
                                         << "application/x-ogg"
                                         << "application/x-ogm-audio"
                                         << "audio/flac"
                                         << "audio/ogg"
                                         << "audio/vorbis"
                                         << "audio/aac"
                                         << "audio/mp4"
                                         << "audio/mpeg"
                                         << "audio/mpegurl"
                                         << "audio/vnd.rn-realaudio"
                                         << "audio/x-flac"
                                         << "audio/x-oggflac"
                                         << "audio/x-vorbis"
                                         << "audio/x-vorbis+ogg"
                                         << "audio/x-speex"
                                         << "audio/x-wav"
                                         << "audio/x-wavpack"
                                         << "audio/x-ape"
                                         << "audio/x-mp3"
                                         << "audio/x-mpeg"
                                         << "audio/x-mpegurl"
                                         << "audio/x-ms-wma"
                                         << "audio/x-musepack"
                                         << "audio/x-pn-realaudio"
                                         << "audio/x-scpls"
                                         << "video/x-ms-asf";

  return res;

}

void Mpris2::Raise() { emit RaiseMainWindow(); }

void Mpris2::Quit() { qApp->quit(); }

QString Mpris2::PlaybackStatus() const {
  return PlaybackStatus(app_->player()->GetState());
}

QString Mpris2::PlaybackStatus(Engine::State state) const {

  switch (state) {
    case Engine::Playing: return "Playing";
    case Engine::Paused: return "Paused";
    default: return "Stopped";
  }

}

QString Mpris2::LoopStatus() const {

  if (!app_->playlist_manager()->sequence()) {
    return "None";
  }

  switch (app_->playlist_manager()->sequence()->repeat_mode()) {
    case PlaylistSequence::Repeat_Album:
    case PlaylistSequence::Repeat_Playlist: return "Playlist";
    case PlaylistSequence::Repeat_Track: return "Track";
    default: return "None";
  }

}

void Mpris2::SetLoopStatus(const QString &value) {

  PlaylistSequence::RepeatMode mode = PlaylistSequence::Repeat_Off;

  if (value == "None") {
    mode = PlaylistSequence::Repeat_Off;
  }
  else if (value == "Track") {
    mode = PlaylistSequence::Repeat_Track;
  }
  else if (value == "Playlist") {
    mode = PlaylistSequence::Repeat_Playlist;
  }

  app_->playlist_manager()->active()->sequence()->SetRepeatMode(mode);

}

double Mpris2::Rate() const { return 1.0; }

void Mpris2::SetRate(double rate) {

  if (rate == 0) {
    app_->player()->Pause();
  }

}

bool Mpris2::Shuffle() const {

  return app_->playlist_manager()->sequence()->shuffle_mode() != PlaylistSequence::Shuffle_Off;

}

void Mpris2::SetShuffle(bool enable) {
  app_->playlist_manager()->active()->sequence()->SetShuffleMode(enable ? PlaylistSequence::Shuffle_All : PlaylistSequence::Shuffle_Off);
}

QVariantMap Mpris2::Metadata() const { return last_metadata_; }

QString Mpris2::current_track_id() const {
  return QString("/org/strawbs/strawberry/Track/%1").arg(QString::number(app_->playlist_manager()->active()->current_row()));
}

// We send Metadata change notification as soon as the process of changing song starts...
void Mpris2::CurrentSongChanged(const Song &song) {

  AlbumCoverLoaded(song, QUrl(), QImage());
  EmitNotification("CanPlay");
  EmitNotification("CanPause");
  EmitNotification("CanGoNext", CanGoNext());
  EmitNotification("CanGoPrevious", CanGoPrevious());
  EmitNotification("CanSeek", CanSeek());

}

// ... and we add the cover information later, when it's available.
void Mpris2::AlbumCoverLoaded(const Song &song, const QUrl &cover_url, const QImage &image) {

  Q_UNUSED(image);

  last_metadata_ = QVariantMap();
  song.ToXesam(&last_metadata_);

  using mpris::AddMetadata;
  AddMetadata("mpris:trackid", current_track_id(), &last_metadata_);

  if (cover_url.isValid()) {
    AddMetadata("mpris:artUrl", cover_url.toString(), &last_metadata_);
  }

  AddMetadata("year", song.year(), &last_metadata_);
  AddMetadata("bitrate", song.bitrate(), &last_metadata_);

  EmitNotification("Metadata", last_metadata_);

}

double Mpris2::Volume() const { return app_->player()->GetVolume() / 100.0; }

void Mpris2::SetVolume(double value) { app_->player()->SetVolume(value * 100); }

qlonglong Mpris2::Position() const {
  return app_->player()->engine()->position_nanosec() / kNsecPerUsec;
}

double Mpris2::MaximumRate() const { return 1.0; }

double Mpris2::MinimumRate() const { return 1.0; }

bool Mpris2::CanGoNext() const {
  return app_->playlist_manager()->active() && app_->playlist_manager()->active()->next_row() != -1;
}

bool Mpris2::CanGoPrevious() const {
  return app_->playlist_manager()->active() && (app_->playlist_manager()->active()->previous_row() != -1 || app_->player()->PreviousWouldRestartTrack());
}

bool Mpris2::CanPlay() const {
  return app_->playlist_manager()->active() && app_->playlist_manager()->active()->rowCount() != 0;
}

// This one's a bit different than MPRIS 1 - we want this to be true even when the song is already paused or stopped.
bool Mpris2::CanPause() const {
  return (app_->player()->GetCurrentItem() && app_->player()->GetState() == Engine::Playing && !(app_->player()->GetCurrentItem()->options() & PlaylistItem::PauseDisabled)) || PlaybackStatus() == "Paused" || PlaybackStatus() == "Stopped";
}

bool Mpris2::CanSeek() const { return CanSeek(app_->player()->GetState()); }

bool Mpris2::CanSeek(Engine::State state) const {
  return app_->player()->GetCurrentItem() && state != Engine::Empty && !app_->player()->GetCurrentItem()->Metadata().is_stream();
}

bool Mpris2::CanControl() const { return true; }

void Mpris2::Next() {
  if (CanGoNext()) {
    app_->player()->Next();
  }
}

void Mpris2::Previous() {
  if (CanGoPrevious()) {
    app_->player()->Previous();
  }
}

void Mpris2::Pause() {
  if (CanPause() && app_->player()->GetState() != Engine::Paused) {
    app_->player()->Pause();
  }
}

void Mpris2::PlayPause() {
  if (CanPause()) {
    app_->player()->PlayPause();
  }
}

void Mpris2::Stop() { app_->player()->Stop(); }

void Mpris2::Play() {
  if (CanPlay()) {
    app_->player()->Play();
  }
}

void Mpris2::Seek(qlonglong offset) {
  if (CanSeek()) {
    app_->player()->SeekTo(app_->player()->engine()->position_nanosec() / kNsecPerSec + offset / kUsecPerSec);
  }
}

void Mpris2::SetPosition(const QDBusObjectPath &trackId, qlonglong offset) {
  if (CanSeek() && trackId.path() == current_track_id() && offset >= 0) {
    offset *= kNsecPerUsec;

    if(offset < app_->player()->GetCurrentItem()->Metadata().length_nanosec()) {
      app_->player()->SeekTo(offset / kNsecPerSec);
    }
  }
}

void Mpris2::OpenUri(const QString &uri) {
  app_->playlist_manager()->active()->InsertUrls(QList<QUrl>() << QUrl(uri), -1, true);
}

TrackIds Mpris2::Tracks() const {
  // TODO
  return TrackIds();
}

bool Mpris2::CanEditTracks() const { return false; }

TrackMetadata Mpris2::GetTracksMetadata(const TrackIds &tracks) const {

  Q_UNUSED(tracks);

  // TODO
  return TrackMetadata();

}

void Mpris2::AddTrack(const QString &uri, const QDBusObjectPath &afterTrack, bool setAsCurrent) {

  Q_UNUSED(uri);
  Q_UNUSED(afterTrack);
  Q_UNUSED(setAsCurrent);

  // TODO

}

void Mpris2::RemoveTrack(const QDBusObjectPath &trackId) {
  Q_UNUSED(trackId);
  // TODO
}

void Mpris2::GoTo(const QDBusObjectPath &trackId) {
  Q_UNUSED(trackId);
  // TODO
}

quint32 Mpris2::PlaylistCount() const {
  return app_->playlist_manager()->GetAllPlaylists().size();
}

QStringList Mpris2::Orderings() const { return QStringList() << "User"; }

namespace {

QDBusObjectPath MakePlaylistPath(int id) {
  return QDBusObjectPath(QString("/org/strawbs/strawberry/PlaylistId/%1").arg(id));
}

}

MaybePlaylist Mpris2::ActivePlaylist() const {

  MaybePlaylist maybe_playlist;
  Playlist *current_playlist = app_->playlist_manager()->current();
  maybe_playlist.valid = current_playlist;
  if (!current_playlist) {
    return maybe_playlist;
  }

  maybe_playlist.playlist.id = MakePlaylistPath(current_playlist->id());
  maybe_playlist.playlist.name = app_->playlist_manager()->GetPlaylistName(current_playlist->id());
  return maybe_playlist;

}

void Mpris2::ActivatePlaylist(const QDBusObjectPath &playlist_id) {

  QStringList split_path = playlist_id.path().split('/');
  qLog(Debug) << Q_FUNC_INFO << playlist_id.path() << split_path;
  if (split_path.isEmpty()) {
    return;
  }
  bool ok = false;
  int p = split_path.last().toInt(&ok);
  if (!ok) {
    return;
  }
  if (!app_->playlist_manager()->IsPlaylistOpen(p)) {
    qLog(Error) << "Playlist isn't opened!";
    return;
  }
  app_->playlist_manager()->SetActivePlaylist(p);
  app_->player()->Next();

}

// TODO: Support sort orders.
MprisPlaylistList Mpris2::GetPlaylists(quint32 index, quint32 max_count, const QString &order, bool reverse_order) {

  Q_UNUSED(order);

  MprisPlaylistList ret;
  for (Playlist *p : app_->playlist_manager()->GetAllPlaylists()) {
    MprisPlaylist mpris_playlist;
    mpris_playlist.id = MakePlaylistPath(p->id());
    mpris_playlist.name = app_->playlist_manager()->GetPlaylistName(p->id());
    ret << mpris_playlist;
  }

  if (reverse_order) {
    std::reverse(ret.begin(), ret.end());
  }

  return ret.mid(index, max_count);

}

void Mpris2::PlaylistChanged(Playlist *playlist) {

  MprisPlaylist mpris_playlist;
  mpris_playlist.id = MakePlaylistPath(playlist->id());
  mpris_playlist.name = app_->playlist_manager()->GetPlaylistName(playlist->id());
  emit PlaylistChanged(mpris_playlist);

}

void Mpris2::PlaylistCollectionChanged(Playlist *playlist) {
  Q_UNUSED(playlist);
  EmitNotification("PlaylistCount", "", "org.mpris.MediaPlayer2.Playlists");
}

}  // namespace mpris
