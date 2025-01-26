/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERBASE_H
#define TAGREADERBASE_H

#include "config.h"

#include <QtGlobal>
#include <QByteArray>
#include <QString>

#include "core/song.h"

#include "tagreaderresult.h"
#include "savetagsoptions.h"
#include "savetagcoverdata.h"
#include "albumcovertagdata.h"

class TagReaderBase {
 public:
  explicit TagReaderBase();
  virtual ~TagReaderBase();

  virtual TagReaderResult IsMediaFile(const QString &filename) const = 0;

  virtual TagReaderResult ReadFile(const QString &filename, Song *song) const = 0;
#ifdef HAVE_STREAMTAGREADER
  virtual TagReaderResult ReadStream(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token, Song *song) const = 0;
#endif

  virtual TagReaderResult WriteFile(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options, const SaveTagCoverData &save_tag_cover_data) const = 0;

  virtual TagReaderResult LoadEmbeddedCover(const QString &filename, QByteArray &data) const = 0;
  virtual TagReaderResult SaveEmbeddedCover(const QString &filename, const SaveTagCoverData &save_tag_cover_data) const = 0;

  virtual TagReaderResult SaveSongPlaycount(const QString &filename, const uint playcount) const = 0;
  virtual TagReaderResult SaveSongRating(const QString &filename, const float rating) const = 0;

 protected:
  static float ConvertPOPMRating(const int POPM_rating);
  static int ConvertToPOPMRating(const float rating);

  static AlbumCoverTagData LoadAlbumCoverTagData(const QString &song_filename, const SaveTagCoverData &save_tag_cover_data);

  Q_DISABLE_COPY(TagReaderBase)
};

#endif  // TAGREADERBASE_H
