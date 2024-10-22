/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2011, David Sansome <me@davidsansome.com>
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
#include <QString>
#include <QImage>
#include <QFontMetrics>
#include <QSize>
#include <QColor>
#include <QPainter>
#include <QPalette>
#include <QBrush>
#include <QRect>
#include <QPoint>
#include <QPolygon>
#include <QPaintEvent>

#include "includes/qt_blurimage.h"
#include "tracksliderpopup.h"

namespace {
constexpr int kTextMargin = 4;
constexpr int kPointLength = 16;
constexpr int kPointWidth = 4;
constexpr int kBorderRadius = 4;
constexpr qreal kBlurRadius = 20.0;
}  // namespace

TrackSliderPopup::TrackSliderPopup(QWidget *parent)
    : QWidget(parent),
      font_metrics_(fontMetrics()),
      small_font_metrics_(fontMetrics()) {

  setAttribute(Qt::WA_TransparentForMouseEvents);
  setMouseTracking(true);

  font_.setPointSizeF(7.5);
  font_.setBold(true);
  small_font_.setPointSizeF(7.5);
  font_metrics_ = QFontMetrics(font_);
  small_font_metrics_ = QFontMetrics(small_font_);

}

void TrackSliderPopup::SetText(const QString &text) {
  text_ = text;
  UpdatePixmap();
}

void TrackSliderPopup::SetSmallText(const QString &text) {
  small_text_ = text;
  UpdatePixmap();
}

void TrackSliderPopup::SetPopupPosition(const QPoint pos) {
  pos_ = pos;
  UpdatePosition();
}

void TrackSliderPopup::paintEvent(QPaintEvent *e) {
  Q_UNUSED(e)
  QPainter p(this);
  p.drawPixmap(0, 0, pixmap_);
}

void TrackSliderPopup::UpdatePixmap() {

  const int text_width = qMax(font_metrics_.horizontalAdvance(text_), small_font_metrics_.horizontalAdvance(small_text_));
  const QRect text_rect1(static_cast<int>(kBlurRadius) + kTextMargin, static_cast<int>(kBlurRadius) + kTextMargin, text_width + 2, font_metrics_.height());
  const QRect text_rect2(static_cast<int>(kBlurRadius) + kTextMargin, text_rect1.bottom(), text_width, small_font_metrics_.height());

  const int bubble_bottom = text_rect2.bottom() + kTextMargin;
  const QRect total_rect(0, 0, text_rect1.right() + static_cast<int>(kBlurRadius) + kTextMargin, static_cast<int>(kBlurRadius) + bubble_bottom + kPointLength);
  const QRect bubble_rect(static_cast<int>(kBlurRadius), static_cast<int>(kBlurRadius), total_rect.width() - static_cast<int>(kBlurRadius) * 2, bubble_bottom - static_cast<int>(kBlurRadius));

  if (background_cache_.size() != total_rect.size()) {
    const QColor highlight(palette().color(QPalette::Active, QPalette::Highlight));
    const QColor bg_color_1(highlight.lighter(110));
    const QColor bg_color_2(highlight.darker(120));

    QPolygon pointy;
    pointy << QPoint(total_rect.width() / 2 - kPointWidth, bubble_bottom)
           << QPoint(total_rect.width() / 2, total_rect.bottom() - static_cast<int>(kBlurRadius))
           << QPoint(total_rect.width() / 2 + kPointWidth, bubble_bottom);

    QPolygon inner_pointy;
    inner_pointy << QPoint(pointy[0].x() + 1, pointy[0].y() - 1)
                 << QPoint(pointy[1].x(), pointy[1].y() - 1)
                 << QPoint(pointy[2].x() - 1, pointy[2].y() - 1);

    background_cache_ = QPixmap(total_rect.size());
    background_cache_.fill(Qt::transparent);
    QPainter p(&background_cache_);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw the shadow to a different image
    QImage blur_source(total_rect.size(), QImage::Format_ARGB32);
    blur_source.fill(Qt::transparent);

    QPainter blur_painter(&blur_source);
    blur_painter.setRenderHint(QPainter::Antialiasing);
    blur_painter.setBrush(bg_color_2);
    blur_painter.drawRoundedRect(bubble_rect, kBorderRadius, kBorderRadius);
    blur_painter.drawPolygon(pointy);

    // Fade the shadow out towards the bottom
    QLinearGradient fade_gradient(QPoint(0, bubble_bottom), QPoint(0, bubble_bottom + kPointLength));
    fade_gradient.setColorAt(0.0, QColor(255, 0, 0, 0));
    fade_gradient.setColorAt(1.0, QColor(255, 0, 0, 255));
    blur_painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    blur_painter.fillRect(total_rect, fade_gradient);
    blur_painter.end();

    p.save();
    qt_blurImage(&p, blur_source, kBlurRadius, true, false);
    p.restore();

    // Outer bubble
    p.setPen(Qt::NoPen);
    p.setBrush(bg_color_2);
    p.drawRoundedRect(bubble_rect, kBorderRadius, kBorderRadius);

    // Outer pointy
    p.drawPolygon(pointy);

    // Inner bubble
    p.setBrush(bg_color_1);
    p.drawRoundedRect(bubble_rect.adjusted(1, 1, -1, -1), kBorderRadius, kBorderRadius);

    // Inner pointy
    p.drawPolygon(inner_pointy);
  }

  pixmap_ = QPixmap(total_rect.size());
  pixmap_.fill(Qt::transparent);
  QPainter p(&pixmap_);
  p.setRenderHint(QPainter::Antialiasing);

  // Background
  p.drawPixmap(total_rect.topLeft(), background_cache_);

  // Text
  p.setPen(palette().color(QPalette::HighlightedText));
  p.setFont(font_);
  p.drawText(text_rect1, Qt::AlignHCenter, text_);

  p.setFont(small_font_);
  p.setOpacity(0.65);
  p.drawText(text_rect2, Qt::AlignHCenter, small_text_);

  p.end();

  resize(pixmap_.size());
  UpdatePosition();
  update();
}

void TrackSliderPopup::UpdatePosition() {
  move(pos_.x() - pixmap_.width() / 2, pos_.y() - pixmap_.height() + static_cast<int>(kBlurRadius));
}
