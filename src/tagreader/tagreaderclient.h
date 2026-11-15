/*
 * Strawberry Music Player
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERCLIENT_H
#define TAGREADERCLIENT_H

#include "config.h"

#include <atomic>
#include <functional>

#include <QObject>
#include <QList>
#include <QQueue>
#include <QString>
#include <QImage>
#include <QMutex>
#include <QReadWriteLock>

#include "core/song.h"

#include "tagreadertaglib.h"
#include "tagreadergme.h"
#include "tagreaderrequest.h"
#include "tagreaderresult.h"
#include "tagreaderreply.h"
#include "tagreaderreadfilereply.h"
#include "tagreaderreadstreamreply.h"
#include "tagreaderloadcoverdatareply.h"
#include "tagreaderloadcoverimagereply.h"
#include "savetagsoptions.h"
#include "savetagcoverdata.h"
#include "tagid3v2version.h"

class QThread;
class Song;

class TagReaderClient : public QObject {
  Q_OBJECT

 public:
  explicit TagReaderClient(QObject *parent = nullptr);

  void Start();
  // Stops processing the requests reading files when the application starts exiting, the requests writing to files are still processed until the tagreader exits.
  void AbortReads();
  void ExitAsync();

  using SaveOption = SaveTagsOption;
  using SaveOptions = SaveTagsOptions;

  bool IsMediaFileBlocking(const QString &filename) const;
  [[nodiscard]] TagReaderReplyPtr IsMediaFileAsync(const QString &filename);

  TagReaderResult ReadFileBlocking(const QString &filename, Song *song);
  [[nodiscard]] TagReaderReadFileReplyPtr ReadFileAsync(const QString &filename, const Song &song = Song());

#ifdef HAVE_STREAMTAGREADER
  TagReaderResult ReadStreamBlocking(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token, Song *song);
  [[nodiscard]] TagReaderReadStreamReplyPtr ReadStreamAsync(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token);
#endif

  TagReaderResult WriteFileBlocking(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options = SaveTagsOption::Tags, const SaveTagCoverData &save_tag_cover_data = SaveTagCoverData(), const TagID3v2Version tag_id3v2_version = TagID3v2Version::Default);
  [[nodiscard]] TagReaderReplyPtr WriteFileAsync(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options = SaveTagsOption::Tags, const SaveTagCoverData &save_tag_cover_data = SaveTagCoverData(), const TagID3v2Version tag_id3v2_version = TagID3v2Version::Default);
  // Reads the tags from the file, changes them with update_song, and writes them, so only the changed tags are written, the other tags are kept as they are in the file.
  // update_song is called in the tagreader thread, it returns false when the tags can't be changed, then the file is not written.
  // The file is read when the request is processed, after the requests enqueued before, and the request is processed when exiting like the other writes.
  [[nodiscard]] TagReaderReplyPtr UpdateFileAsync(const QString &filename, const std::function<bool(Song *song)> &update_song);

  TagReaderResult LoadCoverDataBlocking(const QString &filename, QByteArray &data);
  TagReaderResult LoadCoverImageBlocking(const QString &filename, QImage &image);
  [[nodiscard]] TagReaderLoadCoverDataReplyPtr LoadCoverDataAsync(const QString &filename);
  [[nodiscard]] TagReaderLoadCoverImageReplyPtr LoadCoverImageAsync(const QString &filename);

  TagReaderResult SaveCoverBlocking(const QString &filename, const SaveTagCoverData &save_tag_cover_data);
  [[nodiscard]] TagReaderReplyPtr SaveCoverAsync(const QString &filename, const SaveTagCoverData &save_tag_cover_data);

  [[nodiscard]] TagReaderReplyPtr SaveSongPlaycountAsync(const QString &filename, const uint playcount);
  TagReaderResult SaveSongPlaycountBlocking(const QString &filename, const uint playcount);

  [[nodiscard]] TagReaderReplyPtr SaveSongRatingAsync(const QString &filename, const float rating);
  TagReaderResult SaveSongRatingBlocking(const QString &filename, const float rating);

  [[nodiscard]] TagReaderReplyPtr SaveSongLyricsAsync(const QString &filename, const QString &lyrics);
  TagReaderResult SaveSongLyricsBlocking(const QString &filename, const QString &lyrics);

  [[nodiscard]] TagReaderReplyPtr SaveSongSyncedLyricsAsync(const QString &filename, const QString &synced_lyrics);
  TagReaderResult SaveSongSyncedLyricsBlocking(const QString &filename, const QString &synced_lyrics);

 private:
  bool HaveRequests() const;
  void EnqueueRequest(TagReaderRequestPtr request);
  TagReaderRequestPtr DequeueRequest();
  void ProcessRequestsAsync();
  void ProcessRequest(TagReaderRequestPtr request);
  static bool IsWriteRequest(TagReaderRequestPtr request);

 Q_SIGNALS:
  void ExitFinished();

 private Q_SLOTS:
  void Exit();
  void ProcessRequests();

 public Q_SLOTS:
  void SaveSongsPlaycountAsync(const SongList &songs);
  void SaveSongsRatingAsync(const SongList &songs);

 private:
  QThread *original_thread_;
  QQueue<TagReaderRequestPtr> requests_;
  mutable QMutex mutex_requests_;
  // Locked for writing while writing tags, and for reading while reading files, the blocking functions are also called from other threads than the queued requests.
  // Two writes to the same file at once can corrupt it, and on Windows a file which is open for reading can't be opened for writing, and the other way around, so a read at the same time makes the write fail.
  mutable QReadWriteLock lock_files_;
  TagReaderTagLib tagreader_;
  TagReaderGME gmereader_;
  // Set when exiting, the requests reading files are dropped, but the requests writing to files are still processed, so changes to the tags are not lost.
  std::atomic<bool> abort_;
  // Set when the tagreader exited and moved back to the original thread.
  std::atomic<bool> exited_;
  std::atomic<bool> processing_;
};

#endif  // TAGREADERCLIENT_H
