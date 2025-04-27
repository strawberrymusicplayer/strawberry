/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QAbstractItemModel>
#include <QSettings>
#include <QItemDelegate>
#include <QByteArray>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QRect>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/settings.h"
#include "playlist/playlist.h"
#include "playlist/playlistview.h"
#include "playlist/playlistfilter.h"

#include "moodbaritemdelegate.h"
#include "moodbarloader.h"
#include "moodbarpipeline.h"
#include "moodbarrenderer.h"

#include "constants/moodbarsettings.h"

using std::make_shared;

MoodbarItemDelegate::Data::Data() : state_(State::None) {}

MoodbarItemDelegate::MoodbarItemDelegate(const SharedPtr<MoodbarLoader> moodbar_loader, PlaylistView *playlist_view, QObject *parent)
    : QItemDelegate(parent),
      moodbar_loader_(moodbar_loader),
      playlist_view_(playlist_view),
      enabled_(false),
      style_(MoodbarSettings::Style::Normal) {

  QObject::connect(&*moodbar_loader, &MoodbarLoader::SettingsReloaded, this, &MoodbarItemDelegate::ReloadSettings);
  QObject::connect(&*moodbar_loader, &MoodbarLoader::StyleChanged, this, &MoodbarItemDelegate::ReloadSettings);

  ReloadSettings();

}

void MoodbarItemDelegate::ReloadSettings() {

  Settings s;
  s.beginGroup(MoodbarSettings::kSettingsGroup);
  enabled_ = s.value(MoodbarSettings::kEnabled, false).toBool();
  const MoodbarSettings::Style new_style = static_cast<MoodbarSettings::Style>(s.value(MoodbarSettings::kStyle, static_cast<int>(MoodbarSettings::Style::Normal)).toInt());
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

QPixmap MoodbarItemDelegate::PixmapForIndex(const QModelIndex &idx, const QSize size) {

  // Pixmaps are keyed off URL.
  const QUrl url = idx.sibling(idx.row(), static_cast<int>(Playlist::Column::URL)).data().toUrl();
  const bool has_cue = idx.sibling(idx.row(), static_cast<int>(Playlist::Column::HasCUE)).data().toBool();

  Data *data = nullptr;
  if (data_.contains(url)) {
    data = data_[url];
  }
  else {
    data = new Data;
    if (!data_.insert(url, data)) {
      qLog(Error) << "Could not insert moodbar data for URL" << url << "into cache";
      return QPixmap();
    }
  }

  data->indexes_.insert(idx);
  data->desired_size_ = size;

  switch (data->state_) {
    case Data::State::CannotLoad:
    case Data::State::LoadingData:
    case Data::State::LoadingColors:
    case Data::State::LoadingImage:
      return data->pixmap_;

    case Data::State::Loaded:
      // Is the pixmap the right size?
      if (data->pixmap_.size() != size) {
        StartLoadingImage(url, data);
      }

      return data->pixmap_;

    case Data::State::None:
      break;
  }

  // We have to start loading the data from scratch.
  StartLoadingData(url, has_cue, data);

  return QPixmap();

}

void MoodbarItemDelegate::StartLoadingData(const QUrl &url, const bool has_cue, Data *data) {

  data->state_ = Data::State::LoadingData;

  // Load a mood file for this song and generate some colors from it
  const MoodbarLoader::LoadResult load_result = moodbar_loader_->Load(url, has_cue);
  switch (load_result.status) {
    case MoodbarLoader::LoadStatus::CannotLoad:
      data->state_ = Data::State::CannotLoad;
      break;

    case MoodbarLoader::LoadStatus::Loaded:
      StartLoadingColors(url, load_result.data, data);
      break;

    case MoodbarLoader::LoadStatus::WillLoadAsync:
      MoodbarPipelinePtr pipeline = load_result.pipeline;
      Q_ASSERT(pipeline);
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*pipeline, &MoodbarPipeline::Finished, this, [this, connection, url, pipeline]() {
        DataLoaded(url, pipeline);
        QObject::disconnect(*connection);
      });
      break;
  }

}

bool MoodbarItemDelegate::RemoveFromCacheIfIndexesInvalid(const QUrl &url, Data *data) {

  QSet<QPersistentModelIndex> indexes = data->indexes_;

  if (std::any_of(indexes.begin(), indexes.end(), [](const QPersistentModelIndex &idx) { return idx.isValid(); })) { return false; }

  data_.remove(url);
  return true;

}

void MoodbarItemDelegate::ReloadAllColors() {

  const QList<QUrl> urls = data_.keys();
  for (const QUrl &url : urls) {
    Data *data = data_[url];

    if (data->state_ == Data::State::Loaded) {
      StartLoadingData(url, false, data);
    }
  }

}

void MoodbarItemDelegate::DataLoaded(const QUrl &url, MoodbarPipelinePtr pipeline) {

  if (!data_.contains(url)) return;

  Data *data = data_[url];

  if (RemoveFromCacheIfIndexesInvalid(url, data)) {
    return;
  }

  if (!pipeline->success()) {
    data->state_ = Data::State::CannotLoad;
    return;
  }

  // Load the colors next.
  StartLoadingColors(url, pipeline->data(), data);

}

void MoodbarItemDelegate::StartLoadingColors(const QUrl &url, const QByteArray &bytes, Data *data) {

  data->state_ = Data::State::LoadingColors;

  QFuture<ColorVector> future = QtConcurrent::run(MoodbarRenderer::Colors, bytes, style_, qApp->palette());
  QFutureWatcher<ColorVector> *watcher = new QFutureWatcher<ColorVector>();
  QObject::connect(watcher, &QFutureWatcher<ColorVector>::finished, this, [this, watcher, url]() {
    ColorsLoaded(url, watcher->result());
    watcher->deleteLater();
  });
  watcher->setFuture(future);

}

void MoodbarItemDelegate::ColorsLoaded(const QUrl &url, const ColorVector &colors) {

  if (!data_.contains(url)) return;

  Data *data = data_[url];

  if (RemoveFromCacheIfIndexesInvalid(url, data)) {
    return;
  }

  data->colors_ = colors;

  // Load the image next.
  StartLoadingImage(url, data);

}

void MoodbarItemDelegate::StartLoadingImage(const QUrl &url, Data *data) {

  data->state_ = Data::State::LoadingImage;

  QFuture<QImage> future = QtConcurrent::run(MoodbarRenderer::RenderToImage, data->colors_, data->desired_size_);
  QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
  QObject::connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, url]() {
    ImageLoaded(url, watcher->result());
    watcher->deleteLater();
  });
  watcher->setFuture(future);

}

void MoodbarItemDelegate::ImageLoaded(const QUrl &url, const QImage &image) {

  if (!data_.contains(url)) return;

  Data *data = data_[url];

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
  data->state_ = Data::State::Loaded;

  Playlist *playlist = playlist_view_->playlist();
  const PlaylistFilter *filter = playlist->filter();

  // Update all the indices with the new pixmap.
  for (const QPersistentModelIndex &idx : std::as_const(data->indexes_)) {
    if (idx.isValid() && idx.sibling(idx.row(), static_cast<int>(Playlist::Column::URL)).data().toUrl() == url) {
      QModelIndex source_index = idx;
      if (idx.model() == filter) {
        source_index = filter->mapToSource(source_index);
      }

      if (source_index.model() != playlist) {
        // The pixmap was for an index in a different playlist, maybe the user switched to a different one.
        continue;
      }

      playlist->MoodbarUpdated(source_index);
    }
  }

}
