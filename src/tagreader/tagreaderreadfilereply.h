/*
 * Strawberry Music Player
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

#ifndef TAGREADERREADFILEREPLY_H
#define TAGREADERREADFILEREPLY_H

#include <QString>

#include "core/shared_ptr.h"
#include "core/song.h"
#include "tagreaderreply.h"
#include "tagreaderresult.h"

class TagReaderReadFileReply : public TagReaderReply {
  Q_OBJECT

 public:
  explicit TagReaderReadFileReply(const QString &_filename, QObject *parent = nullptr);

  void Finish() override;

  Song song() const { return song_; }
  void set_song(const Song &song) { song_ = song; }

 Q_SIGNALS:
  void Finished(const QString &filename, const Song &song, const TagReaderResult &result);

 private:
  Song song_;
};

using TagReaderReadFileReplyPtr = SharedPtr<TagReaderReadFileReply>;

#endif  // TAGREADERREADFILEREPLY_H
