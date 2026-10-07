/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QList>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QMovie>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"

class QMenu;
class QTimeLine;
class QTimer;
class QPainter;
class QPaintEvent;
class QResizeEvent;

class AlbumCoverChoiceController;

class ContextAlbum : public QWidget {
  Q_OBJECT

 public:
  explicit ContextAlbum(QWidget *parent = nullptr);

  void Init(AlbumCoverChoiceController *album_cover_choice_controller);
  void set_song_playing(const Song &song) { song_playing_ = song; }
  void SetImage(const QImage &image = QImage());

 protected:
  QSize sizeHint() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(const int widget_width) const override;
  void resizeEvent(QResizeEvent *e) override;
  void paintEvent(QPaintEvent *paint_event) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 private:
  struct PreviousCover {
    explicit PreviousCover() : opacity(0.0) {}
    QImage image;
    QPixmap pixmap;
    qreal opacity;
    SharedPtr<QTimeLine> timeline;
  };

  QList<SharedPtr<PreviousCover>> previous_covers_;

  void DrawImage(QPainter *p, const QPixmap &pixmap, const qreal opacity);
  void DrawSpinner(QPainter *p);
  void DrawPreviousCovers(QPainter *p);
  void ScaleCover();
  void ScalePreviousCovers();
  void ScaleCovers();

 Q_SIGNALS:
  void FadeStopFinished();

 private Q_SLOTS:
  void Update() { update(); }
  void AutomaticCoverSearchDone();
  void FadeCurrentCover(const qreal value);
  void FadeCurrentCoverFinished();
  void FadePreviousCover(SharedPtr<ContextAlbum::PreviousCover> previous_cover);
  void FadePreviousCoverFinished(SharedPtr<ContextAlbum::PreviousCover> previous_cover);

 public Q_SLOTS:
  void SearchCoverInProgress();

 private:
  QMenu *menu_;
  AlbumCoverChoiceController *album_cover_choice_controller_;
  bool downloading_covers_;
  QTimeLine *timeline_fade_;
  QTimer *timer_scale_;
  QImage image_strawberry_;
  QImage image_original_;
  QPixmap pixmap_current_;
  qreal pixmap_current_opacity_;
  ScopedPtr<QMovie> spinner_animation_;
  int desired_height_;
  Song song_playing_;
};

#endif  // CONTEXTALBUM_H
