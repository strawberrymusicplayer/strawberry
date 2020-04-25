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

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QByteArray>
#include <QImage>
#include <QPixmap>
#include <QPalette>
#include <QBrush>
#include <QMovie>
#include <QTimeLine>
#include <QPainter>
#include <QSizePolicy>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPaintEvent>

#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloader.h"

#include "contextview.h"
#include "contextalbum.h"

const int ContextAlbum::kWidgetSpacing = 40;

ContextAlbum::ContextAlbum(QWidget *parent) :
    QWidget(parent),
    menu_(new QMenu(this)),
    context_view_(nullptr),
    album_cover_choice_controller_(nullptr),
    downloading_covers_(false),
    timeline_fade_(new QTimeLine(1000, this)),
    image_strawberry_(":/pictures/strawberry.png"),
    image_original_(image_strawberry_),
    prev_width_(width()) {

  setObjectName("context-widget-album");
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  cover_loader_options_.desired_height_ = 600;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;
  QPair<QImage, QImage> images = AlbumCoverLoader::ScaleAndPad(cover_loader_options_, image_strawberry_);
  pixmap_current_ = QPixmap::fromImage(images.first);

  connect(timeline_fade_, SIGNAL(valueChanged(qreal)), SLOT(FadePreviousTrack(qreal)));
  timeline_fade_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0

}

void ContextAlbum::Init(ContextView *context_view, AlbumCoverChoiceController *album_cover_choice_controller) {

  context_view_ = context_view;

  album_cover_choice_controller_ = album_cover_choice_controller;
  connect(album_cover_choice_controller_, SIGNAL(AutomaticCoverSearchDone()), this, SLOT(AutomaticCoverSearchDone()));

  QList<QAction*> cover_actions = album_cover_choice_controller_->GetAllActions();
  cover_actions.append(album_cover_choice_controller_->search_cover_auto_action());
  menu_->addActions(cover_actions);
  menu_->addSeparator();

}

void ContextAlbum::contextMenuEvent(QContextMenuEvent *e) {
  if (menu_ && image_original_ != image_strawberry_) menu_->popup(mapToGlobal(e->pos()));
}

void ContextAlbum::mouseDoubleClickEvent(QMouseEvent *e) {

  // Same behaviour as right-click > Show Fullsize
  if (image_original_ != image_strawberry_ && e->button() == Qt::LeftButton && context_view_->song_playing().is_valid()) {
    album_cover_choice_controller_->ShowCover(context_view_->song_playing(), image_original_);
  }

}

void ContextAlbum::paintEvent(QPaintEvent*) {

  QPainter p(this);

  DrawImage(&p);

  // Draw the previous track's image if we're fading
  if (!pixmap_previous_.isNull()) {
    p.setOpacity(pixmap_previous_opacity_);
    p.drawPixmap(0, 0, pixmap_previous_);
  }

}

void ContextAlbum::DrawImage(QPainter *p) {

  if (width() != prev_width_) {
    cover_loader_options_.desired_height_ = width() - kWidgetSpacing;
    pixmap_current_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, image_original_).first);
    prev_width_ = width();
  }

  p->drawPixmap(0, 0, width() - kWidgetSpacing, width() - kWidgetSpacing, pixmap_current_);
  if (downloading_covers_ && spinner_animation_) {
    p->drawPixmap(50, 50, 16, 16, spinner_animation_->currentPixmap());
  }

}

void ContextAlbum::FadePreviousTrack(const qreal value) {

  pixmap_previous_opacity_ = value;
  if (qFuzzyCompare(pixmap_previous_opacity_, qreal(0.0))) {
    image_previous_ = QImage();
    pixmap_previous_ = QPixmap();
  }
  update();

  if (value == 0 && image_original_ == image_strawberry_) {
    emit FadeStopFinished();
  }

}

void ContextAlbum::ScaleCover() {

  cover_loader_options_.desired_height_ = width() - kWidgetSpacing;
  pixmap_current_ = QPixmap::fromImage(AlbumCoverLoader::ScaleAndPad(cover_loader_options_, image_original_).first);
  prev_width_ = width();
  update();

}

void ContextAlbum::SetImage(QImage image) {

  if (image.isNull()) image = image_strawberry_;

  if (downloading_covers_) {
    downloading_covers_ = false;
    spinner_animation_.reset();
  }

  // Cache the current pixmap so we can fade between them
  pixmap_previous_ = QPixmap(width() - kWidgetSpacing, width() - kWidgetSpacing);
  pixmap_previous_.fill(palette().window().color());
  pixmap_previous_opacity_ = 1.0;

  QPainter p(&pixmap_previous_);
  DrawImage(&p);
  p.end();

  image_previous_ = image_original_;
  image_original_ = image;

  ScaleCover();

  // Were we waiting for this cover to load before we started fading?
  if (!pixmap_previous_.isNull() && timeline_fade_) {
    timeline_fade_->stop();
    timeline_fade_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0
    timeline_fade_->start();
  }

}

void ContextAlbum::SearchCoverInProgress() {

  downloading_covers_ = true;

  // Show a spinner animation
  spinner_animation_.reset(new QMovie(":/pictures/spinner.gif", QByteArray(), this));
  connect(spinner_animation_.get(), SIGNAL(updated(QRect)), SLOT(update()));
  spinner_animation_->start();
  update();

}

void ContextAlbum::AutomaticCoverSearchDone() {

  downloading_covers_ = false;
  spinner_animation_.reset();
  update();

}
