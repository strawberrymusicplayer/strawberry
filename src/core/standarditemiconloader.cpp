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
#include <QMap>
#include <QSet>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QIcon>

#include "covermanager/albumcoverloader.h"
#include "standarditemiconloader.h"

StandardItemIconLoader::StandardItemIconLoader(AlbumCoverLoader *cover_loader, QObject *parent)
  : QObject(parent),
    cover_loader_(cover_loader),
    model_(nullptr) {

  cover_options_.desired_height_ = 16;

  connect(cover_loader_, SIGNAL(ImageLoaded(quint64, QUrl, QImage)), SLOT(ImageLoaded(quint64, QUrl, QImage)));
}

void StandardItemIconLoader::SetModel(QAbstractItemModel *model) {

  if (model_) {
    disconnect(model_, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)), this, SLOT(RowsAboutToBeRemoved(QModelIndex, int, int)));
  }

  model_ = model;

  connect(model_, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), SLOT(RowsAboutToBeRemoved(QModelIndex,int,int)));
  connect(model_, SIGNAL(modelAboutToBeReset()), SLOT(ModelReset()));

}

void StandardItemIconLoader::LoadIcon(const QUrl &art_automatic, const QUrl &art_manual, QStandardItem *for_item) {

  const quint64 id = cover_loader_->LoadImageAsync(cover_options_, art_automatic, art_manual);
  pending_covers_[id] = for_item;

}

void StandardItemIconLoader::LoadIcon(const Song &song, QStandardItem *for_item) {
  const quint64 id = cover_loader_->LoadImageAsync(cover_options_, song);
  pending_covers_[id] = for_item;
}

void StandardItemIconLoader::RowsAboutToBeRemoved(const QModelIndex &parent, int begin, int end) {

  for (QMap<quint64, QStandardItem*>::iterator it = pending_covers_.begin() ; it != pending_covers_.end() ; ) {
    const QStandardItem *item = it.value();
    const QStandardItem *item_parent = item->parent();

    if (item_parent && item_parent->index() == parent && item->index().row() >= begin && item->index().row() <= end) {
      cover_loader_->CancelTask(it.key());
      it = pending_covers_.erase(it);
    }
    else {
      ++ it;
    }
  }

}

void StandardItemIconLoader::ModelReset() {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  cover_loader_->CancelTasks(QSet<quint64>(pending_covers_.begin(), pending_covers_.end()));
#else
  cover_loader_->CancelTasks(QSet<quint64>::fromList(pending_covers_.keys()));
#endif
  pending_covers_.clear();

}

void StandardItemIconLoader::ImageLoaded(const quint64 id, const QUrl &cover_url, const QImage &image) {

  Q_UNUSED(cover_url);

  QStandardItem *item = pending_covers_.take(id);
  if (!item) return;

  if (!image.isNull()) {
    item->setIcon(QIcon(QPixmap::fromImage(image)));
  }

}
