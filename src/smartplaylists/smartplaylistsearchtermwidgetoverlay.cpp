/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QString>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QKeyEvent>
#include <QMouseEvent>

#include "core/iconloader.h"
#include "smartplaylistsearchterm.h"
#include "smartplaylistsearchtermwidget.h"
#include "smartplaylistsearchtermwidgetoverlay.h"

using namespace Qt::Literals::StringLiterals;

const int SmartPlaylistSearchTermWidgetOverlay::kSpacing = 6;
const int SmartPlaylistSearchTermWidgetOverlay::kIconSize = 22;

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

SmartPlaylistSearchTermWidgetOverlay::SmartPlaylistSearchTermWidgetOverlay(SmartPlaylistSearchTermWidget *parent)
    : QWidget(parent),
      parent_(parent),
      opacity_(0.0),
      text_(tr("Add search term")),
      icon_(IconLoader::Load(u"list-add"_s).pixmap(kIconSize)) {

  raise();
  setFocusPolicy(Qt::TabFocus);

}

void SmartPlaylistSearchTermWidgetOverlay::Grab() {

  hide();

  // Take a "screenshot" of the window
  QPixmap pixmap = parent_->grab();
  QImage image = pixmap.toImage();

  // Blur it
  QImage blurred(image.size(), QImage::Format_ARGB32_Premultiplied);
  blurred.fill(Qt::transparent);

  QPainter blur_painter(&blurred);
  qt_blurImage(&blur_painter, image, 10.0, true, false);
  blur_painter.end();

  pixmap_ = QPixmap::fromImage(blurred);

  resize(parent_->size());
  show();
  update();

}

void SmartPlaylistSearchTermWidgetOverlay::SetOpacity(const float opacity) {

  opacity_ = opacity;
  update();

}

void SmartPlaylistSearchTermWidgetOverlay::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

  QPainter p(this);

  // Background
  p.fillRect(rect(), palette().window());

  // Blurred parent widget
  p.setOpacity(0.25 + opacity_ * 0.25);
  p.drawPixmap(0, 0, pixmap_);

  // Draw a frame
  p.setOpacity(1.0);
  p.setPen(palette().color(QPalette::Mid));
  p.setRenderHint(QPainter::Antialiasing);
  p.drawRoundedRect(rect(), 5, 5);

  // Geometry

  const QSize contents_size(kIconSize + kSpacing + fontMetrics().horizontalAdvance(text_), qMax(kIconSize, fontMetrics().height()));

  const QRect contents(QPoint((width() - contents_size.width()) / 2, (height() - contents_size.height()) / 2), contents_size);
  const QRect icon(contents.topLeft(), QSize(kIconSize, kIconSize));
  const QRect text(icon.right() + kSpacing, icon.top(), contents.width() - kSpacing - kIconSize, contents.height());

  // Icon and text
  p.setPen(palette().color(QPalette::Text));
  p.drawPixmap(icon, icon_);
  p.drawText(text, Qt::TextDontClip | Qt::AlignVCenter, text_);  // NOLINT(bugprone-suspicious-enum-usage)

}

void SmartPlaylistSearchTermWidgetOverlay::mouseReleaseEvent(QMouseEvent *e) {

  Q_UNUSED(e)
  Q_EMIT parent_->Clicked();

}

void SmartPlaylistSearchTermWidgetOverlay::keyReleaseEvent(QKeyEvent *e) {

  if (e->key() == Qt::Key_Space) {
    Q_EMIT parent_->Clicked();
  }

}

float SmartPlaylistSearchTermWidgetOverlay::opacity() const { return opacity_; }
