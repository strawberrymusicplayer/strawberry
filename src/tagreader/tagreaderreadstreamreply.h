/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERREADSTREAMREPLY_H
#define TAGREADERREADSTREAMREPLY_H

#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "core/song.h"
#include "tagreaderreply.h"
#include "tagreaderresult.h"

class TagReaderReadStreamReply : public TagReaderReply {
  Q_OBJECT

 public:
  explicit TagReaderReadStreamReply(const QUrl &url, const QString &_filename, QObject *parent = nullptr);

  void Finish() override;

  Song song() const { return song_; }
  void set_song(const Song &song) { song_ = song; }

 Q_SIGNALS:
  void Finished(const QUrl &url, const Song &song, const TagReaderResult &result);

 private Q_SLOTS:
  void EmitFinished() override;

 private:
  Song song_;
  QUrl url_;
};

using TagReaderReadStreamReplyPtr = QSharedPointer<TagReaderReadStreamReply>;

#endif  // TAGREADERREADSTREAMREPLY_H
