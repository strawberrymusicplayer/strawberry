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

#include <algorithm>

#include <QString>
#include <QUrl>
#include <QDir>
#include <QSettings>

#include "core/logging.h"
#include "core/mimedata.h"
#include "constants/timeconstants.h"
#include "engine/enginebase.h"
#include "playlist/playlist.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "incomingdataparser.h"
#include "scrobbler/audioscrobbler.h"
#include "constants/mainwindowsettings.h"

using namespace Qt::Literals::StringLiterals;

IncomingDataParser::IncomingDataParser(const SharedPtr<Player> player,
                                       const SharedPtr<PlaylistManager> playlist_manager,
                                       const SharedPtr<AudioScrobbler> scrobbler,
                                       QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      scrobbler_(scrobbler),
      close_connection_(false),
      doubleclick_playlist_addmode_(BehaviourSettings::PlaylistAddBehaviour::Enqueue) {

  ReloadSettings();

  QObject::connect(this, &IncomingDataParser::Play, &*player_, &Player::PlayHelper);
  QObject::connect(this, &IncomingDataParser::PlayPause, &*player_, &Player::PlayPauseHelper);
  QObject::connect(this, &IncomingDataParser::Pause, &*player_, &Player::Pause);
  QObject::connect(this, &IncomingDataParser::Stop, &*player_, &Player::Stop);
  QObject::connect(this, &IncomingDataParser::StopAfterCurrent, &*player_, &Player::StopAfterCurrent);
  QObject::connect(this, &IncomingDataParser::Next, &*player_, &Player::Next);
  QObject::connect(this, &IncomingDataParser::Previous, &*player_, &Player::Previous);
  QObject::connect(this, &IncomingDataParser::SetVolume, &*player_, &Player::SetVolume);
  QObject::connect(this, &IncomingDataParser::PlayAt, &*player_, &Player::PlayAt);
  QObject::connect(this, &IncomingDataParser::SeekTo, &*player_, &Player::SeekTo);

  QObject::connect(this, &IncomingDataParser::Enqueue, &*playlist_manager_, &PlaylistManager::Enqueue);
  QObject::connect(this, &IncomingDataParser::SetActivePlaylist, &*playlist_manager_, &PlaylistManager::SetActivePlaylist);
  QObject::connect(this, &IncomingDataParser::ShuffleCurrent, &*playlist_manager_, &PlaylistManager::ShuffleCurrent);
  QObject::connect(this, &IncomingDataParser::InsertUrls, &*playlist_manager_, &PlaylistManager::InsertUrls);
  QObject::connect(this, &IncomingDataParser::InsertSongs, &*playlist_manager_, &PlaylistManager::InsertSongs);
  QObject::connect(this, &IncomingDataParser::RemoveSongs, &*playlist_manager_, &PlaylistManager::RemoveItemsWithoutUndo);
  QObject::connect(this, &IncomingDataParser::New, &*playlist_manager_, &PlaylistManager::New);
  QObject::connect(this, &IncomingDataParser::Open, &*playlist_manager_, &PlaylistManager::Open);
  QObject::connect(this, &IncomingDataParser::Close, &*playlist_manager_, &PlaylistManager::Close);
  QObject::connect(this, &IncomingDataParser::Clear, &*playlist_manager_, &PlaylistManager::Clear);
  QObject::connect(this, &IncomingDataParser::Rename, &*playlist_manager_, &PlaylistManager::Rename);
  QObject::connect(this, &IncomingDataParser::Favorite, &*playlist_manager_, &PlaylistManager::Favorite);

  QObject::connect(this, &IncomingDataParser::SetRepeatMode, &*playlist_manager_->sequence(), &PlaylistSequence::SetRepeatMode);
  QObject::connect(this, &IncomingDataParser::SetShuffleMode, &*playlist_manager_->sequence(), &PlaylistSequence::SetShuffleMode);

  QObject::connect(this, &IncomingDataParser::RateCurrentSong, &*playlist_manager_, &PlaylistManager::RateCurrentSong);

  QObject::connect(this, &IncomingDataParser::Love, &*scrobbler_, &AudioScrobbler::Love);

}

IncomingDataParser::~IncomingDataParser() = default;

void IncomingDataParser::ReloadSettings() {

  QSettings s;
  s.beginGroup(MainWindowSettings::kSettingsGroup);
  doubleclick_playlist_addmode_ = static_cast<BehaviourSettings::PlaylistAddBehaviour>(s.value(BehaviourSettings::kDoubleClickPlaylistAddMode, static_cast<int>(BehaviourSettings::PlaylistAddBehaviour::Enqueue)).toInt());
  s.endGroup();

}

bool IncomingDataParser::close_connection() const { return close_connection_; }

void IncomingDataParser::SetRemoteRootFiles(const QString &files_root_folder) {
  files_root_folder_ = files_root_folder;
}

Song IncomingDataParser::SongFromPbSongMetadata(const networkremote::SongMetadata &pb_song_metadata) const {

  Song song;
  song.Init(pb_song_metadata.title(), pb_song_metadata.artist(), pb_song_metadata.album(), pb_song_metadata.length() * kNsecPerSec);
  song.set_albumartist(pb_song_metadata.albumartist());
  song.set_genre(pb_song_metadata.genre());
  song.set_year(pb_song_metadata.prettyYear().toInt());
  song.set_track(pb_song_metadata.track());
  song.set_disc(pb_song_metadata.disc());
  song.set_url(QUrl(pb_song_metadata.url()));
  song.set_filesize(pb_song_metadata.fileSize());
  song.set_rating(pb_song_metadata.rating());
  song.set_basefilename(pb_song_metadata.filename());
  song.set_art_automatic(QUrl(pb_song_metadata.artAutomatic()));
  song.set_art_manual(QUrl(pb_song_metadata.artManual()));
  song.set_filetype(static_cast<Song::FileType>(pb_song_metadata.filetype()));

  return song;

}

void IncomingDataParser::Parse(const networkremote::Message &msg) {

  close_connection_ = false;
  NetworkRemoteClient *client = qobject_cast<NetworkRemoteClient*>(sender());

  switch (msg.type()) {
    case networkremote::MsgTypeGadget::MsgType::CONNECT:
      ClientConnect(msg, client);
      break;
    case networkremote::MsgTypeGadget::MsgType::DISCONNECT:
      close_connection_ = true;
      break;
    case networkremote::MsgTypeGadget::MsgType::GET_COLLECTION:
      Q_EMIT SendCollection(client);
      break;
    case networkremote::MsgTypeGadget::MsgType::GET_PLAYLISTS:
      ParseSendPlaylists(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::GET_PLAYLIST_SONGS:
      ParseGetPlaylistSongs(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::SET_VOLUME:
      Q_EMIT SetVolume(msg.requestSetVolume().volume());
      break;
    case networkremote::MsgTypeGadget::MsgType::PLAY:
      Q_EMIT Play();
      break;
    case networkremote::MsgTypeGadget::MsgType::PLAYPAUSE:
      Q_EMIT PlayPause();
      break;
    case networkremote::MsgTypeGadget::MsgType::PAUSE:
      Q_EMIT Pause();
      break;
    case networkremote::MsgTypeGadget::MsgType::STOP:
      Q_EMIT Stop();
      break;
    case networkremote::MsgTypeGadget::MsgType::STOP_AFTER:
      Q_EMIT StopAfterCurrent();
      break;
    case networkremote::MsgTypeGadget::MsgType::NEXT:
      Q_EMIT Next();
      break;
    case networkremote::MsgTypeGadget::MsgType::PREVIOUS:
      Q_EMIT Previous();
      break;
    case networkremote::MsgTypeGadget::MsgType::CHANGE_SONG:
      ParseChangeSong(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::SHUFFLE_PLAYLIST:
      Q_EMIT ShuffleCurrent();
      break;
    case networkremote::MsgTypeGadget::MsgType::REPEAT:
      ParseSetRepeatMode(msg.repeat());
      break;
    case networkremote::MsgTypeGadget::MsgType::SHUFFLE:
      ParseSetShuffleMode(msg.shuffle());
      break;
    case networkremote::MsgTypeGadget::MsgType::SET_TRACK_POSITION:
      Q_EMIT SeekTo(msg.requestSetTrackPosition().position());
      break;
    case networkremote::MsgTypeGadget::MsgType::PLAYLIST_INSERT_URLS:
      ParseInsertUrls(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::REMOVE_PLAYLIST_SONGS:
      ParseRemoveSongs(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::OPEN_PLAYLIST:
      ParseOpenPlaylist(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::CLOSE_PLAYLIST:
      ParseClosePlaylist(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::UPDATE_PLAYLIST:
      ParseUpdatePlaylist(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::LOVE:
      Q_EMIT Love();
      break;
    case networkremote::MsgTypeGadget::MsgType::GET_LYRICS:
      Q_EMIT GetLyrics();
      break;
    case networkremote::MsgTypeGadget::MsgType::DOWNLOAD_SONGS:
      client->song_sender()->SendSongs(msg.requestDownloadSongs());
      break;
    case networkremote::MsgTypeGadget::MsgType::SONG_OFFER_RESPONSE:
      client->song_sender()->ResponseSongOffer(msg.responseSongOffer().accepted());
      break;
    case networkremote::MsgTypeGadget::MsgType::RATE_SONG:
      ParseRateSong(msg);
      break;
    case networkremote::MsgTypeGadget::MsgType::REQUEST_FILES:
      Q_EMIT SendListFiles(msg.requestListFiles().relativePath(), client);
      break;
    case networkremote::MsgTypeGadget::MsgType::APPEND_FILES:
      ParseAppendFilesToPlaylist(msg);
      break;

    default:
      break;
  }

}

void IncomingDataParser::ClientConnect(const networkremote::Message &msg, NetworkRemoteClient *client) {

  Q_EMIT SendInfo();

  if (!client->isDownloader()) {
    if (!msg.requestConnect().hasSendPlaylistSongs() || msg.requestConnect().sendPlaylistSongs()) {
      Q_EMIT SendFirstData(true);
    }
    else {
      Q_EMIT SendFirstData(false);
    }
  }

}

void IncomingDataParser::ParseGetPlaylistSongs(const networkremote::Message &msg) {
  Q_EMIT SendPlaylistSongs(msg.requestPlaylistSongs().playlistId());
}

void IncomingDataParser::ParseChangeSong(const networkremote::Message &msg) {

  // Get the first entry and check if there is a song
  const networkremote::RequestChangeSong &request = msg.requestChangeSong();

  // Check if we need to change the playlist
  if (request.playlistId() != playlist_manager_->active_id()) {
    Q_EMIT SetActivePlaylist(request.playlistId());
  }

  switch (doubleclick_playlist_addmode_) {
    case BehaviourSettings::PlaylistAddBehaviour::Play:{
      Q_EMIT PlayAt(request.songIndex(), false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Maybe, false, false);
      break;
    }
    case BehaviourSettings::PlaylistAddBehaviour::Enqueue:{
      Q_EMIT Enqueue(request.playlistId(), request.songIndex());
      if (player_->GetState() != EngineBase::State::Playing) {
        Q_EMIT PlayAt(request.songIndex(), false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Maybe, false, false);
      }
      break;
    }
  }

}

void IncomingDataParser::ParseSetRepeatMode(const networkremote::Repeat &repeat) {

  switch (repeat.repeatMode()) {
    case networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Off:
      Q_EMIT SetRepeatMode(PlaylistSequence::RepeatMode::Off);
      break;
    case networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Track:
      Q_EMIT SetRepeatMode(PlaylistSequence::RepeatMode::Track);
      break;
    case networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Album:
      Q_EMIT SetRepeatMode(PlaylistSequence::RepeatMode::Album);
      break;
    case networkremote::RepeatModeGadget::RepeatMode::RepeatMode_Playlist:
      Q_EMIT SetRepeatMode(PlaylistSequence::RepeatMode::Playlist);
      break;
    default:
      break;
  }

}

void IncomingDataParser::ParseSetShuffleMode(const networkremote::Shuffle &shuffle) {

  switch (shuffle.shuffleMode()) {
    case networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_Off:
      Q_EMIT SetShuffleMode(PlaylistSequence::ShuffleMode::Off);
      break;
    case networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_All:
      Q_EMIT SetShuffleMode(PlaylistSequence::ShuffleMode::All);
      break;
    case networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_InsideAlbum:
      Q_EMIT SetShuffleMode(PlaylistSequence::ShuffleMode::InsideAlbum);
      break;
    case networkremote::ShuffleModeGadget::ShuffleMode::ShuffleMode_Albums:
      Q_EMIT SetShuffleMode(PlaylistSequence::ShuffleMode::Albums);
      break;
    default:
      break;
  }

}

void IncomingDataParser::ParseInsertUrls(const networkremote::Message &msg) {

  const networkremote::RequestInsertUrls &request = msg.requestInsertUrls();
  int playlist_id = request.playlistId();

  // Insert plain urls without metadata
  if (!request.urls().empty()) {
    QList<QUrl> urls;
    for (auto it = request.urls().begin(); it != request.urls().end(); ++it) {
      const QString s = *it;
      urls << QUrl(s);
    }

    if (request.hasNewPlaylistName()) {
      playlist_id = playlist_manager_->New(request.newPlaylistName());
    }

    // Insert the urls
    Q_EMIT InsertUrls(playlist_id, urls, request.position(), request.playNow(), request.enqueue());
  }

  // Add songs with metadata if present
  if (!request.songs().empty()) {
    SongList songs;
    for (int i = 0; i < request.songs().size(); i++) {
      songs << SongFromPbSongMetadata(request.songs().at(i));
    }

    // Create a new playlist if required and not already done above by InsertUrls
    if (request.hasNewPlaylistName() && playlist_id == request.playlistId()) {
      playlist_id = playlist_manager_->New(request.newPlaylistName());
    }

    Q_EMIT InsertSongs(request.playlistId(), songs, request.position(), request.playNow(), request.enqueue());
  }

}

void IncomingDataParser::ParseRemoveSongs(const networkremote::Message &msg) {

  const networkremote::RequestRemoveSongs &request = msg.requestRemoveSongs();

  QList<int> songs;
  songs.reserve(request.songs().size());
  for (int i = 0; i < request.songs().size(); i++) {
    songs.append(request.songs().at(i));
  }

  Q_EMIT RemoveSongs(request.playlistId(), songs);

}

void IncomingDataParser::ParseSendPlaylists(const networkremote::Message &msg) {

  if (!msg.hasRequestPlaylistSongs() || !msg.requestPlaylists().includeClosed()) {
    Q_EMIT SendAllActivePlaylists();
  }
  else {
    Q_EMIT SendAllPlaylists();
  }

}

void IncomingDataParser::ParseOpenPlaylist(const networkremote::Message &msg) {
  Q_EMIT Open(msg.requestOpenPlaylist().playlistId());
}

void IncomingDataParser::ParseClosePlaylist(const networkremote::Message &msg) {
  Q_EMIT Close(msg.requestClosePlaylist().playlistId());
}

void IncomingDataParser::ParseUpdatePlaylist(const networkremote::Message &msg) {

  const networkremote::RequestUpdatePlaylist &req_update = msg.requestUpdatePlaylist();
  if (req_update.hasCreateNewPlaylist() && req_update.createNewPlaylist()) {
    Q_EMIT New(req_update.hasNewPlaylistName() ? req_update.newPlaylistName() : u"New Playlist"_s);
    return;
  }
  if (req_update.hasClearPlaylist() && req_update.clearPlaylist()) {
    Q_EMIT Clear(req_update.playlistId());
    return;
  }
  if (req_update.hasNewPlaylistName() && !req_update.newPlaylistName().isEmpty()) {
    Q_EMIT Rename(req_update.playlistId(), req_update.newPlaylistName());
  }
  if (req_update.hasFavorite()) {
    Q_EMIT Favorite(req_update.playlistId(), req_update.favorite());
  }

}

void IncomingDataParser::ParseRateSong(const networkremote::Message &msg) {

  Q_EMIT RateCurrentSong(msg.requestRateSong().rating());

}

void IncomingDataParser::ParseAppendFilesToPlaylist(const networkremote::Message &msg) {

  if (files_root_folder_.isEmpty()) {
    qLog(Warning) << "Remote root dir is not set although receiving APPEND_FILES request...";
    return;
  }
  QDir root_dir(files_root_folder_);
  if (!root_dir.exists()) {
    qLog(Warning) << "Remote root dir doesn't exist...";
    return;
  }

  const networkremote::RequestAppendFiles &req_append = msg.requestAppendFiles();
  QString relative_path = req_append.relativePath();
  if (relative_path.startsWith("/"_L1)) relative_path.remove(0, 1);

  QFileInfo fi_folder(root_dir, relative_path);
  if (!fi_folder.exists()) {
    qLog(Warning) << "Remote relative path " << relative_path << " doesn't exist...";
    return;
  }
  else if (!fi_folder.isDir()) {
    qLog(Warning) << "Remote relative path " << relative_path << " is not a directory...";
    return;
  }
  else if (root_dir.relativeFilePath(fi_folder.absoluteFilePath()).startsWith("../"_L1)) {
    qLog(Warning) << "Remote relative path " << relative_path << " should not be accessed...";
    return;
  }

  QList<QUrl> urls;
  QDir dir(fi_folder.absoluteFilePath());
  for (const auto &file : req_append.files()) {
    QFileInfo fi(dir, file);
    if (fi.exists()) urls << QUrl::fromLocalFile(fi.canonicalFilePath());
  }
  if (!urls.isEmpty()) {
    MimeData *data = new MimeData;
    data->setUrls(urls);
    if (req_append.hasPlayNow()) {
      data->play_now_ = req_append.playNow();
    }
    if (req_append.hasClearFirst()) {
      data->clear_first_ = req_append.clearFirst();
    }
    if (req_append.hasNewPlaylistName()) {
      QString playlist_name = req_append.newPlaylistName();
      if (!playlist_name.isEmpty()) {
        data->open_in_new_playlist_ = true;
        data->name_for_new_playlist_ = playlist_name;
      }
    }
    else if (req_append.hasPlaylistId()) {
      // If playing we will drop the files in another playlist
      if (player_->GetState() == EngineBase::State::Playing) {
        data->playlist_id_ = req_append.playlistId();
      }
      else {
        // As we may play the song, we change the current playlist
        Q_EMIT SetCurrentPlaylist(req_append.playlistId());
      }
    }
    Q_EMIT AddToPlaylistSignal(data);
  }

}
