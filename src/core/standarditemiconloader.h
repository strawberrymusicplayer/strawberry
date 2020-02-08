/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef STANDARDITEMICONLOADER_H
#define STANDARDITEMICONLOADER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QImage>

#include "covermanager/albumcoverloaderoptions.h"

class QAbstractItemModel;
class QStandardItem;

class Song;
class AlbumCoverLoader;

class QModelIndex;

// Uses an AlbumCoverLoader to asynchronously load and set an icon on a QStandardItem.
class StandardItemIconLoader : public QObject {
  Q_OBJECT

 public:
  StandardItemIconLoader(AlbumCoverLoader *cover_loader, QObject *parent = nullptr);

  AlbumCoverLoaderOptions *options() { return &cover_options_; }

  void SetModel(QAbstractItemModel *model);

  void LoadIcon(const QUrl &art_automatic, const QUrl &art_manual, QStandardItem *for_item);
  void LoadIcon(const Song &song, QStandardItem *for_item);

private slots:
  void ImageLoaded(const quint64 id, const QUrl &cover_url, const QImage &image);
  void RowsAboutToBeRemoved(const QModelIndex &parent, int begin, int end);
  void ModelReset();

private:
  AlbumCoverLoader *cover_loader_;
  AlbumCoverLoaderOptions cover_options_;

  QAbstractItemModel *model_;

  QMap<quint64, QStandardItem*> pending_covers_;
};

#endif  // STANDARDITEMICONLOADER_H
