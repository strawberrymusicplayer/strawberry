/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef SONGLOADERINSERTER_H
#define SONGLOADERINSERTER_H

#include "config.h"


#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>

#include "core/song.h"

class Player;
class SongLoader;
class TaskManager;
class CollectionBackendInterface;
class Playlist;

class SongLoaderInserter : public QObject {
  Q_OBJECT
 public:
  SongLoaderInserter(TaskManager *task_manager, CollectionBackendInterface *collection, const Player *player);
  ~SongLoaderInserter();

  void Load(Playlist *destination, int row, bool play_now, bool enqueue, bool enqueue_next, const QList<QUrl> &urls);
  void LoadAudioCD(Playlist *destination, int row, bool play_now, bool enqueue, bool enqueue_next);

 signals:
  void Error(const QString &message);
  void PreloadFinished();
  void EffectiveLoadFinished(const SongList &songs);

 private slots:
  void DestinationDestroyed();
  void AudioCDTracksLoaded(SongLoader *loader);
  void AudioCDTagsLoaded(bool success);
  void InsertSongs();

 private:
  void AsyncLoad();

 private:
  TaskManager *task_manager_;

  Playlist *destination_;
  int row_;
  bool play_now_;
  bool enqueue_;
  bool enqueue_next_;

  SongList songs_;

  QList<SongLoader*> pending_;
  CollectionBackendInterface *collection_;
  const Player *player_;

};

#endif  // SONGLOADERINSERTER_H
