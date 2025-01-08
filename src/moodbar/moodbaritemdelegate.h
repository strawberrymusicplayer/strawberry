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

#ifndef MOODBARITEMDELEGATE_H
#define MOODBARITEMDELEGATE_H

#include "moodbarrenderer.h"

#include <QObject>
#include <QItemDelegate>
#include <QCache>
#include <QSet>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QStyleOption>

#include "includes/shared_ptr.h"
#include "constants/moodbarsettings.h"
#include "moodbarpipeline.h"

class QPainter;
class MoodbarLoader;
class PlaylistView;

class MoodbarItemDelegate : public QItemDelegate {
  Q_OBJECT

 public:
  explicit MoodbarItemDelegate(const SharedPtr<MoodbarLoader> moodbar_loader, PlaylistView *view, QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;

 private Q_SLOTS:
  void ReloadSettings();

 Q_SIGNALS:
  void StyleChanged();

 private:
  void DataLoaded(const QUrl &url, MoodbarPipelinePtr pipeline);
  void ColorsLoaded(const QUrl &url, const ColorVector &colors);
  void ImageLoaded(const QUrl &url, const QImage &image);

 private:
  struct Data {
    Data();

    enum class State {
      None,
      CannotLoad,
      LoadingData,
      LoadingColors,
      LoadingImage,
      Loaded
    };

    QSet<QPersistentModelIndex> indexes_;

    State state_;
    ColorVector colors_;
    QSize desired_size_;
    QPixmap pixmap_;
  };

 private:
  QPixmap PixmapForIndex(const QModelIndex &idx, const QSize size);
  void StartLoadingData(const QUrl &url, const bool has_cue, Data *data);
  void StartLoadingColors(const QUrl &url, const QByteArray &bytes, Data *data);
  void StartLoadingImage(const QUrl &url, Data *data);

  bool RemoveFromCacheIfIndexesInvalid(const QUrl &url, Data *data);

  void ReloadAllColors();

 private:
  const SharedPtr<MoodbarLoader> moodbar_loader_;
  PlaylistView *playlist_view_;
  QCache<QUrl, Data> data_;

  bool enabled_;
  MoodbarSettings::Style style_;
};

#endif  // MOODBARITEMDELEGATE_H
