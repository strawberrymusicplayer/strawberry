/***************************************************************************
                        volumeslider.cpp
                        -------------------
  begin                : Dec 15 2003
  copyright            : (C) 2003 by Mark Kretschmann
  email                : markey@web.de
  copyright            : (C) 2005 by Gábor Lehel
  email                : illissius@gmail.com
  copyright            : (C) 2018-2023 by Jonas Kvinge
  email                : jonas@jkvinge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QSlider>
#include <QHash>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QBrush>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QMenu>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QAction>
#include <QLinearGradient>
#include <QStyleOptionViewItem>
#include <QEnterEvent>
#include <QPaintEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "volumeslider.h"

using namespace Qt::Literals::StringLiterals;

VolumeSlider::VolumeSlider(QWidget *parent, const uint max)
    : SliderSlider(Qt::Horizontal, parent, static_cast<int>(max)),
      wheel_accumulator_(0),
      anim_enter_(false),
      anim_count_(0),
      timer_anim_(new QTimer(this)),
      pixmap_inset_(drawVolumePixmap()) {

  setFocusPolicy(Qt::NoFocus);

  // Store theme colors to check theme change at paintEvent
  previous_theme_text_color_ = palette().color(QPalette::WindowText);
  previous_theme_highlight_color_ = palette().color(QPalette::Highlight);

  drawVolumeSliderHandle();
  generateGradient();

  setMinimumWidth(pixmap_inset_.width());
  setMinimumHeight(pixmap_inset_.height());

  QObject::connect(timer_anim_, &QTimer::timeout, this, &VolumeSlider::slotAnimTimer);

}

void VolumeSlider::SetEnabled(const bool enabled) {
  QSlider::setEnabled(enabled);
  QSlider::setVisible(enabled);
}

void VolumeSlider::HandleWheel(const int delta) {

  const int scroll_state = wheel_accumulator_ + delta;
  const int steps = scroll_state / WHEEL_ROTATION_PER_STEP;
  wheel_accumulator_ = scroll_state % WHEEL_ROTATION_PER_STEP;

  if (steps != 0) {
    wheeling_ = true;

    QSlider::setValue(SliderSlider::value() + steps);
    Q_EMIT SliderReleased(value());

    wheeling_ = false;
  }

}

void VolumeSlider::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

  QPainter p(this);

  const int padding = 7;
  const int offset = static_cast<int>(static_cast<double>((width() - 2 * padding) * value()) / maximum());

  // If theme changed since last paintEvent, redraw the volume pixmap with new theme colors
  if (previous_theme_text_color_ != palette().color(QPalette::WindowText)) {
    pixmap_inset_ = drawVolumePixmap();
    previous_theme_text_color_ = palette().color(QPalette::WindowText);
  }

  if (previous_theme_highlight_color_ != palette().color(QPalette::Highlight)) {
    drawVolumeSliderHandle();
    previous_theme_highlight_color_ = palette().color(QPalette::Highlight);
  }

  p.drawPixmap(0, 0, pixmap_gradient_, 0, 0, offset + padding, 0);
  p.drawPixmap(0, 0, pixmap_inset_);
  p.drawPixmap(offset - handle_pixmaps_.value(0).width() / 2 + padding, 0, handle_pixmaps_[anim_count_]);

  // Draw percentage number
  QStyleOptionViewItem opt;
  p.setPen(opt.palette.color(QPalette::Normal, QPalette::Text));
  QFont vol_font(opt.font);
  vol_font.setPixelSize(9);
  p.setFont(vol_font);
  const QRect rect(0, 0, 34, 15);
  p.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, QString::number(value()) + QLatin1Char('%'));

}

void VolumeSlider::generateGradient() {

  const QImage mask(u":/pictures/volumeslider-gradient.png"_s);

  QImage gradient_image(mask.size(), QImage::Format_ARGB32_Premultiplied);
  QPainter p(&gradient_image);

  QLinearGradient gradient(gradient_image.rect().topLeft(), gradient_image.rect().topRight());
  gradient.setColorAt(0, palette().color(QPalette::Window));
  gradient.setColorAt(1, palette().color(QPalette::Highlight));
  p.fillRect(gradient_image.rect(), QBrush(gradient));

  p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  p.drawImage(0, 0, mask);
  p.end();

  pixmap_gradient_ = QPixmap::fromImage(gradient_image);

}

void VolumeSlider::slotAnimTimer() {

  if (anim_enter_) {
    ++anim_count_;
    update();
    if (anim_count_ == ANIM_MAX - 1) timer_anim_->stop();
  }
  else {
    --anim_count_;
    update();
    if (anim_count_ == 0) timer_anim_->stop();
  }

}

void VolumeSlider::paletteChange(const QPalette &palette) {
  Q_UNUSED(palette)
  generateGradient();
}

QPixmap VolumeSlider::drawVolumePixmap() const {

  QPixmap pixmap(112, 36);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  QPen pen(palette().color(QPalette::WindowText), 0.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  painter.setPen(pen);

  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  // Draw volume control pixmap
  QPolygon poly;
  poly << QPoint(6, 21) << QPoint(104, 21) << QPoint(104, 7) << QPoint(6, 16) << QPoint(6, 21);
  QPainterPath path;
  path.addPolygon(poly);
  painter.drawPolygon(poly);
  painter.drawLine(6, 29, 104, 29);

  // Return QPixmap
  return pixmap;

}

void VolumeSlider::drawVolumeSliderHandle() {

  QImage pixmapHandle(u":/pictures/volumeslider-handle.png"_s);
  QImage pixmapHandleGlow(u":/pictures/volumeslider-handle_glow.png"_s);

  QImage pixmapHandleGlow_image(pixmapHandleGlow.size(), QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&pixmapHandleGlow_image);

  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Repaint volume slider handle glow image with theme highlight color
  painter.fillRect(pixmapHandleGlow_image.rect(), QBrush(palette().color(QPalette::Highlight)));
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(0, 0, pixmapHandleGlow);

  // Overlay the volume slider handle image
  painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
  painter.drawImage(0, 0, pixmapHandle);

  // BEGIN Calculate handle animation pixmaps for mouse-over effect
  float opacity = 0.0F;
  const float step = 1.0F / ANIM_MAX;
  QImage dst;
  handle_pixmaps_.clear();
  for (int i = 0; i < ANIM_MAX; ++i) {
    dst = pixmapHandle.copy();

    QPainter p(&dst);
    p.setOpacity(opacity);
    p.drawImage(0, 0, pixmapHandleGlow_image);
    p.end();

    handle_pixmaps_.append(QPixmap::fromImage(dst));
    opacity += step;
  }
  // END

}

void VolumeSlider::enterEvent(QEnterEvent *e) {

  Q_UNUSED(e)

  anim_enter_ = true;
  anim_count_ = 0;

  timer_anim_->start(ANIM_INTERVAL);

}

void VolumeSlider::leaveEvent(QEvent *e) {

  Q_UNUSED(e)

  // This can happen if you enter and leave the widget quickly
  if (anim_count_ == 0) anim_count_ = 1;

  anim_enter_ = false;
  timer_anim_->start(ANIM_INTERVAL);

}

void VolumeSlider::contextMenuEvent(QContextMenuEvent *e) {

  QHash<QAction*, int> values;
  QMenu menu;
  menu.setTitle(u"Volume"_s);
  values[menu.addAction(u"100%"_s)] = 100;
  values[menu.addAction(u"80%"_s)] = 80;
  values[menu.addAction(u"60%"_s)] = 60;
  values[menu.addAction(u"40%"_s)] = 40;
  values[menu.addAction(u"20%"_s)] = 20;
  values[menu.addAction(u"0%"_s)] = 0;

  QAction *ret = menu.exec(mapToGlobal(e->pos()));
  if (ret) {
    QSlider::setValue(values[ret]);
    Q_EMIT SliderReleased(values[ret]);
  }

}

void VolumeSlider::slideEvent(QMouseEvent *e) {
  QSlider::setValue(QStyle::sliderValueFromPosition(minimum(), maximum(), e->pos().x(), width() - 2));
}

void VolumeSlider::mousePressEvent(QMouseEvent *e) {

  if (e->button() != Qt::RightButton) {
    SliderSlider::mousePressEvent(e);
    slideEvent(e);
  }

}

void VolumeSlider::wheelEvent(QWheelEvent *e) {
  HandleWheel(e->angleDelta().y());
}
