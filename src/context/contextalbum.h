/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CONTEXTALBUM_H
#define CONTEXTALBUM_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QMovie>

#include "covermanager/albumcoverloaderoptions.h"

class QTimeLine;
class QPainter;
class QPaintEvent;

class AlbumCoverChoiceController;

class ContextAlbum : public QWidget {
  Q_OBJECT

 public:
  ContextAlbum(QWidget *parent = nullptr);

  void Init(AlbumCoverChoiceController *album_cover_choice_controller);
  void SetImage(QImage image = QImage());

 protected:
  void paintEvent(QPaintEvent*);

 private:
  void DrawImage(QPainter *p);
  void ScaleCover();
  void GetCoverAutomatically();

 signals:
  void FadeStopFinished();

 private slots:
  void SearchCoverInProgress();
  void AutomaticCoverSearchDone();
  void FadePreviousTrack(const qreal value);

 private:
  static const int kWidgetSpacing;
  AlbumCoverChoiceController *album_cover_choice_controller_;
  AlbumCoverLoaderOptions cover_loader_options_;
  bool downloading_covers_;
  QTimeLine *timeline_fade_;
  QImage image_strawberry_;
  QImage image_original_;
  QImage image_previous_;
  QPixmap pixmap_current_;
  QPixmap pixmap_previous_;
  qreal pixmap_previous_opacity_;
  std::unique_ptr<QMovie> spinner_animation_;
  int prev_width_;
};

#endif  // CONTEXTALBUM_H
