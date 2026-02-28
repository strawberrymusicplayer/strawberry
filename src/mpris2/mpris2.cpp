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

#include "constants/timeconstants.h"
#include "core/song.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#include "mpris2_player.h"
#include "mpris2_playlists.h"
#include "mpris2_root.h"
#include "mpris2_tracklist.h"

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

using namespace Qt::Literals::StringLiterals;

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
constexpr char kTrackPrefix[] = "/org/strawberrymusicplayer/strawberry/Track/";
constexpr char kNoTrack[] = "/org/mpris/MediaPlayer2/TrackList/NoTrack";

constexpr int kNbTracksDisplaid = 20;

Mpris2::Mpris2(const SharedPtr<Player> player,
               const SharedPtr<PlaylistManager> playlist_manager,
               const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
               QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      current_albumcover_loader_(current_albumcover_loader) {

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

  QObject::connect(&*current_albumcover_loader_, &CurrentAlbumCoverLoader::AlbumCoverLoaded, this, &Mpris2::AlbumCoverLoaded);

  QObject::connect(&*player_->engine(), &EngineBase::StateChanged, this, &Mpris2::EngineStateChanged);
  QObject::connect(&*player_, &Player::VolumeChanged, this, &Mpris2::VolumeChanged);
  QObject::connect(&*player_, &Player::Seeked, this, &Mpris2::Seeked);

  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistManagerInitialized, this, &Mpris2::PlaylistManagerInitialized);
  QObject::connect(&*playlist_manager_, &PlaylistManager::AllPlaylistsLoaded, this, &Mpris2::AllPlaylistsLoaded);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, &Mpris2::CurrentSongChanged);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistItemsAdded, this, &Mpris2::PlaylistItemsAdded);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistItemsRemoved, this, &Mpris2::PlaylistItemsRemoved);
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistChanged, this, &Mpris2::PlaylistChangedSlot);
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentChanged, this, &Mpris2::PlaylistCollectionChanged);

  QStringList data_dirs = QString::fromUtf8(qgetenv("XDG_DATA_DIRS")).split(u':');

  if (!data_dirs.contains("/usr/local/share"_L1)) {
    data_dirs.append(u"/usr/local/share"_s);
  }

  if (!data_dirs.contains("/usr/share"_L1)) {
    data_dirs.append(u"/usr/share"_s);
  }

  for (const QString &data_dir : std::as_const(data_dirs)) {
    const QString desktopfilepath = QStringLiteral("%1/applications/%2.desktop").arg(data_dir, QGuiApplication::desktopFileName());
    if (QFile::exists(desktopfilepath)) {
      desktopfilepath_ = desktopfilepath;
      break;
    }
  }

  if (desktopfilepath_.isEmpty()) {
    desktopfilepath_ = QGuiApplication::desktopFileName() + u".desktop"_s;
  }

}

// when PlaylistManager gets it ready, we connect PlaylistSequence with this
void Mpris2::PlaylistManagerInitialized() {

  QObject::connect(playlist_manager_->sequence(), &PlaylistSequence::ShuffleModeChanged, this, &Mpris2::ShuffleModeChanged);
  QObject::connect(playlist_manager_->sequence(), &PlaylistSequence::RepeatModeChanged, this, &Mpris2::RepeatModeChanged);

}

void Mpris2::AllPlaylistsLoaded() {

  qLog(Debug) << "MPRIS2: All playlists loaded, emitting MPRIS2 notifications";

  EmitNotification(u"CanPlay"_s);
  EmitNotification(u"CanPause"_s);
  EmitNotification(u"CanGoNext"_s, CanGoNext());
  EmitNotification(u"CanGoPrevious"_s, CanGoPrevious());
  EmitNotification(u"CanSeek"_s, CanSeek());

}

void Mpris2::EngineStateChanged(EngineBase::State newState) {

  if (newState != EngineBase::State::Playing && newState != EngineBase::State::Paused) {
    last_metadata_ = QVariantMap();
    EmitNotification(u"Metadata"_s);
  }

  EmitNotification(u"CanPlay"_s);
  EmitNotification(u"CanPause"_s);
  EmitNotification(u"PlaybackStatus"_s, PlaybackStatus(newState));
  if (newState == EngineBase::State::Playing) EmitNotification(u"CanSeek"_s, CanSeek(newState));

}

void Mpris2::VolumeChanged() {
  EmitNotification(u"Volume"_s);
}

void Mpris2::ShuffleModeChanged() { EmitNotification(u"Shuffle"_s); }

void Mpris2::RepeatModeChanged() {

  EmitNotification(u"LoopStatus"_s);
  EmitNotification(u"CanGoNext"_s, CanGoNext());
  EmitNotification(u"CanGoPrevious"_s, CanGoPrevious());

}

void Mpris2::EmitNotification(const QString &name, const QVariant &value) {
  EmitNotification(name, value, u"org.mpris.MediaPlayer2.Player"_s);
}

void Mpris2::EmitNotification(const QString &name, const QVariant &value, const QString &mprisEntity) {

  QDBusMessage msg = QDBusMessage::createSignal(QLatin1String(kMprisObjectPath), QLatin1String(kFreedesktopPath), u"PropertiesChanged"_s);
  QVariantMap map;
  map.insert(name, value);
  const QVariantList args = QVariantList() << mprisEntity << map << QStringList();
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

// ------------------Root Interface--------------------------//

bool Mpris2::CanQuit() const { return true; }

bool Mpris2::CanRaise() const { return true; }

bool Mpris2::HasTrackList() const { return true; }

QString Mpris2::Identity() const { return QCoreApplication::applicationName(); }

QString Mpris2::DesktopEntryAbsolutePath() const {

  return desktopfilepath_;

}

QString Mpris2::DesktopEntry() const { return QGuiApplication::desktopFileName(); }

QStringList Mpris2::SupportedUriSchemes() const {

  static QStringList res = QStringList() << u"file"_s
                                         << u"http"_s
                                         << u"cdda"_s
                                         << u"smb"_s
                                         << u"sftp"_s;
  return res;

}

QStringList Mpris2::SupportedMimeTypes() const {

  static QStringList res = QStringList() << u"x-content/audio-player"_s
                                         << u"application/ogg"_s
                                         << u"application/x-ogg"_s
                                         << u"application/x-ogm-audio"_s
                                         << u"audio/flac"_s
                                         << u"audio/ogg"_s
                                         << u"audio/vorbis"_s
                                         << u"audio/aac"_s
                                         << u"audio/mp4"_s
                                         << u"audio/mpeg"_s
                                         << u"audio/mpegurl"_s
                                         << u"audio/vnd.rn-realaudio"_s
                                         << u"audio/x-flac"_s
                                         << u"audio/x-oggflac"_s
                                         << u"audio/x-vorbis"_s
                                         << u"audio/x-vorbis+ogg"_s
                                         << u"audio/x-speex"_s
                                         << u"audio/x-wav"_s
                                         << u"audio/x-wavpack"_s
                                         << u"audio/x-ape"_s
                                         << u"audio/x-mp3"_s
                                         << u"audio/x-mpeg"_s
                                         << u"audio/x-mpegurl"_s
                                         << u"audio/x-ms-wma"_s
                                         << u"audio/x-musepack"_s
                                         << u"audio/x-pn-realaudio"_s
                                         << u"audio/x-scpls"_s
                                         << u"video/x-ms-asf"_s;

  return res;

}

void Mpris2::Raise() { Q_EMIT RaiseMainWindow(); }

void Mpris2::Quit() { QCoreApplication::quit(); }

QString Mpris2::PlaybackStatus() const {
  return PlaybackStatus(player_->GetState());
}

QString Mpris2::PlaybackStatus(EngineBase::State state) const {

  switch (state) {
    case EngineBase::State::Playing: return u"Playing"_s;
    case EngineBase::State::Paused: return u"Paused"_s;
    default: return u"Stopped"_s;
  }

}

QString Mpris2::LoopStatus() const {

  if (!playlist_manager_->sequence()) {
    return u"None"_s;
  }

  switch (playlist_manager_->active() ? playlist_manager_->active()->RepeatMode() : playlist_manager_->sequence()->repeat_mode()) {
    case PlaylistSequence::RepeatMode::Off: return u"None"_s;
    case PlaylistSequence::RepeatMode::Album:
    case PlaylistSequence::RepeatMode::Playlist: return u"Playlist"_s;
    case PlaylistSequence::RepeatMode::Track: return u"Track"_s;
    default: return u"None"_s;
  }

}

void Mpris2::SetLoopStatus(const QString &value) {

  if (!playlist_manager_->sequence()) {
    return;
  }

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

  if (playlist_manager_->active()) {
    playlist_manager_->active()->sequence()->SetRepeatMode(mode);
  }
  else {
    playlist_manager_->sequence()->SetRepeatMode(mode);
  }

}

double Mpris2::Rate() const { return 1.0; }

void Mpris2::SetRate(double rate) {

  if (rate == 0) {
    player_->Pause();
  }

}

bool Mpris2::Shuffle() const {

  if (!playlist_manager_->active() && !playlist_manager_->sequence()) {
    return false;
  }

  const PlaylistSequence::ShuffleMode shuffle_mode = playlist_manager_->active() ? playlist_manager_->active()->ShuffleMode() : playlist_manager_->sequence()->shuffle_mode();
  return shuffle_mode != PlaylistSequence::ShuffleMode::Off;

}

void Mpris2::SetShuffle(bool enable) {

  if (!playlist_manager_->sequence()) {
    return;
  }

  const PlaylistSequence::ShuffleMode mode = enable ? PlaylistSequence::ShuffleMode::All : PlaylistSequence::ShuffleMode::Off;
  if (playlist_manager_->active()) {
    playlist_manager_->active()->sequence()->SetShuffleMode(mode);
  }
  else {
    playlist_manager_->sequence()->SetShuffleMode(mode);
  }

}

QVariantMap Mpris2::Metadata() const { return last_metadata_; }

double Mpris2::Rating() const {

  const float rating = playlist_manager_->active() ? playlist_manager_->active()->current_item_metadata().rating() : .0F;
  return (rating <= 0) ? 0 : rating;

}

void Mpris2::SetRating(double rating) {

  if (rating > 1.0) {
    rating = 1.0;
  }
  else if (rating <= 0.0) {
    rating = -1.0;
  }

  playlist_manager_->RateCurrentSong(static_cast<float>(rating));

}

int Mpris2::current_playlist_row() const {
  return playlist_manager_->active() ? playlist_manager_->active()->current_row() : -1;
}

QDBusObjectPath Mpris2::current_track_id(const QUuid &current_row) const {
  return QDBusObjectPath(QStringLiteral("/org/strawberrymusicplayer/strawberry/Track/%1").arg(current_row.toString(QUuid::WithoutBraces).replace(u'-', u'_')));
}

// We send Metadata change notification as soon as the process of changing song starts...
void Mpris2::CurrentSongChanged(const Song &song) {

  AlbumCoverLoaded(song);
  EmitNotification(u"CanPlay"_s);
  EmitNotification(u"CanPause"_s);
  EmitNotification(u"CanGoNext"_s, CanGoNext());
  EmitNotification(u"CanGoPrevious"_s, CanGoPrevious());
  EmitNotification(u"CanSeek"_s, CanSeek());

}

void Mpris2::PlaylistItemsAdded(const int playlist_id, const QList<QUuid> &tracks_id, const QUuid after_track_id) {

  if (tracks_id.count() != 1 || !playlist_manager_->active() || playlist_manager_->active_id() != playlist_id) {
    return;
  }
  QDBusObjectPath after_track(QLatin1String(kTrackPrefix) + after_track_id.toString(QUuid::WithoutBraces).replace(u'-', u'_'));

  if (after_track_id.isNull()) {
    after_track = QDBusObjectPath(kNoTrack);
  }
  Q_EMIT TrackAdded(GetTracksMetadata(Track_Ids() << QDBusObjectPath(QLatin1String(kTrackPrefix) + tracks_id[0].toString(QUuid::WithoutBraces).replace(u'-', u'_'))), after_track);

}

void Mpris2::PlaylistItemsRemoved(const int playlist_id, const QList<QUuid> &tracks_id) {

  if (tracks_id.count() != 1 || !playlist_manager_->active() || playlist_manager_->active_id() != playlist_id) {
    return;
  }
  Q_EMIT TrackRemoved(QDBusObjectPath(QLatin1String(kTrackPrefix) + tracks_id[0].toString(QUuid::WithoutBraces).replace(u'-', u'_')));

}

void Mpris2::EmitTrackListReplaced() {
  const Track_Ids tracks = Tracks();
  const int current_row = playlist_manager_->active()->current_row();
  QDBusObjectPath current_track_path(kNoTrack);
  if (current_row >= 0 && current_row < playlist_manager_->active()->rowCount()) {
    const QUuid &unique_id = playlist_manager_->active()->items()[current_row]->uuid();
    current_track_path = QDBusObjectPath(QLatin1String(kTrackPrefix) + unique_id.toString(QUuid::WithoutBraces).replace(u'-', u'_'));
  }

  Q_EMIT TrackListReplaced(tracks, current_track_path);
}

// ... and we add the cover information later, when it's available.
void Mpris2::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  const int current_row = current_playlist_row();
  if (current_row == -1) return;

  const QUuid unique_id = playlist_manager_->active()->items()[current_row]->uuid();
  last_metadata_ = QVariantMap();
  song.ToXesam(&last_metadata_);

  using mpris::AddMetadata;
  AddMetadata(u"mpris:trackid"_s, current_track_id(unique_id), &last_metadata_);

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
    AddMetadata(u"mpris:artUrl"_s, cover_url.toString(), &last_metadata_);
  }

  AddMetadata(u"year"_s, song.year(), &last_metadata_);
  AddMetadata(u"bitrate"_s, song.bitrate(), &last_metadata_);

  EmitNotification(u"Metadata"_s, last_metadata_);

}

double Mpris2::Volume() const {
  return player_->GetVolume() / 100.0;
}

void Mpris2::SetVolume(const double volume) {
  player_->SetVolume(static_cast<uint>(qBound(0L, lround(volume * 100.0), 100L)));
}

qint64 Mpris2::Position() const {
  return player_->engine()->position_nanosec() / kNsecPerUsec;
}

double Mpris2::MaximumRate() const { return 1.0; }

double Mpris2::MinimumRate() const { return 1.0; }

bool Mpris2::CanGoNext() const {

  const bool can_go_next = playlist_manager_->active() && playlist_manager_->active()->next_row() != -1;
  qLog(Debug) << "MPRIS2: Can go next" << can_go_next;
  return can_go_next;

}

bool Mpris2::CanGoPrevious() const {

  const bool can_go_previous = playlist_manager_->active() && (playlist_manager_->active()->previous_row() != -1 || player_->PreviousWouldRestartTrack());
  qLog(Debug) << "MPRIS2: Can go previous" << can_go_previous;
  return can_go_previous;

}

bool Mpris2::CanPlay() const {

  const bool can_play = playlist_manager_->active() && playlist_manager_->active()->rowCount() != 0;
  qLog(Debug) << "MPRIS2: Can play" << can_play;
  return can_play;

}

// This one's a bit different than MPRIS 1 - we want this to be true even when the song is already paused or stopped.
bool Mpris2::CanPause() const {

  const bool can_pause = (player_->GetCurrentItem() && player_->GetState() == EngineBase::State::Playing && !(player_->GetCurrentItem()->options() & PlaylistItem::Option::PauseDisabled)) || PlaybackStatus() == "Paused"_L1 || PlaybackStatus() == "Stopped"_L1;
  qLog(Debug) << "MPRIS2: Can pause" << can_pause;
  return can_pause;

}

bool Mpris2::CanSeek() const {

  const bool can_seek = CanSeek(player_->GetState());
  qLog(Debug) << "MPRIS2: Can can seek" << can_seek;
  return can_seek;

}

bool Mpris2::CanSeek(EngineBase::State state) const {
  return player_->GetCurrentItem() && state != EngineBase::State::Empty && !player_->GetCurrentItem()->EffectiveMetadata().is_stream();
}

bool Mpris2::CanControl() const { return true; }

void Mpris2::Next() {

  if (CanGoNext()) {
    player_->Next();
  }

}

void Mpris2::Previous() {

  if (CanGoPrevious()) {
    player_->Previous();
  }

}

void Mpris2::Pause() {

  if (CanPause() && player_->GetState() != EngineBase::State::Paused) {
    player_->Pause();
  }

}

void Mpris2::PlayPause() {

  if (CanPause()) {
    player_->PlayPause();
  }

}

void Mpris2::Stop() { player_->Stop(); }

void Mpris2::Play() {

  if (CanPlay() && player_->GetState() != EngineBase::State::Playing) {
    player_->Play();
  }

}

void Mpris2::Seek(qint64 offset) {

  if (CanSeek()) {
    player_->SeekTo(player_->engine()->position_nanosec() / kNsecPerSec + offset / kUsecPerSec);
  }

}

void Mpris2::SetPosition(const QDBusObjectPath &trackId, qint64 offset) {

  const int current_row = current_playlist_row();
  if (current_row == -1) return;

  const QUuid unique_id = playlist_manager_->active()->items()[current_row]->uuid();
  if (CanSeek() && trackId == current_track_id(unique_id) && offset >= 0) {
    offset *= kNsecPerUsec;

    if (offset < player_->GetCurrentItem()->EffectiveMetadata().length_nanosec()) {
      player_->SeekTo(offset / kNsecPerSec);
    }
  }

}

void Mpris2::OpenUri(const QString &uri) {

  if (playlist_manager_->active()) {
    playlist_manager_->active()->InsertUrls(QList<QUrl>() << QUrl(uri), -1, true);
  }

}

Track_Ids Mpris2::Tracks() const {

  if (!playlist_manager_->active() || playlist_manager_->active()->rowCount() == 0) {
    return Track_Ids();
  }

  Track_Ids track_ids;
  int current_row = playlist_manager_->active()->current_row() - kNbTracksDisplaid;
  int last_row = playlist_manager_->active()->current_row() + kNbTracksDisplaid;

  if (current_row < 0) {
    current_row = 0;
  }
  if (last_row >= playlist_manager_->active()->rowCount()) {
    last_row = playlist_manager_->active()->rowCount();
  }
  for (; current_row < last_row; ++current_row) {
    track_ids << current_track_id(playlist_manager_->active()->items()[current_row]->uuid());
  }

  return track_ids;

}

bool Mpris2::CanEditTracks() const { return playlist_manager_->active() != nullptr; }

TrackMetadata Mpris2::GetTracksMetadata(const Track_Ids &tracks) const {

  if (!playlist_manager_->active()) {
    return TrackMetadata();
  }

  TrackMetadata track_metadata;
  for (const QDBusObjectPath &track_object_path : tracks) {
    const QString path = track_object_path.path();
    static const QString track_prefix = QLatin1String(kTrackPrefix);
    if (!path.startsWith(track_prefix)) {
      track_metadata << QVariantMap();
      continue;
    }
    QString id_part = path.mid(track_prefix.length());
    if (id_part.isEmpty()) {
      track_metadata << QVariantMap();
      continue;
    }
    const QUuid playlist_id = QUuid::fromString(id_part.replace(u'_', u'-'));
    PlaylistItemPtr playlist_item = playlist_manager_->active()->find_item_id(playlist_id);
    if (!playlist_item) {
      track_metadata << QVariantMap();
      continue;
    }
    QVariantMap track_map;
    playlist_item->EffectiveMetadata().ToXesam(&track_map);
    track_map.insert(u"mpris:trackid"_s, QVariant::fromValue(track_object_path));
    track_metadata << track_map;
  }

  return track_metadata;

}

void Mpris2::AddTrack(const QString &uri, const QDBusObjectPath &afterTrack, bool setAsCurrent) {

  if (!playlist_manager_->active() || playlist_manager_->active_id() < 0) {
    return;
  }
  int after_track_pos = playlist_manager_->active()->rowCount() - 1;
  const QString path = afterTrack.path();
  if (path == u"/"_s || path == QLatin1String(kNoTrack)) {
    after_track_pos = -1;
  }
  else {
    static const QString track_prefix = QLatin1String(kTrackPrefix);
    if (path.startsWith(track_prefix)) {
      QString id_part = path.mid(track_prefix.length());
      if (!id_part.isEmpty()) {
        const QUuid parsed_track_id = QUuid::fromString(id_part.replace(u'_', u'-'));

        after_track_pos = playlist_manager_->active()->find_pos_id(parsed_track_id);
      }
    }
  }

  const int insert_row = after_track_pos + 1;
  playlist_manager_->InsertUrls(playlist_manager_->active_id(), QList<QUrl>() << QUrl(uri), insert_row, setAsCurrent, false, true);

}

void Mpris2::RemoveTrack(const QDBusObjectPath &trackId) {

  if (!playlist_manager_->active()) {
    return;
  }

  const QString path = trackId.path();
  if (!path.startsWith(QLatin1String(kTrackPrefix))) {
    return;
  }
  QStringList split_path = path.split(u'/');
  if (split_path.isEmpty()) {
    return;
  }
  const QUuid track_uid = QUuid::fromString(split_path.last().replace(u'_', u'-'));
  const int track_index = playlist_manager_->active()->find_pos_id(track_uid);

  if (track_index >= 0 && !playlist_manager_->active()->removeRowSignal(track_index)) {
    qLog(Warning) << "Mpris2::RemoveTrack failed to remove row " << track_index << " id " << track_uid;
  }

}

void Mpris2::GoTo(const QDBusObjectPath &trackId) {

  if (!playlist_manager_->active()) {
    return;
  }

  const QString path = trackId.path();
  if (!path.startsWith(QLatin1String(kTrackPrefix))) {
    return;
  }
  QStringList split_path = path.split(u'/');
  if (split_path.isEmpty()) {
    return;
  }
  const QUuid track_uid = QUuid::fromString(split_path.last().replace(u'_', u'-'));
  const int track_index = playlist_manager_->active()->find_pos_id(track_uid);

  if (track_index >= 0) {
    playlist_manager_->active()->set_current_row(track_index);
    EmitTrackListReplaced();

    // As the GoTo should launch the track play... and PlayAt has some parameters I am not sure of
    Play();
  }

}

quint32 Mpris2::PlaylistCount() const {
  return playlist_manager_->GetAllPlaylists().size();
}

QStringList Mpris2::Orderings() const { return QStringList() << u"User"_s; }

namespace {

QDBusObjectPath MakePlaylistPath(int id) {
  return QDBusObjectPath(QStringLiteral("/org/strawberrymusicplayer/strawberry/PlaylistId/%1").arg(id));
}

}  // namespace

MaybePlaylist Mpris2::ActivePlaylist() const {

  MaybePlaylist maybe_playlist;
  Playlist *current_playlist = playlist_manager_->current();
  maybe_playlist.valid = current_playlist;
  if (!current_playlist) {
    return maybe_playlist;
  }

  maybe_playlist.playlist.id = MakePlaylistPath(current_playlist->id());
  maybe_playlist.playlist.name = playlist_manager_->GetPlaylistName(current_playlist->id());
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
  if (!playlist_manager_->IsPlaylistOpen(p)) {
    qLog(Error) << "Playlist isn't opened!";
    return;
  }
  playlist_manager_->SetActivePlaylist(p);
  player_->Next();
  EmitTrackListReplaced();

}

MprisPlaylistList Mpris2::GetPlaylists(quint32 index, quint32 max_count, const QString &order, bool reverse_order) {

  const QList<Playlist*> playlists = playlist_manager_->GetAllPlaylists();
  MprisPlaylistList ret;
  ret.reserve(playlists.count());
  for (Playlist *p : playlists) {
    MprisPlaylist mpris_playlist;
    mpris_playlist.id = MakePlaylistPath(p->id());
    mpris_playlist.name = playlist_manager_->GetPlaylistName(p->id());
    ret << mpris_playlist;
  }

  if (!order.isEmpty()) {
    // Align with Orderings(): currently only "Alphabetical" is advertised there.
    // other values : CreationDate, ModifiedDate, LastPlayDate cannot be handled as we don't have such values
    if (order == "Alphabetical"_L1) {
      std::sort(ret.begin(), ret.end(), [](const MprisPlaylist &a, const MprisPlaylist &b) { return a.name < b.name; });
    }
    // Let's say user defined will sort by the id
    else if (order == "UserDefined"_L1) {
      std::sort(ret.begin(), ret.end(), [](const MprisPlaylist &a, const MprisPlaylist &b) { return a.id < b.id; });
    }
  }
  if (reverse_order) {
    std::reverse(ret.begin(), ret.end());
  }

  return ret.mid(index, max_count);

}

void Mpris2::PlaylistChangedSlot(Playlist *playlist) {

  MprisPlaylist mpris_playlist;
  mpris_playlist.id = MakePlaylistPath(playlist->id());
  mpris_playlist.name = playlist_manager_->GetPlaylistName(playlist->id());

  Q_EMIT PlaylistChanged(mpris_playlist);

}

void Mpris2::PlaylistCollectionChanged(Playlist *playlist) {
  Q_UNUSED(playlist);
  EmitNotification(u"PlaylistCount"_s, ""_L1, u"org.mpris.MediaPlayer2.Playlists"_s);
}

}  // namespace mpris
