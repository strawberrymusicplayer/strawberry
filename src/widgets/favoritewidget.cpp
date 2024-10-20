/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>
#include <QWidget>
#include <QSize>
#include <QStyle>
#include <QStylePainter>
#include <QPaintEvent>
#include <QMouseEvent>

#include "core/iconloader.h"

#include "favoritewidget.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kStarSize = 15;
}

FavoriteWidget::FavoriteWidget(const int tab_index, const bool favorite, QWidget *parent)
    : QWidget(parent),
      tab_index_(tab_index),
      favorite_(favorite),
      on_(IconLoader::Load(u"star"_s)),
      off_(IconLoader::Load(u"star-grey"_s)),
      rect_(0, 0, kStarSize, kStarSize) {}

void FavoriteWidget::SetFavorite(const bool favorite) {

  if (favorite_ != favorite) {
    favorite_ = favorite;
    update();
    Q_EMIT FavoriteStateChanged(tab_index_, favorite_);
  }

}

QSize FavoriteWidget::sizeHint() const {
  const int frame_width = 1 + style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  return QSize(kStarSize + frame_width, kStarSize + frame_width);
}

void FavoriteWidget::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e);

  QStylePainter p(this);

  if (favorite_) {
    p.drawPixmap(rect_, on_.pixmap(rect_.size(), devicePixelRatioF()));
  }
  else {
    p.drawPixmap(rect_, off_.pixmap(rect_.size(), devicePixelRatioF()));
  }

}

void FavoriteWidget::mouseDoubleClickEvent(QMouseEvent *e) {

  Q_UNUSED(e)

  favorite_ = !favorite_;
  update();
  Q_EMIT FavoriteStateChanged(tab_index_, favorite_);

}
