/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>
#include <cmath>

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
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

#include "core/logging.h"

#include "mpris_common.h"
#include "mpris2.h"

#include "song.h"
#include "application.h"
#include "player.h"
#include "utilities/timeconstants.h"
#include "engine/enginebase.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"

#include "mpris2_player.h"
#include "mpris2_playlists.h"
#include "mpris2_root.h"
#include "mpris2_tracklist.h"

using namespace Qt::StringLiterals;

QDBusArgument &operator<<(QDBusArgument &arg, const MprisPlaylist &playlist) {
  arg.beginStructure();
  arg << playlist.id << playlist.name << playlist.icon;
  arg.endStructure();
  return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, MprisPlaylist &playlist) {
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

const QDBusArgument &operator>>(const QDBusArgument &arg, MaybePlaylist &playlist) {
  arg.beginStructure();
  arg >> playlist.valid >> playlist.playlist;
  arg.endStructure();
  return arg;
}

namespace mpris {

constexpr char kMprisObjectPath[] = "/org/mpris/MediaPlayer2";
constexpr char kServiceName[] = "org.mpris.MediaPlayer2.strawberry";
constexpr char kFreedesktopPath[] = "org.freedesktop.DBus.Properties";

Mpris2::Mpris2(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      app_name_(QCoreApplication::applicationName()) {

  new Mpris2Root(this);
  new Mpris2TrackList(this);
  new Mpris2Player(this);
  new Mpris2Playlists(this);

  if (!QDBusConnection::sessionBus().registerService(QLatin1String(kServiceName))) {
    qLog(Warning) << "Failed to register" << kServiceName << "on the session bus";
    return;
  }

  if (!QDBusConnection::sessionBus().registerObject(QLatin1String(kMprisObjectPath), this)) {
    qLog(Warning) << "Failed to register" << kMprisObjectPath << "on the session bus";
    return;
  }

  QObject::connect(&*app_->current_albumcover_loader(), &CurrentAlbumCoverLoader::AlbumCoverLoaded, this, &Mpris2::AlbumCoverLoaded);

  QObject::connect(&*app_->player()->engine(), &EngineBase::StateChanged, this, &Mpris2::EngineStateChanged);
  QObject::connect(&*app_->player(), &Player::VolumeChanged, this, &Mpris2::VolumeChanged);
  QObject::connect(&*app_->player(), &Player::Seeked, this, &Mpris2::Seeked);

  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::PlaylistManagerInitialized, this, &Mpris2::PlaylistManagerInitialized);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, this, &Mpris2::CurrentSongChanged);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::PlaylistChanged, this, &Mpris2::PlaylistChangedSlot);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentChanged, this, &Mpris2::PlaylistCollectionChanged);

  app_name_[0] = app_name_[0].toUpper();

  QStringList data_dirs = QString::fromUtf8(qgetenv("XDG_DATA_DIRS")).split(u':');

  if (!data_dirs.contains("/usr/local/share"_L1)) {
    data_dirs.append(QStringLiteral("/usr/local/share"));
  }

  if (!data_dirs.contains("/usr/share"_L1)) {
    data_dirs.append(QStringLiteral("/usr/share"));
  }

  for (const QString &data_dir : std::as_const(data_dirs)) {
    const QString desktopfilepath = QStringLiteral("%1/applications/%2.desktop").arg(data_dir, QGuiApplication::desktopFileName());
    if (QFile::exists(desktopfilepath)) {
      desktopfilepath_ = desktopfilepath;
      break;
    }
  }

  if (desktopfilepath_.isEmpty()) {
    desktopfilepath_ = QGuiApplication::desktopFileName() + QStringLiteral(".desktop");
  }

}

// when PlaylistManager gets it ready, we connect PlaylistSequence with this
void Mpris2::PlaylistManagerInitialized() {
  QObject::connect(app_->playlist_manager()->sequence(), &PlaylistSequence::ShuffleModeChanged, this, &Mpris2::ShuffleModeChanged);
  QObject::connect(app_->playlist_manager()->sequence(), &PlaylistSequence::RepeatModeChanged, this, &Mpris2::RepeatModeChanged);
}

void Mpris2::EngineStateChanged(EngineBase::State newState) {

  if (newState != EngineBase::State::Playing && newState != EngineBase::State::Paused) {
    last_metadata_ = QVariantMap();
    EmitNotification(QStringLiteral("Metadata"));
  }

  EmitNotification(QStringLiteral("CanPlay"));
  EmitNotification(QStringLiteral("CanPause"));
  EmitNotification(QStringLiteral("PlaybackStatus"), PlaybackStatus(newState));
  if (newState == EngineBase::State::Playing) EmitNotification(QStringLiteral("CanSeek"), CanSeek(newState));

}

void Mpris2::VolumeChanged() {
  EmitNotification(QStringLiteral("Volume"));
}

void Mpris2::ShuffleModeChanged() { EmitNotification(QStringLiteral("Shuffle")); }

void Mpris2::RepeatModeChanged() {

  EmitNotification(QStringLiteral("LoopStatus"));
  EmitNotification(QStringLiteral("CanGoNext"), CanGoNext());
  EmitNotification(QStringLiteral("CanGoPrevious"), CanGoPrevious());

}

void Mpris2::EmitNotification(const QString &name, const QVariant &value) {
  EmitNotification(name, value, QStringLiteral("org.mpris.MediaPlayer2.Player"));
}

void Mpris2::EmitNotification(const QString &name, const QVariant &value, const QString &mprisEntity) {

  QDBusMessage msg = QDBusMessage::createSignal(QLatin1String(kMprisObjectPath), QLatin1String(kFreedesktopPath), QStringLiteral("PropertiesChanged"));
  QVariantMap map;
  map.insert(name, value);
  QVariantList args = QVariantList() << mprisEntity << map << QStringList();
  msg.setArguments(args);
  QDBusConnection::sessionBus().send(msg);

}

void Mpris2::EmitNotification(const QString &name) {

  QVariant value;
  if (name == "PlaybackStatus"_L1) value = PlaybackStatus();
  else if (name == "LoopStatus"_L1) value = LoopStatus();
  else if (name == "Shuffle"_L1) value = Shuffle();
  else if (name == "Metadata"_L1) value = Metadata();
  else if (name == "Rating"_L1) value = Rating();
  else if (name == "Volume"_L1) value = Volume();
  else if (name == "Position"_L1) value = Position();
  else if (name == "CanPlay"_L1) value = CanPlay();
  else if (name == "CanPause"_L1) value = CanPause();
  else if (name == "CanSeek"_L1) value = CanSeek();
  else if (name == "CanGoNext"_L1) value = CanGoNext();
  else if (name == "CanGoPrevious"_L1) value = CanGoPrevious();

  if (value.isValid()) EmitNotification(name, value);

}

//------------------Root Interface--------------------------//

bool Mpris2::CanQuit() const { return true; }

bool Mpris2::CanRaise() const { return true; }

bool Mpris2::HasTrackList() const { return true; }

QString Mpris2::Identity() const { return app_name_; }

QString Mpris2::DesktopEntryAbsolutePath() const {

  return desktopfilepath_;

}

QString Mpris2::DesktopEntry() const { return QGuiApplication::desktopFileName(); }

QStringList Mpris2::SupportedUriSchemes() const {

  static QStringList res = QStringList() << QStringLiteral("file")
                                         << QStringLiteral("http")
                                         << QStringLiteral("cdda")
                                         << QStringLiteral("smb")
                                         << QStringLiteral("sftp");
  return res;

}

QStringList Mpris2::SupportedMimeTypes() const {

  static QStringList res = QStringList() << QStringLiteral("x-content/audio-player")
                                         << QStringLiteral("application/ogg")
                                         << QStringLiteral("application/x-ogg")
                                         << QStringLiteral("application/x-ogm-audio")
                                         << QStringLiteral("audio/flac")
                                         << QStringLiteral("audio/ogg")
                                         << QStringLiteral("audio/vorbis")
                                         << QStringLiteral("audio/aac")
                                         << QStringLiteral("audio/mp4")
                                         << QStringLiteral("audio/mpeg")
                                         << QStringLiteral("audio/mpegurl")
                                         << QStringLiteral("audio/vnd.rn-realaudio")
                                         << QStringLiteral("audio/x-flac")
                                         << QStringLiteral("audio/x-oggflac")
                                         << QStringLiteral("audio/x-vorbis")
                                         << QStringLiteral("audio/x-vorbis+ogg")
                                         << QStringLiteral("audio/x-speex")
                                         << QStringLiteral("audio/x-wav")
                                         << QStringLiteral("audio/x-wavpack")
                                         << QStringLiteral("audio/x-ape")
                                         << QStringLiteral("audio/x-mp3")
                                         << QStringLiteral("audio/x-mpeg")
                                         << QStringLiteral("audio/x-mpegurl")
                                         << QStringLiteral("audio/x-ms-wma")
                                         << QStringLiteral("audio/x-musepack")
                                         << QStringLiteral("audio/x-pn-realaudio")
                                         << QStringLiteral("audio/x-scpls")
                                         << QStringLiteral("video/x-ms-asf");

  return res;

}

void Mpris2::Raise() { Q_EMIT RaiseMainWindow(); }

void Mpris2::Quit() { QCoreApplication::quit(); }

QString Mpris2::PlaybackStatus() const {
  return PlaybackStatus(app_->player()->GetState());
}

QString Mpris2::PlaybackStatus(EngineBase::State state) const {

  switch (state) {
    case EngineBase::State::Playing: return QStringLiteral("Playing");
    case EngineBase::State::Paused: return QStringLiteral("Paused");
    default: return QStringLiteral("Stopped");
  }

}

QString Mpris2::LoopStatus() const {

  if (!app_->playlist_manager()->sequence()) {
    return QStringLiteral("None");
  }

  switch (app_->playlist_manager()->active() ? app_->playlist_manager()->active()->RepeatMode() : app_->playlist_manager()->sequence()->repeat_mode()) {
    case PlaylistSequence::RepeatMode::Album:
    case PlaylistSequence::RepeatMode::Playlist: return QStringLiteral("Playlist");
    case PlaylistSequence::RepeatMode::Track: return QStringLiteral("Track");
    default: return QStringLiteral("None");
  }

}

void Mpris2::SetLoopStatus(const QString &value) {

  PlaylistSequence::RepeatMode mode = PlaylistSequence::RepeatMode::Off;

  if (value == "None"_L1) {
    mode = PlaylistSequence::RepeatMode::Off;
  }
  else if (value == "Track"_L1) {
    mode = PlaylistSequence::RepeatMode::Track;
  }
  else if (value == "Playlist"_L1) {
    mode = PlaylistSequence::RepeatMode::Playlist;
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

  const PlaylistSequence::ShuffleMode shuffle_mode = app_->playlist_manager()->active() ? app_->playlist_manager()->active()->ShuffleMode() : app_->playlist_manager()->sequence()->shuffle_mode();
  return shuffle_mode != PlaylistSequence::ShuffleMode::Off;

}

void Mpris2::SetShuffle(bool enable) {
  app_->playlist_manager()->active()->sequence()->SetShuffleMode(enable ? PlaylistSequence::ShuffleMode::All : PlaylistSequence::ShuffleMode::Off);
}

QVariantMap Mpris2::Metadata() const { return last_metadata_; }

double Mpris2::Rating() const {
  float rating = app_->playlist_manager()->active()->current_item_metadata().rating();
  return (rating <= 0) ? 0 : rating;
}

void Mpris2::SetRating(double rating) {

  if (rating > 1.0) {
    rating = 1.0;
  }
  else if (rating <= 0.0) {
    rating = -1.0;
  }

  app_->playlist_manager()->RateCurrentSong(static_cast<float>(rating));

}

QDBusObjectPath Mpris2::current_track_id() const {
  return QDBusObjectPath(QStringLiteral("/org/strawberrymusicplayer/strawberry/Track/%1").arg(QString::number(app_->playlist_manager()->active()->current_row())));
}

// We send Metadata change notification as soon as the process of changing song starts...
void Mpris2::CurrentSongChanged(const Song &song) {

  AlbumCoverLoaded(song);
  EmitNotification(QStringLiteral("CanPlay"));
  EmitNotification(QStringLiteral("CanPause"));
  EmitNotification(QStringLiteral("CanGoNext"), CanGoNext());
  EmitNotification(QStringLiteral("CanGoPrevious"), CanGoPrevious());
  EmitNotification(QStringLiteral("CanSeek"), CanSeek());

}

// ... and we add the cover information later, when it's available.
void Mpris2::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  last_metadata_ = QVariantMap();
  song.ToXesam(&last_metadata_);

  using mpris::AddMetadata;
  AddMetadata(QStringLiteral("mpris:trackid"), current_track_id(), &last_metadata_);

  QUrl cover_url;
  if (result.album_cover.cover_url.isValid() && result.album_cover.cover_url.isLocalFile() && QFile(result.album_cover.cover_url.toLocalFile()).exists()) {
    cover_url = result.album_cover.cover_url;
  }
  else if (result.temp_cover_url.isValid() && result.temp_cover_url.isLocalFile()) {
    cover_url = result.temp_cover_url;
  }
  else if (song.art_manual().isValid() && song.art_manual().isLocalFile()) {
    cover_url = song.art_manual();
  }
  else if (song.art_automatic().isValid() && song.art_automatic().isLocalFile()) {
    cover_url = song.art_automatic();
  }

  if (cover_url.isValid()) {
    AddMetadata(QStringLiteral("mpris:artUrl"), cover_url.toString(), &last_metadata_);
  }

  AddMetadata(QStringLiteral("year"), song.year(), &last_metadata_);
  AddMetadata(QStringLiteral("bitrate"), song.bitrate(), &last_metadata_);

  EmitNotification(QStringLiteral("Metadata"), last_metadata_);

}

double Mpris2::Volume() const {
  return app_->player()->GetVolume() / 100.0;
}

void Mpris2::SetVolume(const double volume) {
  app_->player()->SetVolume(static_cast<uint>(qBound(0L, lround(volume * 100.0), 100L)));
}

qint64 Mpris2::Position() const {
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
  return (app_->player()->GetCurrentItem() && app_->player()->GetState() == EngineBase::State::Playing && !(app_->player()->GetCurrentItem()->options() & PlaylistItem::Option::PauseDisabled)) || PlaybackStatus() == "Paused"_L1 || PlaybackStatus() == "Stopped"_L1;
}

bool Mpris2::CanSeek() const { return CanSeek(app_->player()->GetState()); }

bool Mpris2::CanSeek(EngineBase::State state) const {
  return app_->player()->GetCurrentItem() && state != EngineBase::State::Empty && !app_->player()->GetCurrentItem()->Metadata().is_stream();
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
  if (CanPause() && app_->player()->GetState() != EngineBase::State::Paused) {
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

void Mpris2::Seek(qint64 offset) {

  if (CanSeek()) {
    app_->player()->SeekTo(app_->player()->engine()->position_nanosec() / kNsecPerSec + offset / kUsecPerSec);
  }

}

void Mpris2::SetPosition(const QDBusObjectPath &trackId, qint64 offset) {

  if (CanSeek() && trackId == current_track_id() && offset >= 0) {
    offset *= kNsecPerUsec;

    if (offset < app_->player()->GetCurrentItem()->Metadata().length_nanosec()) {
      app_->player()->SeekTo(offset / kNsecPerSec);
    }
  }

}

void Mpris2::OpenUri(const QString &uri) {
  app_->playlist_manager()->active()->InsertUrls(QList<QUrl>() << QUrl(uri), -1, true);
}

Track_Ids Mpris2::Tracks() const {
  // TODO
  return Track_Ids();
}

bool Mpris2::CanEditTracks() const { return false; }

TrackMetadata Mpris2::GetTracksMetadata(const Track_Ids &tracks) const {

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

QStringList Mpris2::Orderings() const { return QStringList() << QStringLiteral("User"); }

namespace {

QDBusObjectPath MakePlaylistPath(int id) {
  return QDBusObjectPath(QStringLiteral("/org/strawberrymusicplayer/strawberry/PlaylistId/%1").arg(id));
}

}  // namespace

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

  QStringList split_path = playlist_id.path().split(u'/');
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

  const QList<Playlist*> playlists = app_->playlist_manager()->GetAllPlaylists();
  MprisPlaylistList ret;
  ret.reserve(playlists.count());
  for (Playlist *p : playlists) {
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

void Mpris2::PlaylistChangedSlot(Playlist *playlist) {

  MprisPlaylist mpris_playlist;
  mpris_playlist.id = MakePlaylistPath(playlist->id());
  mpris_playlist.name = app_->playlist_manager()->GetPlaylistName(playlist->id());

  Q_EMIT PlaylistChanged(mpris_playlist);

}

void Mpris2::PlaylistCollectionChanged(Playlist *playlist) {
  Q_UNUSED(playlist);
  EmitNotification(QStringLiteral("PlaylistCount"), ""_L1, QStringLiteral("org.mpris.MediaPlayer2.Playlists"));
}

}  // namespace mpris
