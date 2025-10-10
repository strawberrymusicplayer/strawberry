/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "includes/shared_ptr.h"
#include "core/song.h"

class TaskManager;
class UrlHandlers;
class Player;
class TagReaderClient;
class SongLoader;
class CollectionBackendInterface;
class Playlist;

class SongLoaderInserter : public QObject {
  Q_OBJECT

 public:
  explicit SongLoaderInserter(const SharedPtr<TaskManager> task_manager,
                              const SharedPtr<TagReaderClient> tagreader_client,
                              const SharedPtr<UrlHandlers> url_handlers,
                              const SharedPtr<CollectionBackendInterface> collection_backend,
                              QObject *parent = nullptr);

  ~SongLoaderInserter() override;

  void Load(Playlist *destination, const int row, const bool play_now, const bool enqueue, const bool enqueue_next, const QList<QUrl> &urls);
  void LoadAudioCD(Playlist *destination, const int row, const bool play_now, const bool enqueue, const bool enqueue_next);

 Q_SIGNALS:
  void Error(const QString &message);
  void PreloadFinished();
  void EffectiveLoadFinished(const SongList &songs);

 private Q_SLOTS:
  void DestinationDestroyed();
  void AudioCDTracksLoadedSlot();
  void AudioCDTracksUpdatedSlot();
  void AudioCDLoadingFinishedSlot(const bool success);
  void InsertSongs();

 private:
  void AsyncLoad();

 private:
  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<UrlHandlers> url_handlers_;
  const SharedPtr<CollectionBackendInterface> collection_backend_;

  Playlist *destination_;
  int row_;
  bool play_now_;
  bool enqueue_;
  bool enqueue_next_;

  SongList songs_;
  QString playlist_name_;

  QList<SongLoader*> pending_;
};

#endif  // SONGLOADERINSERTER_H
