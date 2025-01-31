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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QStandardItem>
#include <QMap>
#include <QSet>
#include <QPixmap>
#include <QIcon>

#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "standarditemiconloader.h"

StandardItemIconLoader::StandardItemIconLoader(AlbumCoverLoader *cover_loader, QObject *parent)
    : QObject(parent),
      cover_loader_(cover_loader),
      model_(nullptr) {

  QObject::connect(cover_loader_, &AlbumCoverLoader::AlbumCoverLoaded, this, &StandardItemIconLoader::AlbumCoverLoaded);

}

void StandardItemIconLoader::SetModel(QAbstractItemModel *model) {

  if (model_) {
    QObject::disconnect(model_, &QAbstractItemModel::rowsAboutToBeRemoved, this, &StandardItemIconLoader::RowsAboutToBeRemoved);
  }

  model_ = model;

  QObject::connect(model_, &QAbstractItemModel::rowsAboutToBeRemoved, this, &StandardItemIconLoader::RowsAboutToBeRemoved);
  QObject::connect(model_, &QAbstractItemModel::modelAboutToBeReset, this, &StandardItemIconLoader::ModelReset);

}

void StandardItemIconLoader::LoadAlbumCover(const QUrl &art_automatic, const QUrl &art_manual, QStandardItem *for_item) {

  AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::ScaledImage);
  cover_options.desired_scaled_size = QSize(16, 16);
  const quint64 id = cover_loader_->LoadImageAsync(cover_options, false, art_automatic, art_manual, false);
  pending_covers_.insert(id, for_item);

}

void StandardItemIconLoader::LoadAlbumCover(const Song &song, QStandardItem *for_item) {

  AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::ScaledImage);
  cover_options.desired_scaled_size = QSize(16, 16);
  const quint64 id = cover_loader_->LoadImageAsync(cover_options, song);
  pending_covers_.insert(id, for_item);

}

void StandardItemIconLoader::RowsAboutToBeRemoved(const QModelIndex &parent, int begin, int end) {

  for (QMap<quint64, QStandardItem*>::iterator it = pending_covers_.begin(); it != pending_covers_.end();) {
    const QStandardItem *item = it.value();
    const QStandardItem *item_parent = item->parent();

    if (item_parent && item_parent->index() == parent && item->index().row() >= begin && item->index().row() <= end) {
      cover_loader_->CancelTask(it.key());
      it = pending_covers_.erase(it);
    }
    else {
      ++it;
    }
  }

}

void StandardItemIconLoader::ModelReset() {

  cover_loader_->CancelTasks(QSet<quint64>(pending_covers_.keyBegin(), pending_covers_.keyEnd()));
  pending_covers_.clear();

}

void StandardItemIconLoader::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!pending_covers_.contains(id)) return;

  QStandardItem *item = pending_covers_.take(id);
  if (!item) return;

  if (result.success && !result.image_scaled.isNull() && result.type != AlbumCoverLoaderResult::Type::Unset) {
    item->setIcon(QIcon(QPixmap::fromImage(result.image_scaled)));
  }

}
