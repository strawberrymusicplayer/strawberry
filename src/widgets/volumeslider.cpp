/***************************************************************************
                        amarokslider.cpp  -  description
                           -------------------
  begin                : Dec 15 2003
  copyright            : (C) 2003 by Mark Kretschmann
  email                : markey@web.de
  copyright            : (C) 2005 by Gábor Lehel
  email                : illissius@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "config.h"

#include "volumeslider.h"

#include <QApplication>
#include <QWidget>
#include <QHash>
#include <QString>
#include <QStringBuilder>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QFont>
#include <QBrush>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QVector>
#include <QMenu>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QAction>
#include <QSlider>
#include <QLinearGradient>
#include <QStyleOptionViewItem>
#include <QFlags>
#include <QtEvents>

SliderSlider::SliderSlider(const Qt::Orientation orientation, QWidget *parent, const int max)
    : QSlider(orientation, parent),
      sliding_(false),
      outside_(false),
      prev_value_(0) {

  setRange(0, max);

}

void SliderSlider::wheelEvent(QWheelEvent *e) {

  if (orientation() == Qt::Vertical) {
    // Will be handled by the parent widget
    e->ignore();
    return;
  }

  // Position Slider (horizontal)
  int step = e->angleDelta().y() * 1500 / 18;
  int nval = qBound(minimum(), QSlider::value() + step, maximum());

  QSlider::setValue(nval);

  emit sliderReleased(value());

}

void SliderSlider::mouseMoveEvent(QMouseEvent *e) {

  if (sliding_) {
    // feels better, but using set value of 20 is bad of course
    QRect rect(-20, -20, width() + 40, height() + 40);

    if (orientation() == Qt::Horizontal && !rect.contains(e->pos())) {
      if (!outside_) QSlider::setValue(prev_value_);
      outside_ = true;
    }
    else {
      outside_ = false;
      slideEvent(e);
      emit sliderMoved(value());
    }
  }
  else {
    QSlider::mouseMoveEvent(e);
  }

}

void SliderSlider::slideEvent(QMouseEvent *e) {

  QStyleOptionSlider option;
  initStyleOption(&option);
  QRect sliderRect(style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this));

  QSlider::setValue(
      orientation() == Qt::Horizontal
          ? ((QApplication::layoutDirection() == Qt::RightToLeft)
                 ? QStyle::sliderValueFromPosition(
                       minimum(), maximum(),
                       width() - (e->pos().x() - sliderRect.width() / 2),
                       width() + sliderRect.width(), true)
                 : QStyle::sliderValueFromPosition(
                       minimum(), maximum(),
                       e->pos().x() - sliderRect.width() / 2,
                       width() - sliderRect.width()))
          : QStyle::sliderValueFromPosition(
                minimum(), maximum(), e->pos().y() - sliderRect.height() / 2,
                height() - sliderRect.height()));

}

void SliderSlider::mousePressEvent(QMouseEvent *e) {

  QStyleOptionSlider option;
  initStyleOption(&option);
  QRect sliderRect(style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this));

  sliding_ = true;
  prev_value_ = QSlider::value();

  if (!sliderRect.contains(e->pos())) mouseMoveEvent(e);

}

void SliderSlider::mouseReleaseEvent(QMouseEvent*) {

  if (!outside_ && QSlider::value() != prev_value_) {
    emit sliderReleased(value());
  }

  sliding_ = false;
  outside_ = false;

}

void SliderSlider::setValue(int newValue) {
  // don't adjust the slider while the user is dragging it!

  if (!sliding_ || outside_) {
    QSlider::setValue(adjustValue(newValue));
  }
  else {
    prev_value_ = newValue;
  }

}

//////////////////////////////////////////////////////////////////////////////////////////
/// CLASS PrettySlider
//////////////////////////////////////////////////////////////////////////////////////////

PrettySlider::PrettySlider(const Qt::Orientation orientation, const SliderMode mode, QWidget *parent, const uint max)
    : SliderSlider(orientation, parent, static_cast<int>(max)), m_mode(mode) {

  if (m_mode == Pretty) {
    setFocusPolicy(Qt::NoFocus);
  }

}

void PrettySlider::mousePressEvent(QMouseEvent *e) {

  SliderSlider::mousePressEvent(e);

  slideEvent(e);

}

void PrettySlider::slideEvent(QMouseEvent *e) {

  if (m_mode == Pretty) {
    QSlider::setValue(orientation() == Qt::Horizontal ? QStyle::sliderValueFromPosition(minimum(), maximum(), e->pos().x(), width() - 2) : QStyle::sliderValueFromPosition(minimum(), maximum(), e->pos().y(), height() - 2));  // clazy:exclude=skipped-base-method
  }
  else {
    SliderSlider::slideEvent(e);
  }

}

namespace Amarok {
namespace ColorScheme {
extern QColor Background;
extern QColor Foreground;
}  // namespace ColorScheme
}  // namespace Amarok

#if 0
/** these functions aren't required in our fixed size world, but they may become useful one day **/

QSize PrettySlider::minimumSizeHint() const {
    return sizeHint();
}

QSize PrettySlider::sizeHint() const {
    constPolish();

    return (orientation() == Horizontal
             ? QSize( maxValue(), THICKNESS + MARGIN )
             : QSize( THICKNESS + MARGIN, maxValue() )).expandedTo( QApplit ication::globalStrut() );
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
/// CLASS VolumeSlider
//////////////////////////////////////////////////////////////////////////////////////////

VolumeSlider::VolumeSlider(QWidget *parent, const uint max)
    : SliderSlider(Qt::Horizontal, parent, static_cast<int>(max)),
      anim_enter_(false),
      anim_count_(0),
      timer_anim_(new QTimer(this)),
      pixmap_inset_(QPixmap(drawVolumePixmap())) {

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

void VolumeSlider::generateGradient() {

  const QImage mask(":/pictures/volumeslider-gradient.png");

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

void VolumeSlider::mousePressEvent(QMouseEvent *e) {

  if (e->button() != Qt::RightButton) {
    SliderSlider::mousePressEvent(e);
    slideEvent(e);
  }

}

void VolumeSlider::contextMenuEvent(QContextMenuEvent *e) {

  QHash<QAction*, int> values;
  QMenu menu;
  menu.setTitle("Volume");
  values[menu.addAction("100%")] = 100;
  values[menu.addAction("80%")] = 80;
  values[menu.addAction("60%")] = 60;
  values[menu.addAction("40%")] = 40;
  values[menu.addAction("20%")] = 20;
  values[menu.addAction("0%")] = 0;

  QAction *ret = menu.exec(mapToGlobal(e->pos()));
  if (ret) {
    QSlider::setValue(values[ret]);  // clazy:exclude=skipped-base-method
    emit sliderReleased(values[ret]);
  }

}

void VolumeSlider::slideEvent(QMouseEvent *e) {
  QSlider::setValue(QStyle::sliderValueFromPosition(minimum(), maximum(), e->pos().x(), width() - 2));  // clazy:exclude=skipped-base-method
}

void VolumeSlider::wheelEvent(QWheelEvent *e) {

  const int step = e->angleDelta().y() / (e->angleDelta().x() == 0 ? 30 : -30);
  QSlider::setValue(SliderSlider::value() + step);  // clazy:exclude=skipped-base-method
  emit sliderReleased(value());

}

void VolumeSlider::paintEvent(QPaintEvent*) {

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
  p.drawPixmap(offset - handle_pixmaps_[0].width() / 2 + padding, 0, handle_pixmaps_[anim_count_]);

  // Draw percentage number
  QStyleOptionViewItem opt;
  p.setPen(opt.palette.color(QPalette::Normal, QPalette::Text));
  QFont vol_font(opt.font);
  vol_font.setPixelSize(9);
  p.setFont(vol_font);
  const QRect rect(0, 0, 34, 15);
  p.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, QString::number(value()) + '%');

}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VolumeSlider::enterEvent(QEnterEvent*) {
#else
void VolumeSlider::enterEvent(QEvent*) {
#endif

  anim_enter_ = true;
  anim_count_ = 0;

  timer_anim_->start(ANIM_INTERVAL);

}

void VolumeSlider::leaveEvent(QEvent*) {

  // This can happen if you enter and leave the widget quickly
  if (anim_count_ == 0) anim_count_ = 1;

  anim_enter_ = false;
  timer_anim_->start(ANIM_INTERVAL);

}

void VolumeSlider::paletteChange(const QPalette&) {
  generateGradient();
}

QPixmap VolumeSlider::drawVolumePixmap () const {

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

  QImage pixmapHandle(":/pictures/volumeslider-handle.png");
  QImage pixmapHandleGlow(":/pictures/volumeslider-handle_glow.png");

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
  float opacity = 0.0;
  const float step = 1.0 / ANIM_MAX;
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
