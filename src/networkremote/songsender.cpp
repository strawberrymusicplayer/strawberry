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

#include "songsender.h"

#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "constants/networkremotesettingsconstants.h"
#include "constants/networkremoteconstants.h"
#include "core/logging.h"
#include "core/player.h"
#include "collection/collectionbackend.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlist.h"
#include "networkremote.h"
#include "outgoingdatacreator.h"
#include "networkremoteclient.h"
#include "utilities/randutils.h"
#include "utilities/cryptutils.h"

using namespace Qt::Literals::StringLiterals;
using namespace NetworkRemoteSettingsConstants;
using namespace NetworkRemoteConstants;

SongSender::SongSender(const SharedPtr<Player> player,
                       const SharedPtr<CollectionBackend> collection_backend,
                       const SharedPtr<PlaylistManager> playlist_manager,
                       NetworkRemoteClient *client,
                       QObject *parent)
    : QObject(parent),
      player_(player),
      collection_backend_(collection_backend),
      playlist_manager_(playlist_manager),
      client_(client),
      transcoder_(new Transcoder(this, QLatin1String(kTranscoderSettingPostfix))) {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  transcode_lossless_files_ = s.value("convert_lossless", false).toBool();

  // Load preset
  QString last_output_format = s.value("last_output_format", u"audio/x-vorbis"_s).toString();
  QList<TranscoderPreset> presets = transcoder_->GetAllPresets();
  for (int i = 0; i < presets.count(); ++i) {
    if (last_output_format == presets.at(i).codec_mimetype_) {
      transcoder_preset_ = presets.at(i);
      break;
    }
  }

  qLog(Debug) << "Transcoder preset" << transcoder_preset_.codec_mimetype_;

  QObject::connect(transcoder_, &Transcoder::JobComplete, this, &SongSender::TranscodeJobComplete);
  QObject::connect(transcoder_, &Transcoder::AllJobsComplete, this, &SongSender::StartTransfer);

  total_transcode_ = 0;

}

SongSender::~SongSender() {

  QObject::disconnect(transcoder_, &Transcoder::JobComplete, this, &SongSender::TranscodeJobComplete);
  QObject::disconnect(transcoder_, &Transcoder::AllJobsComplete, this, &SongSender::StartTransfer);

  transcoder_->Cancel();

}

void SongSender::SendSongs(const networkremote::RequestDownloadSongs &request) {

  Song current_song;
  if (player_->GetCurrentItem()) {
    current_song = player_->GetCurrentItem()->Metadata();
  }

  switch (request.downloadItem()) {
    case networkremote::DownloadItemGadget::DownloadItem::CurrentItem:{
      if (current_song.is_valid()) {
        const DownloadItem item(current_song, 1, 1);
        download_queue_.append(item);
      }
      break;
    }
    case networkremote::DownloadItemGadget::DownloadItem::ItemAlbum:
      if (current_song.is_valid()) {
        SendAlbum(current_song);
      }
      break;
    case networkremote::DownloadItemGadget::DownloadItem::APlaylist:
      SendPlaylist(request);
      break;
    case networkremote::DownloadItemGadget::DownloadItem::Urls:
      SendUrls(request);
      break;
    default:
      break;
  }

  if (transcode_lossless_files_) {
    TranscodeLosslessFiles();
  }
  else {
    StartTransfer();
  }

}

void SongSender::TranscodeLosslessFiles() {

  for (const DownloadItem &item : std::as_const(download_queue_)) {
    // Check only lossless files
    if (!item.song_.IsFileLossless()) continue;

    // Add the file to the transcoder
    const QString local_file = item.song_.url().toLocalFile();

    qLog(Debug) << "Transcoding" << local_file;

    transcoder_->AddJob(local_file, transcoder_preset_, Utilities::GetRandomStringWithCharsAndNumbers(20));

    total_transcode_++;
  }

  if (total_transcode_ > 0) {
    transcoder_->Start();
    SendTranscoderStatus();
  }
  else {
    StartTransfer();
  }

}

void SongSender::TranscodeJobComplete(const QString &input, const QString &output, const bool success) {

  qLog(Debug) << input << "transcoded to" << output << success;

  // If it wasn't successful send original file
  if (success) {
    transcoder_map_.insert(input, output);
  }

  SendTranscoderStatus();

}

void SongSender::SendTranscoderStatus() {

  // Send a message to the remote that we are converting files
  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::TRANSCODING_FILES);

  networkremote::ResponseTranscoderStatus status = msg.responseTranscoderStatus();
  status.setProcessed(static_cast<int>(transcoder_map_.count()));
  status.setTotal(total_transcode_);

  client_->SendData(&msg);

}

void SongSender::StartTransfer() {

  total_transcode_ = 0;

  // Send total file size & file count
  SendTotalFileSize();

  // Send first file
  OfferNextSong();

}

void SongSender::SendTotalFileSize() {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::DOWNLOAD_TOTAL_SIZE);

  networkremote::ResponseDownloadTotalSize response = msg.responseDownloadTotalSize();

  response.setFileCount(download_queue_.size());

  qint64 total = 0;
  for (const DownloadItem &item : std::as_const(download_queue_)) {
    QString local_file = item.song_.url().toLocalFile();
    const bool is_transcoded = transcoder_map_.contains(local_file);

    if (is_transcoded) {
      local_file = transcoder_map_.value(local_file);
    }

    total += QFileInfo(local_file).size();

  }

  response.setTotalSize(total);

  client_->SendData(&msg);

}

void SongSender::OfferNextSong() {

  networkremote::Message msg;

  if (download_queue_.isEmpty()) {
    msg.setType(networkremote::MsgTypeGadget::MsgType::DOWNLOAD_QUEUE_EMPTY);
  }
  else {
    // Get the item and send the single song
    const DownloadItem item = download_queue_.head();

    msg.setType(networkremote::MsgTypeGadget::MsgType::SONG_OFFER_FILE_CHUNK);
    networkremote::ResponseSongFileChunk chunk = msg.responseSongFileChunk();

    // Open the file
    QFile file(item.song_.url().toLocalFile());

    // Song offer is chunk no 0
    chunk.setChunkCount(0);
    chunk.setChunkNumber(0);
    chunk.setFileCount(item.song_count_);
    chunk.setFileNumber(item.song_number_);
    chunk.setSize(file.size());
    chunk.setSongMetadata(OutgoingDataCreator::PbSongMetadataFromSong(-1, item.song_));
    msg.setResponseSongFileChunk(chunk);
  }

  client_->SendData(&msg);

}

void SongSender::ResponseSongOffer(const bool accepted) {

  if (download_queue_.isEmpty()) return;

  // Get the item and send the single song
  DownloadItem item = download_queue_.dequeue();
  if (accepted) SendSingleSong(item);

  // And offer the next song
  OfferNextSong();

}

void SongSender::SendSingleSong(const DownloadItem &download_item) {

  if (!download_item.song_.url().isLocalFile()) return;

  QString local_file = download_item.song_.url().toLocalFile();
  bool is_transcoded = transcoder_map_.contains(local_file);

  if (is_transcoded) {
    local_file = transcoder_map_.take(local_file);
  }

  // Open the file
  QFile file(local_file);

  // Get sha1 for file
  QByteArray sha1 = Utilities::Sha1File(file).toHex();
  qLog(Debug) << "sha1 for file" << local_file << "=" << sha1;

  file.open(QIODevice::ReadOnly);

  QByteArray data;
  networkremote::Message msg;
  networkremote::ResponseSongFileChunk chunk = msg.responseSongFileChunk();
  msg.setType(networkremote::MsgTypeGadget::MsgType::SONG_OFFER_FILE_CHUNK);

  // Calculate the number of chunks
  int chunk_count = qRound((static_cast<quint32>(file.size()) / kFileChunkSize) + 0.5);
  int chunk_number = 1;

  while (!file.atEnd()) {
    // Read file chunk
    data = file.read(kFileChunkSize);

    // Set chunk data
    chunk.setChunkCount(chunk_count);
    chunk.setChunkNumber(chunk_number);
    chunk.setFileCount(download_item.song_count_);
    chunk.setFileNumber(download_item.song_number_);
    chunk.setSize(file.size());
    chunk.setData(data);
    chunk.setFileHash(sha1);

    // On the first chunk send the metadata, so the client knows what file it receives.
    if (chunk_number == 1) {
      const int i = playlist_manager_->active()->current_row();
      networkremote::SongMetadata song_metadata = OutgoingDataCreator::PbSongMetadataFromSong(i, download_item.song_);

      // If the file was transcoded, we have to change the filename and filesize
      if (is_transcoded) {
        song_metadata.setFileSize(file.size());
        QString basefilename = download_item.song_.basefilename();
        QFileInfo info(basefilename);
        basefilename.replace(u'.' + info.suffix(), u'.' + transcoder_preset_.extension_);
        song_metadata.setFilename(basefilename);
      }
    }

    // Send data directly to the client
    client_->SendData(&msg);

    // Clear working data
    chunk = networkremote::ResponseSongFileChunk();
    data.clear();

    chunk_number++;
  }

  // If the file was transcoded, delete the temporary one
  if (is_transcoded) {
    file.remove();
  }
  else {
    file.close();
  }

}

void SongSender::SendAlbum(const Song &album_song) {

  if (!album_song.url().isLocalFile()) return;

  const SongList songs = collection_backend_->GetSongsByAlbum(album_song.album());

  for (const Song &song : songs) {
    const DownloadItem item(song, static_cast<int>(songs.indexOf(song)) + 1, static_cast<int>(songs.size()));
    download_queue_.append(item);
  }

}

void SongSender::SendPlaylist(const networkremote::RequestDownloadSongs &request) {

  const int playlist_id = request.playlistId();
  Playlist *playlist = playlist_manager_->playlist(playlist_id);
  if (!playlist) {
    qLog(Info) << "Could not find playlist with id = " << playlist_id;
    return;
  }
  const SongList song_list = playlist->GetAllSongs();

  QList<int> requested_ids;
  requested_ids.reserve(request.songsIds().count());
  for (auto song_id : request.songsIds()) {
    requested_ids << song_id;
  }

  // Count the local songs
  int count = 0;
  for (const Song &song : song_list) {
    if (song.url().isLocalFile() && (requested_ids.isEmpty() || requested_ids.contains(song.id()))) {
      ++count;
    }
  }

  for (const Song &song : song_list) {
    if (song.url().isLocalFile() && (requested_ids.isEmpty() || requested_ids.contains(song.id()))) {
      DownloadItem item(song, static_cast<int>(song_list.indexOf(song)) + 1, count);
      download_queue_.append(item);
    }
  }

}

void SongSender::SendUrls(const networkremote::RequestDownloadSongs &request) {

  SongList songs;

  // First gather all valid songs
  if (!request.relativePath().isEmpty()) {
    // Security checks, cf OutgoingDataCreator::SendListFiles
    const QString &files_root_folder = client_->files_root_folder();
    if (files_root_folder.isEmpty()) return;
    QDir root_dir(files_root_folder);
    QString relative_path = request.relativePath();
    if (!root_dir.exists() || relative_path.startsWith(".."_L1) || relative_path.startsWith("./.."_L1))
      return;

    if (relative_path.startsWith(u'/')) relative_path.remove(0, 1);

    QFileInfo fi_folder(root_dir, relative_path);
    if (!fi_folder.exists() || !fi_folder.isDir() || root_dir.relativeFilePath(fi_folder.absoluteFilePath()).startsWith(u"../"_s)) {
      return;
    }

    QDir dir(fi_folder.absoluteFilePath());
    const QStringList &files_music_extensions = client_->files_music_extensions();
    for (const QString &s : request.urls()) {
      QFileInfo fi(dir, s);
      if (fi.exists() && fi.isFile() && files_music_extensions.contains(fi.suffix())) {
        Song song;
        song.set_basefilename(fi.fileName());
        song.set_filesize(fi.size());
        song.set_url(QUrl::fromLocalFile(fi.absoluteFilePath()));
        song.set_valid(true);
        songs.append(song);
      }
    }
  }
  else {
    for (const QString &url_str : request.urls()) {
      const QUrl url(url_str);
      Song song = collection_backend_->GetSongByUrl(url);
      if (song.is_valid() && song.url().isLocalFile()) {
        songs.append(song);
      }
    }
  }

  for (const Song &song : songs) {
    DownloadItem item(song, static_cast<int>(songs.indexOf(song)) + 1, static_cast<int>(songs.count()));
    download_queue_.append(item);
  }

}
