/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QList>
#include <QQueue>
#include <QString>
#include <QImage>
#include <QMutex>

#include "includes/mutex_protected.h"
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

class QThread;
class Song;

class TagReaderClient : public QObject {
  Q_OBJECT

 public:
  explicit TagReaderClient(QObject *parent = nullptr);

  static TagReaderClient *Instance() { return sInstance; }

  void Start();
  void ExitAsync();

  using SaveOption = SaveTagsOption;
  using SaveOptions = SaveTagsOptions;

  bool IsMediaFileBlocking(const QString &filename) const;
  [[nodiscard]] TagReaderReplyPtr IsMediaFileAsync(const QString &filename);

  TagReaderResult ReadFileBlocking(const QString &filename, Song *song);
  [[nodiscard]] TagReaderReadFileReplyPtr ReadFileAsync(const QString &filename);

#ifdef HAVE_STREAMTAGREADER
  TagReaderResult ReadStreamBlocking(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token, Song *song);
  [[nodiscard]] TagReaderReadStreamReplyPtr ReadStreamAsync(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token);
#endif

  TagReaderResult WriteFileBlocking(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options = SaveTagsOption::Tags, const SaveTagCoverData &save_tag_cover_data = SaveTagCoverData());
  [[nodiscard]] TagReaderReplyPtr WriteFileAsync(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options = SaveTagsOption::Tags, const SaveTagCoverData &save_tag_cover_data = SaveTagCoverData());

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

 private:
  bool HaveRequests() const;
  void EnqueueRequest(TagReaderRequestPtr request);
  TagReaderRequestPtr DequeueRequest();
  void ProcessRequestsAsync();
  void ProcessRequest(TagReaderRequestPtr request);

 Q_SIGNALS:
  void ExitFinished();

 private Q_SLOTS:
  void Exit();
  void ProcessRequests();

 public Q_SLOTS:
  void SaveSongsPlaycountAsync(const SongList &songs);
  void SaveSongsRatingAsync(const SongList &songs);

 private:
  static TagReaderClient *sInstance;

  QThread *original_thread_;
  QQueue<TagReaderRequestPtr> requests_;
  mutable QMutex mutex_requests_;
  TagReaderTagLib tagreader_;
  TagReaderGME gmereader_;
  mutex_protected<bool> abort_;
  mutex_protected<bool> processing_;
};

#endif  // TAGREADERCLIENT_H
