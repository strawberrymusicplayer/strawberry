/*
 * Strawberry Music Player
 * Copyright 2024-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERWRITEFILEREQUEST_H
#define TAGREADERWRITEFILEREQUEST_H

#include <functional>

#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tagreaderrequest.h"
#include "savetagsoptions.h"
#include "savetagcoverdata.h"
#include "tagid3v2version.h"

using std::make_shared;

class TagReaderWriteFileRequest : public TagReaderRequest {
 public:
  explicit TagReaderWriteFileRequest(const QString &_filename);

  static SharedPtr<TagReaderWriteFileRequest> Create(const QString &filename) { return make_shared<TagReaderWriteFileRequest>(filename); }

  SaveTagsOptions save_tags_options;
  Song song;
  // When set, the song is read from the file and updated with this instead of writing the song above, see TagReaderClient::UpdateFileAsync().
  std::function<bool(Song *song)> update_song;
  SaveTagCoverData save_tag_cover_data;
  TagID3v2Version tag_id3v2_version;
};

using TagReaderWriteFileRequestPtr = SharedPtr<TagReaderWriteFileRequest>;

#endif  // TAGREADERWRITEFILEREQUEST_H
