/* This file was part of Clementine.
   Copyright 2012, David Sansome <me@davidsansome.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QSettings>
#include <QItemDelegate>
#include <QList>
#include <QVector>
#include <QVariant>
#include <QByteArray>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QRect>

#include "core/application.h"
#include "playlist/playlist.h"
#include "playlist/playlistview.h"

#include "moodbaritemdelegate.h"
#include "moodbarloader.h"
#include "moodbarpipeline.h"
#include "moodbarrenderer.h"

#include "settings/moodbarsettingspage.h"

MoodbarItemDelegate::Data::Data() : state_(State_None) {}

MoodbarItemDelegate::MoodbarItemDelegate(Application *app, PlaylistView *view, QObject *parent)
    : QItemDelegate(parent),
      app_(app),
      view_(view),
      enabled_(false),
      style_(MoodbarRenderer::Style_Normal) {

  QObject::connect(app_, &Application::SettingsChanged, this, &MoodbarItemDelegate::ReloadSettings);
  ReloadSettings();

}

void MoodbarItemDelegate::ReloadSettings() {

  QSettings s;
  s.beginGroup(MoodbarSettingsPage::kSettingsGroup);
  enabled_ = s.value("enabled", false).toBool();
  MoodbarRenderer::MoodbarStyle new_style = static_cast<MoodbarRenderer::MoodbarStyle>(s.value("style", MoodbarRenderer::Style_Normal).toInt());
  s.endGroup();

  if (!enabled_) {
    data_.clear();
  }

  if (new_style != style_) {
    style_ = new_style;
    ReloadAllColors();
  }

}

void MoodbarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  QPixmap pixmap;

  if (enabled_) {
    pixmap = const_cast<MoodbarItemDelegate*>(this)->PixmapForIndex(idx, option.rect.size());
  }

  drawBackground(painter, option, idx);

  if (!pixmap.isNull()) {
    // Make a little border for the moodbar
    const QRect moodbar_rect(option.rect.adjusted(1, 1, -1, -1));
    painter->drawPixmap(moodbar_rect, pixmap);
  }

}

QPixmap MoodbarItemDelegate::PixmapForIndex(const QModelIndex &idx, const QSize &size) {

  // Pixmaps are keyed off URL.
  const QUrl url(idx.sibling(idx.row(), Playlist::Column_Filename).data().toUrl());

  Data *data = data_[url];
  if (!data) {
    data = new Data;
    data_.insert(url, data);
  }

  data->indexes_.insert(idx);
  data->desired_size_ = size;

  switch (data->state_) {
    case Data::State_CannotLoad:
    case Data::State_LoadingData:
    case Data::State_LoadingColors:
    case Data::State_LoadingImage:
      return data->pixmap_;

    case Data::State_Loaded:
      // Is the pixmap the right size?
      if (data->pixmap_.size() != size) {
        StartLoadingImage(url, data);
      }

      return data->pixmap_;

    case Data::State_None:
      break;
  }

  // We have to start loading the data from scratch.
  StartLoadingData(url, data);

  return QPixmap();

}

void MoodbarItemDelegate::StartLoadingData(const QUrl &url, Data *data) {

  data->state_ = Data::State_LoadingData;

  // Load a mood file for this song and generate some colors from it
  QByteArray bytes;
  MoodbarPipeline *pipeline = nullptr;
  switch (app_->moodbar_loader()->Load(url, &bytes, &pipeline)) {
    case MoodbarLoader::CannotLoad:
      data->state_ = Data::State_CannotLoad;
      break;

    case MoodbarLoader::Loaded:
      // We got the data immediately.
      StartLoadingColors(url, bytes, data);
      break;

    case MoodbarLoader::WillLoadAsync:
      // Maybe in a little while.
      QObject::connect(pipeline, &MoodbarPipeline::Finished, this, [this, url, pipeline]() { DataLoaded(url, pipeline); });
      break;
  }

}

bool MoodbarItemDelegate::RemoveFromCacheIfIndexesInvalid(const QUrl &url, Data *data) {

  for (const QPersistentModelIndex &idx : data->indexes_) {
    if (idx.isValid()) {
      return false;
    }
  }

  data_.remove(url);
  return true;

}

void MoodbarItemDelegate::ReloadAllColors() {

  for (const QUrl &url : data_.keys()) {
    Data *data = data_[url];

    if (data->state_ == Data::State_Loaded) {
      StartLoadingData(url, data);
    }
  }

}

void MoodbarItemDelegate::DataLoaded(const QUrl &url, MoodbarPipeline *pipeline) {

  Data *data = data_[url];
  if (!data) {
    return;
  }

  if (RemoveFromCacheIfIndexesInvalid(url, data)) {
    return;
  }

  if (!pipeline->success()) {
    data->state_ = Data::State_CannotLoad;
    return;
  }

  // Load the colors next.
  StartLoadingColors(url, pipeline->data(), data);

}

void MoodbarItemDelegate::StartLoadingColors(const QUrl &url, const QByteArray &bytes, Data *data) {

  data->state_ = Data::State_LoadingColors;

  QFuture<ColorVector> future = QtConcurrent::run(MoodbarRenderer::Colors, bytes, style_, qApp->palette());
  QFutureWatcher<ColorVector> *watcher = new QFutureWatcher<ColorVector>();
  watcher->setFuture(future);
  QObject::connect(watcher, &QFutureWatcher<ColorVector>::finished, this, [this, watcher, url]() {
    ColorsLoaded(url, watcher->result());
    watcher->deleteLater();
  });

}

void MoodbarItemDelegate::ColorsLoaded(const QUrl &url, const ColorVector &colors) {

  Data *data = data_[url];
  if (!data) {
    return;
  }

  if (RemoveFromCacheIfIndexesInvalid(url, data)) {
    return;
  }

  data->colors_ = colors;

  // Load the image next.
  StartLoadingImage(url, data);

}

void MoodbarItemDelegate::StartLoadingImage(const QUrl &url, Data *data) {

  data->state_ = Data::State_LoadingImage;

  QFuture<QImage> future = QtConcurrent::run(MoodbarRenderer::RenderToImage, data->colors_, data->desired_size_);
  QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
  watcher->setFuture(future);
  QObject::connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, url]() {
    ImageLoaded(url, watcher->result());
    watcher->deleteLater();
  });

}

void MoodbarItemDelegate::ImageLoaded(const QUrl &url, const QImage &image) {

  Data *data = data_[url];
  if (!data) {
    return;
  }

  if (RemoveFromCacheIfIndexesInvalid(url, data)) {
    return;
  }

  // If the desired size changed then don't even bother converting the image
  // to a pixmap, just reload it at the new size.
  if (!image.isNull() && data->desired_size_ != image.size()) {
    StartLoadingImage(url, data);
    return;
  }

  data->pixmap_ = QPixmap::fromImage(image);
  data->state_ = Data::State_Loaded;

  Playlist *playlist = view_->playlist();
  const QSortFilterProxyModel *filter = playlist->proxy();

  // Update all the indices with the new pixmap.
  for (const QPersistentModelIndex &idx : data->indexes_) {
    if (idx.isValid() && idx.sibling(idx.row(), Playlist::Column_Filename).data().toUrl() == url) {
      QModelIndex source_index = idx;
      if (idx.model() == filter) {
        source_index = filter->mapToSource(source_index);
      }

      if (source_index.model() != playlist) {
        // The pixmap was for an index in a different playlist, maybe the user
        // switched to a different one.
        continue;
      }

      playlist->MoodbarUpdated(source_index);
    }
  }

}
