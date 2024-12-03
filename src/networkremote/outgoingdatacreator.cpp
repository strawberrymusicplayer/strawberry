/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Andreas Muttscheller <asfa194@gmail.com>
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cmath>

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QBuffer>
#include <QTimer>
#include <QStandardPaths>

#include "includes/shared_ptr.h"
#include "constants/timeconstants.h"
#include "utilities/randutils.h"
#include "core/player.h"
#include "core/database.h"
#include "core/sqlquery.h"
#include "core/logging.h"
#include "utilities/cryptutils.h"
#include "collection/collectionbackend.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistbackend.h"
#include "networkremote.h"
#include "networkremoteclient.h"
#include "outgoingdatacreator.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

namespace {
constexpr quint32 kFileChunkSize = 100000;
}

OutgoingDataCreator::OutgoingDataCreator(const SharedPtr<Database> database,
                                         const SharedPtr<Player> player,
                                         const SharedPtr<PlaylistManager> playlist_manager,
                                         const SharedPtr<PlaylistBackend> playlist_backend,
                                         QObject *parent)
    : QObject(parent),
      database_(database),
      player_(player),
      playlist_manager_(playlist_manager),
      playlist_backend_(playlist_backend),
      keep_alive_timer_(new QTimer(this)),
      keep_alive_timeout_(10000) {

  QObject::connect(keep_alive_timer_, &QTimer::timeout, this, &OutgoingDataCreator::SendKeepAlive);

}

OutgoingDataCreator::~OutgoingDataCreator() = default;

void OutgoingDataCreator::SetClients(QList<NetworkRemoteClient*> *clients) {

  clients_ = clients;
  // After we got some clients, start the keep alive timer
  // Default: every 10 seconds
  keep_alive_timer_->start(keep_alive_timeout_);

  // Create the song position timer
  track_position_timer_ = new QTimer(this);
  QObject::connect(track_position_timer_, &QTimer::timeout, this, &OutgoingDataCreator::UpdateTrackPosition);

}

void OutgoingDataCreator::SendDataToClients(networkremote::Message *msg) {

  if (clients_->empty()) {
    return;
  }

  for (NetworkRemoteClient *client : std::as_const(*clients_)) {
    // Do not send data to downloaders
    if (client->isDownloader()) {
      if (client->State() != QTcpSocket::ConnectedState) {
        clients_->removeAt(clients_->indexOf(client));
        delete client;
      }
      continue;
    }

    // Check if the client is still active
    if (client->State() == QTcpSocket::ConnectedState) {
      client->SendData(msg);
    }
    else {
      clients_->removeAt(clients_->indexOf(client));
      delete client;
    }
  }

}

void OutgoingDataCreator::SendInfo() {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::INFO);
  networkremote::ResponseInfo info;
  info.setVersion(QLatin1String("%1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()));
  info.setFilesMusicExtensions(files_music_extensions_);
  info.setAllowDownloads(allow_downloads_);
  info.setState(GetEngineState());
  msg.setResponseInfo(info);
  SendDataToClients(&msg);

}

void OutgoingDataCreator::SendKeepAlive() {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::KEEP_ALIVE);
  SendDataToClients(&msg);

}

networkremote::EngineStateGadget::EngineState OutgoingDataCreator::GetEngineState() {

  switch (player_->GetState()) {
    case EngineBase::State::Idle:
      return networkremote::EngineStateGadget::EngineState::EngineState_Idle;
      break;
    case EngineBase::State::Error:
    case EngineBase::State::Empty:
      return networkremote::EngineStateGadget::EngineState::EngineState_Empty;
      break;
    case EngineBase::State::Playing:
      return networkremote::EngineStateGadget::EngineState::EngineState_Playing;
      break;
    case EngineBase::State::Paused:
      return networkremote::EngineStateGadget::EngineState::EngineState_Paused;
      break;
  }

  return networkremote::EngineStateGadget::EngineState::EngineState_Empty;

}

void OutgoingDataCreator::SendAllPlaylists() {

  // Get all Playlists
  const int active_playlist = playlist_manager_->active_id();

  // Create message
  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::SEND_PLAYLISTS);

  networkremote::ResponsePlaylists playlists = msg.responsePlaylists();
  playlists.setIncludeClosed(true);

  // Get all playlists, even ones that are hidden in the UI.
  const QList<PlaylistBackend::Playlist> all_playlists = playlist_backend_->GetAllPlaylists();
  for (const PlaylistBackend::Playlist &p : all_playlists) {
    const bool playlist_open = playlist_manager_->IsPlaylistOpen(p.id);
    const int item_count = playlist_open ? playlist_manager_->playlist(p.id)->rowCount() : 0;

    // Create a new playlist
    networkremote::Playlist playlist;// = playlists.playlist();
    playlist.setPlaylistId(p.id);
    playlist.setName(p.name);
    playlist.setActive((p.id == active_playlist));
    playlist.setItemCount(item_count);
    playlist.setClosed(!playlist_open);
    playlist.setFavorite(p.favorite);
  }

  SendDataToClients(&msg);

}

void OutgoingDataCreator::SendAllActivePlaylists() {

  const int active_playlist = playlist_manager_->active_id();

  const QList<Playlist*> playlists = playlist_manager_->GetAllPlaylists();
  QList<networkremote::Playlist> pb_playlists;
  pb_playlists.reserve(playlists.count());
  for (Playlist *p : playlists) {
    networkremote::Playlist pb_playlist;
    pb_playlist.setPlaylistId(p->id());
    pb_playlist.setName(playlist_manager_->GetPlaylistName(p->id()));
    pb_playlist.setActive(p->id() == active_playlist);
    pb_playlist.setItemCount(p->rowCount());
    pb_playlist.setClosed(false);
    pb_playlist.setFavorite(p->is_favorite());
    pb_playlists << pb_playlist;
  }

  networkremote::ResponsePlaylists response_playlists;
  response_playlists.setPlaylist(pb_playlists);
  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::SEND_PLAYLISTS);
  msg.setResponsePlaylists(response_playlists);

  SendDataToClients(&msg);

}

void OutgoingDataCreator::ActiveChanged(Playlist *playlist) {

  SendPlaylistSongs(playlist->id());

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::ACTIVE_PLAYLIST_CHANGED);
  networkremote::ResponseActiveChanged response_active_changed;
  response_active_changed.setPlaylistId(playlist->id());
  msg.setResponseActiveChanged(response_active_changed);
  SendDataToClients(&msg);

}

void OutgoingDataCreator::PlaylistAdded(const int id, const QString &name, const bool favorite) {

  Q_UNUSED(id)
  Q_UNUSED(name)
  Q_UNUSED(favorite)

  SendAllActivePlaylists();

}

void OutgoingDataCreator::PlaylistDeleted(const int id) {

  Q_UNUSED(id)

  SendAllActivePlaylists();

}

void OutgoingDataCreator::PlaylistClosed(const int id) {

  Q_UNUSED(id)

  SendAllActivePlaylists();

}

void OutgoingDataCreator::PlaylistRenamed(const int id, const QString &new_name) {

  Q_UNUSED(id)
  Q_UNUSED(new_name)

  SendAllActivePlaylists();

}

void OutgoingDataCreator::SendFirstData(const bool send_playlist_songs) {

  CurrentSongChanged(current_song_, albumcoverloader_result_);

  VolumeChanged(player_->GetVolume());

  if (!track_position_timer_->isActive() && player_->engine()->state() == EngineBase::State::Playing) {
    track_position_timer_->start(1s);
  }

  UpdateTrackPosition();

  SendAllActivePlaylists();

  if (send_playlist_songs) {
    SendPlaylistSongs(playlist_manager_->active_id());
  }

  SendShuffleMode(playlist_manager_->sequence()->shuffle_mode());
  SendRepeatMode(playlist_manager_->sequence()->repeat_mode());

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::FIRST_DATA_SENT_COMPLETE);
  SendDataToClients(&msg);

}

void OutgoingDataCreator::CurrentSongChanged(const Song &song, const AlbumCoverLoaderResult &result) {

  albumcoverloader_result_ = result;
  current_song_ = song;
  current_image_ = result.album_cover.image;

  SendSongMetadata();

}

void OutgoingDataCreator::SendSongMetadata() {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::CURRENT_METAINFO);
  const networkremote::SongMetadata pb_song_metadata = PbSongMetadataFromSong(playlist_manager_->active()->current_row(), current_song_, current_image_);
  networkremote::ResponseCurrentMetadata response_current_metadata;
  response_current_metadata.setSongMetadata(pb_song_metadata);
  msg.setResponseCurrentMetadata(response_current_metadata);
  SendDataToClients(&msg);

}

networkremote::SongMetadata OutgoingDataCreator::PbSongMetadataFromSong(const int index, const Song &song, const QImage &image_cover_art) {

  if (!song.is_valid()) {
    return networkremote::SongMetadata();
  }

  networkremote::SongMetadata pb_song_metadata;
  pb_song_metadata.setSongId(song.id());
  pb_song_metadata.setIndex(index);
  pb_song_metadata.setTitle(song.PrettyTitle());
  pb_song_metadata.setArtist(song.artist());
  pb_song_metadata.setAlbum(song.album());
  pb_song_metadata.setAlbumartist(song.albumartist());
  pb_song_metadata.setLength(song.length_nanosec() / kNsecPerSec);
  pb_song_metadata.setPrettyLength(song.PrettyLength());
  pb_song_metadata.setGenre(song.genre());
  pb_song_metadata.setPrettyYear(song.PrettyYear());
  pb_song_metadata.setTrack(song.track());
  pb_song_metadata.setDisc(song.disc());
  pb_song_metadata.setPlaycount(song.playcount());
  pb_song_metadata.setIsLocal(song.url().isLocalFile());
  pb_song_metadata.setFilename(song.basefilename());
  pb_song_metadata.setFileSize(song.filesize());
  pb_song_metadata.setRating(song.rating());
  pb_song_metadata.setUrl(song.url().toString());
  pb_song_metadata.setArtAutomatic(song.art_automatic().toString());
  pb_song_metadata.setArtManual(song.art_manual().toString());
  pb_song_metadata.setFiletype(static_cast<networkremote::SongMetadata::FileType>(song.filetype()));

  if (!image_cover_art.isNull()) {
    QImage image_cover_art_small;
    if (image_cover_art.width() > 1000 || image_cover_art.height() > 1000) {
      image_cover_art_small = image_cover_art.scaled(1000, 1000, Qt::KeepAspectRatio);
    }
    else {
      image_cover_art_small = image_cover_art;
    }

    QByteArray data;
    QBuffer buffer(&data);
    if (buffer.open(QIODevice::WriteOnly)) {
      image_cover_art_small.save(&buffer, "JPG");
      buffer.close();
    }

    pb_song_metadata.setArt(data);
  }

  return pb_song_metadata;

}

void OutgoingDataCreator::VolumeChanged(const uint volume) {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::SET_VOLUME);
  networkremote::RequestSetVolume request_set_volume;
  request_set_volume.setVolume(volume);
  msg.setRequestSetVolume(request_set_volume);
  SendDataToClients(&msg);

}

void OutgoingDataCreator::SendPlaylistSongs(const int playlist_id) {

  Playlist *playlist = playlist_manager_->playlist(playlist_id);
  if (!playlist) {
    qLog(Error) << "Could not find playlist with ID" << playlist_id;
    return;
  }

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::SEND_PLAYLIST_SONGS);

  networkremote::Playlist pb_playlist;
  pb_playlist.setPlaylistId(playlist_id);
  networkremote::ResponsePlaylistSongs pb_response_playlist_songs;
  pb_response_playlist_songs.setRequestedPlaylist(pb_playlist);

  const SongList songs = playlist->GetAllSongs();
  QList<networkremote::SongMetadata> pb_song_metadatas;
  pb_song_metadatas.reserve(songs.count());
  for (const Song &song : songs) {
    pb_song_metadatas << PbSongMetadataFromSong(songs.indexOf(song), song);
  }

  pb_response_playlist_songs.setSongs(pb_song_metadatas);
  msg.setResponsePlaylistSongs(pb_response_playlist_songs);

  SendDataToClients(&msg);

}

void OutgoingDataCreator::PlaylistChanged(Playlist *playlist) {
  SendPlaylistSongs(playlist->id());
}

void OutgoingDataCreator::StateChanged(const EngineBase::State state) {

  if (state == last_state_) {
    return;
  }
  last_state_ = state;

  networkremote::Message msg;

  switch (state) {
    case EngineBase::State::Playing:
      msg.setType(networkremote::MsgTypeGadget::MsgType::PLAY);
      track_position_timer_->start(1s);
      break;
    case EngineBase::State::Paused:
      msg.setType(networkremote::MsgTypeGadget::MsgType::PAUSE);
      track_position_timer_->stop();
      break;
    case EngineBase::State::Empty:
      msg.setType(networkremote::MsgTypeGadget::MsgType::STOP);  // Empty is called when player stopped
      track_position_timer_->stop();
      break;
    default:
      msg.setType(networkremote::MsgTypeGadget::MsgType::STOP);
      track_position_timer_->stop();
      break;
  };

  SendDataToClients(&msg);

}

void OutgoingDataCreator::SendRepeatMode(const PlaylistSequence::RepeatMode mode) {

  networkremote::Repeat repeat;

  switch (mode) {
    case PlaylistSequence::RepeatMode::Off:
      repeat.setRepeatMode(networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Off);
      break;
    case PlaylistSequence::RepeatMode::Track:
      repeat.setRepeatMode(networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Track);
      break;
    case PlaylistSequence::RepeatMode::Album:
      repeat.setRepeatMode(networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Album);
      break;
    case PlaylistSequence::RepeatMode::Playlist:
      repeat.setRepeatMode(networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Playlist);
      break;
    case PlaylistSequence::RepeatMode::OneByOne:
      repeat.setRepeatMode(networkremote::RepeatModeGadget::RepeatMode::RepeatMode_OneByOne);
      break;
    case PlaylistSequence::RepeatMode::Intro:
      repeat.setRepeatMode(networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Intro);
      break;
  }

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::REPEAT);
  msg.setRepeat(repeat);

  SendDataToClients(&msg);

}

void OutgoingDataCreator::SendShuffleMode(const PlaylistSequence::ShuffleMode mode) {

  networkremote::Shuffle shuffle;

  switch (mode) {
    case PlaylistSequence::ShuffleMode::Off:
      shuffle.setShuffleMode(networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_Off);
      break;
    case PlaylistSequence::ShuffleMode::All:
      shuffle.setShuffleMode(networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_All);
      break;
    case PlaylistSequence::ShuffleMode::InsideAlbum:
      shuffle.setShuffleMode(networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_InsideAlbum);
      break;
    case PlaylistSequence::ShuffleMode::Albums:
      shuffle.setShuffleMode(networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_Albums);
      break;
  }

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::SHUFFLE);
  msg.setShuffle(shuffle);

  SendDataToClients(&msg);

}

void OutgoingDataCreator::UpdateTrackPosition() {

  const qint64 position_nanosec = player_->engine()->position_nanosec();
  int position = static_cast<int>(std::floor(static_cast<double>(position_nanosec) / kNsecPerSec + 0.5));

  if (position_nanosec > current_song_.length_nanosec()) {
    position = last_track_position_;
  }

  last_track_position_ = position;

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::UPDATE_TRACK_POSITION);
  networkremote::ResponseUpdateTrackPosition reponse_update_track_position;
  reponse_update_track_position.setPosition(position);
  msg.setResponseUpdateTrackPosition(reponse_update_track_position);

  SendDataToClients(&msg);

}

void OutgoingDataCreator::DisconnectAllClients() {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::DISCONNECT);
  networkremote::ResponseDisconnect reponse_disconnect;
  reponse_disconnect.setReasonDisconnect(networkremote::ReasonDisconnectGadget::ReasonDisconnect::Server_Shutdown);
  msg.setResponseDisconnect(reponse_disconnect);
  SendDataToClients(&msg);

}

void OutgoingDataCreator::SendCollection(NetworkRemoteClient *client) {

  const QString temp_database_filename = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + u'/' + Utilities::GetRandomStringWithChars(20);

  Database::AttachedDatabase adb(temp_database_filename, ""_L1, true);
  QSqlDatabase db(database_->Connect());

  database_->AttachDatabaseOnDbConnection(u"songs_export"_s, adb, db);

  SqlQuery q(db);
  q.prepare(u"CREATE TABLE songs_export.songs AS SELECT * FROM songs WHERE unavailable = 0"_s);

  if (!q.exec()) {
    database_->ReportErrors(q);
    return;
  }

  database_->DetachDatabase(u"songs_export"_s);

  QFile file(temp_database_filename);
  const QByteArray sha1 = Utilities::Sha1File(file).toHex();
  qLog(Debug) << "Collection SHA1" << sha1;

  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Could not open file" << temp_database_filename;
  }

  const int chunk_count = qRound((file.size() / kFileChunkSize) + 0.5);
  int chunk_number = 0;
  while (!file.atEnd()) {
    ++chunk_number;
    const QByteArray data = file.read(kFileChunkSize);
    networkremote::ResponseCollectionChunk chunk;
    chunk.setChunkNumber(chunk_number);
    chunk.setChunkCount(chunk_count);
    chunk.setSize(file.size());
    chunk.setData(data);
    chunk.setFileHash(sha1);
    networkremote::Message msg;
    msg.setType(networkremote::MsgTypeGadget::MsgType::COLLECTION_CHUNK);
    msg.setResponseCollectionChunk(chunk);
    client->SendData(&msg);
  }

  file.remove();
  file.close();

}

void OutgoingDataCreator::SendListFiles(QString relative_path, NetworkRemoteClient *client) {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::LIST_FILES);
  networkremote::ResponseListFiles files;

  if (files_root_folder_.isEmpty()) {
    files.setError(networkremote::ResponseListFiles::Error::ROOT_DIR_NOT_SET);
    SendDataToClients(&msg);
    return;
  }

  QDir root_dir(files_root_folder_);
  if (!root_dir.exists()) {
    files.setError(networkremote::ResponseListFiles::Error::ROOT_DIR_NOT_SET);
  }
  else if (relative_path.startsWith(".."_L1) || relative_path.startsWith("./.."_L1)) {
    files.setError(networkremote::ResponseListFiles::Error::DIR_NOT_ACCESSIBLE);
  }
  else {
    if (relative_path.startsWith("/"_L1)) relative_path.remove(0, 1);

    QFileInfo fi_folder(root_dir, relative_path);
    if (!fi_folder.exists()) {
      files.setError(networkremote::ResponseListFiles::Error::DIR_NOT_EXIST);
    }
    else if (!fi_folder.isDir()) {
      files.setError(networkremote::ResponseListFiles::Error::DIR_NOT_EXIST);
    }
    else if (root_dir.relativeFilePath(fi_folder.absoluteFilePath()).startsWith("../"_L1)) {
      files.setError(networkremote::ResponseListFiles::Error::DIR_NOT_ACCESSIBLE);
    }
    else {
      files.setRelativePath(root_dir.relativeFilePath(fi_folder.absoluteFilePath()));
      QDir dir(fi_folder.absoluteFilePath());
      dir.setFilter(QDir::NoDotAndDotDot | QDir::AllEntries);
      dir.setSorting(QDir::Name | QDir::DirsFirst);

      const QList<QFileInfo> fis = dir.entryInfoList();
      for (const QFileInfo &fi : fis) {
        if (fi.isDir() || files_music_extensions_.contains(fi.suffix())) {
          networkremote::FileMetadata pb_file;// = files->addFiles();
          pb_file.setIsDir(fi.isDir());
          pb_file.setFilename(fi.fileName());
        }
      }
    }
  }

  msg.setResponseListFiles(files);

  client->SendData(&msg);

}
