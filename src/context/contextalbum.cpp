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

#include "config.h"

#include <utility>
#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QByteArray>
#include <QImage>
#include <QPixmap>
#include <QMovie>
#include <QTimeLine>
#include <QTimer>
#include <QRect>
#include <QPainter>
#include <QSizePolicy>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPaintEvent>
#include <QResizeEvent>

#include "includes/shared_ptr.h"
#include "utilities/imageutils.h"
#include "covermanager/albumcoverchoicecontroller.h"

#include "contextalbum.h"

using std::make_unique;
using std::make_shared;

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kFadeTimeLineMs = 1000;
// Rescaling the covers with smooth transformation is expensive for large covers, so it is delayed while the width keeps changing, for example when dragging a splitter.
constexpr int kScaleDelayMs = 100;
}

ContextAlbum::ContextAlbum(QWidget *parent)
    : QWidget(parent),
      menu_(new QMenu(this)),
      album_cover_choice_controller_(nullptr),
      downloading_covers_(false),
      timeline_fade_(new QTimeLine(kFadeTimeLineMs, this)),
      timer_scale_(new QTimer(this)),
      image_strawberry_(u":/pictures/strawberry.png"_s),
      image_original_(image_strawberry_),
      pixmap_current_opacity_(1.0),
      desired_height_(width()) {

  setObjectName(u"context-widget-album"_s);

  // The cover is square and as wide as the layout allows, the height follows the width.
  QSizePolicy size_policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  size_policy.setHeightForWidth(true);
  setSizePolicy(size_policy);

  QImage image = ImageUtils::ScaleImage(image_strawberry_, QSize(desired_height_, desired_height_), devicePixelRatioF(), true);
  if (!image.isNull()) {
    pixmap_current_ = QPixmap::fromImage(image);
  }

  timeline_fade_->setDirection(QTimeLine::Direction::Forward);
  QObject::connect(timeline_fade_, &QTimeLine::valueChanged, this, &ContextAlbum::FadeCurrentCover);
  QObject::connect(timeline_fade_, &QTimeLine::finished, this, &ContextAlbum::FadeCurrentCoverFinished);

  timer_scale_->setSingleShot(true);
  timer_scale_->setInterval(kScaleDelayMs);
  QObject::connect(timer_scale_, &QTimer::timeout, this, &ContextAlbum::ScaleCovers);

}

void ContextAlbum::Init(AlbumCoverChoiceController *album_cover_choice_controller) {

  album_cover_choice_controller_ = album_cover_choice_controller;
  QObject::connect(album_cover_choice_controller_, &AlbumCoverChoiceController::AutomaticCoverSearchDone, this, &ContextAlbum::AutomaticCoverSearchDone);

  QList<QAction*> cover_actions = album_cover_choice_controller_->GetAllActions();
  menu_->addActions(cover_actions);
  menu_->addSeparator();
  menu_->addAction(album_cover_choice_controller_->search_cover_auto_action());
  menu_->addSeparator();

}

QSize ContextAlbum::sizeHint() const {

  // The covers are scaled and padded to a square of the desired size, use that size directly so the size is right while rescaling is delayed.
  return pixmap_current_.isNull() ? QSize(0, 0) : QSize(desired_height_, desired_height_);

}

void ContextAlbum::paintEvent(QPaintEvent *paint_event) {

  Q_UNUSED(paint_event)

  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  DrawPreviousCovers(&p);
  DrawImage(&p, pixmap_current_, pixmap_current_opacity_);
  DrawSpinner(&p);
  p.end();

}

void ContextAlbum::mouseDoubleClickEvent(QMouseEvent *e) {

  // Same behaviour as right-click > Show Fullsize
  if (image_original_ != image_strawberry_ && e->button() == Qt::LeftButton && song_playing_.is_valid()) {
    album_cover_choice_controller_->ShowCover(song_playing_, image_original_);
  }

}

void ContextAlbum::contextMenuEvent(QContextMenuEvent *e) {

  if (menu_ && image_original_ != image_strawberry_) {
    menu_->popup(mapToGlobal(e->pos()));
  }
  else {
    QWidget::contextMenuEvent(e);
  }

}

bool ContextAlbum::hasHeightForWidth() const {

  return true;

}

int ContextAlbum::heightForWidth(const int widget_width) const {

  return widget_width;

}

void ContextAlbum::resizeEvent(QResizeEvent *e) {

  QWidget::resizeEvent(e);

  if (width() != desired_height_) {
    desired_height_ = width();
    // The current pixmaps are drawn scaled to the new size until the covers are rescaled.
    timer_scale_->start();
    update();
  }

}

void ContextAlbum::SetImage(const QImage &image) {

  if (downloading_covers_) {
    downloading_covers_ = false;
    spinner_animation_.reset();
  }

  // Setting the same image again, for example when stopping is signalled twice, would restart the fade.
  const QImage &image_new = image.isNull() ? image_strawberry_ : image;
  if (image_new == image_original_) {
    // The stopped view is still shown once the strawberry is shown, like when the fade to it finishes.
    if (image_original_ == image_strawberry_ && timeline_fade_->state() == QTimeLine::State::NotRunning) {
      Q_EMIT FadeStopFinished();
    }
    return;
  }

  QImage image_previous = image_original_;
  QPixmap pixmap_previous = pixmap_current_;
  qreal opacity_previous = pixmap_current_opacity_;

  image_original_ = image_new;

  pixmap_current_opacity_ = 0.0;
  ScaleCover();

  if (!pixmap_previous.isNull()) {
    SharedPtr<PreviousCover> previous_cover = make_shared<PreviousCover>();
    previous_cover->image = image_previous;
    previous_cover->pixmap = pixmap_previous;
    previous_cover->opacity = opacity_previous;
    // The timeline has this as parent, so it is also deleted if this is deleted while fading.
    previous_cover->timeline.reset(new QTimeLine(kFadeTimeLineMs, this), [](QTimeLine *timeline) { timeline->deleteLater(); });
    previous_cover->timeline->setDirection(QTimeLine::Direction::Backward);
    previous_cover->timeline->setCurrentTime(timeline_fade_->state() == QTimeLine::State::Running ? timeline_fade_->currentTime() : kFadeTimeLineMs);
    // Capture a weak pointer, the timeline is owned by the previous cover, so a shared pointer would be a reference cycle.
    const std::weak_ptr<PreviousCover> previous_cover_weak = previous_cover;
    QObject::connect(&*previous_cover->timeline, &QTimeLine::valueChanged, this, [this, previous_cover_weak]() {
      if (SharedPtr<PreviousCover> previous_cover_locked = previous_cover_weak.lock()) {
        FadePreviousCover(previous_cover_locked);
      }
    });
    QObject::connect(&*previous_cover->timeline, &QTimeLine::finished, this, [this, previous_cover_weak]() {
      if (SharedPtr<PreviousCover> previous_cover_locked = previous_cover_weak.lock()) {
        FadePreviousCoverFinished(previous_cover_locked);
      }
    });
    previous_covers_ << previous_cover;
    previous_cover->timeline->start();
  }

  if (timeline_fade_->state() != QTimeLine::State::NotRunning) {
    timeline_fade_->stop();
  }
  timeline_fade_->start();

}

void ContextAlbum::DrawImage(QPainter *p, const QPixmap &pixmap, const qreal opacity) {

  if (qFuzzyCompare(opacity, static_cast<qreal>(0.0))) return;

  p->setOpacity(opacity);
  // Draw to the desired size, the pixmap is scaled to it, or is still being rescaled after the width changed.
  p->drawPixmap(QRect(0, 0, desired_height_, desired_height_), pixmap);

}

void ContextAlbum::DrawSpinner(QPainter *p) {

  if (downloading_covers_) {
    p->drawPixmap(50, 50, 16, 16, spinner_animation_->currentPixmap());
  }

}

void ContextAlbum::DrawPreviousCovers(QPainter *p) {

  for (const SharedPtr<PreviousCover> &previous_cover : std::as_const(previous_covers_)) {
    DrawImage(p, previous_cover->pixmap, previous_cover->opacity);
  }

}

void ContextAlbum::FadeCurrentCover(const qreal value) {

  if (value <= pixmap_current_opacity_) return;

  pixmap_current_opacity_ = value;
  update();

}

void ContextAlbum::FadeCurrentCoverFinished() {

  if (image_original_ == image_strawberry_) {
    Q_EMIT FadeStopFinished();
  }

}

void ContextAlbum::FadePreviousCover(SharedPtr<PreviousCover> previous_cover) {

  if (previous_cover->timeline->currentValue() >= previous_cover->opacity) return;

  previous_cover->opacity = previous_cover->timeline->currentValue();

}

void ContextAlbum::FadePreviousCoverFinished(SharedPtr<PreviousCover> previous_cover) {

  previous_cover->timeline.reset();
  previous_covers_.removeAll(previous_cover);

}

void ContextAlbum::ScaleCover() {

  const QImage image = ImageUtils::ScaleImage(image_original_, QSize(desired_height_, desired_height_), devicePixelRatioF(), true);
  if (image.isNull()) {
    pixmap_current_ = QPixmap();
  }
  else {
    pixmap_current_ = QPixmap::fromImage(image);
  }

}

void ContextAlbum::ScalePreviousCovers() {

  for (const SharedPtr<PreviousCover> &previous_cover : std::as_const(previous_covers_)) {
    const QImage image = ImageUtils::ScaleImage(previous_cover->image, QSize(desired_height_, desired_height_), devicePixelRatioF(), true);
    if (image.isNull()) {
      previous_cover->pixmap = QPixmap();
    }
    else {
      previous_cover->pixmap = QPixmap::fromImage(image);
    }
  }

}

void ContextAlbum::ScaleCovers() {

  ScaleCover();
  ScalePreviousCovers();
  update();

}

void ContextAlbum::SearchCoverInProgress() {

  downloading_covers_ = true;

  // Show a spinner animation
  spinner_animation_ = make_unique<QMovie>(u":/pictures/spinner.gif"_s, QByteArray(), this);
  QObject::connect(&*spinner_animation_, &QMovie::updated, this, &ContextAlbum::Update);
  spinner_animation_->start();
  update();

}

void ContextAlbum::AutomaticCoverSearchDone() {

  downloading_covers_ = false;
  spinner_animation_.reset();
  update();

}
