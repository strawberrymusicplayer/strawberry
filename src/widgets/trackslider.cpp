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

#include "config.h"

#include <QtGlobal>
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QSize>
#include <QLabel>
#include <QSettings>
#include <QEvent>

#include "core/settings.h"
#include "utilities/timeutils.h"
#include "constants/timeconstants.h"
#include "trackslider.h"
#include "ui_trackslider.h"
#include "clickablelabel.h"
#include "tracksliderslider.h"

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarproxystyle.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "MainWindow";
}

TrackSlider::TrackSlider(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_TrackSlider),
#ifdef HAVE_MOODBAR
      moodbar_proxy_style_(nullptr),
#endif
      setting_value_(false),
      show_remaining_time_(true),
      slider_maximum_value_(0) {

  ui_->setupUi(this);

  UpdateLabelWidth();

  // Load settings
  Settings s;
  s.beginGroup(kSettingsGroup);
  show_remaining_time_ = s.value("show_remaining_time").toBool();
  s.endGroup();

  QObject::connect(ui_->slider, &TrackSliderSlider::sliderMoved, this, &TrackSlider::ValueChanged);
  QObject::connect(ui_->slider, &TrackSliderSlider::valueChanged, this, &TrackSlider::ValueMaybeChanged);
  QObject::connect(ui_->remaining, &ClickableLabel::Clicked, this, &TrackSlider::ToggleTimeDisplay);
  QObject::connect(ui_->slider, &TrackSliderSlider::SeekForward, this, &TrackSlider::SeekForward);
  QObject::connect(ui_->slider, &TrackSliderSlider::SeekBackward, this, &TrackSlider::SeekBackward);
  QObject::connect(ui_->slider, &TrackSliderSlider::Previous, this, &TrackSlider::Previous);
  QObject::connect(ui_->slider, &TrackSliderSlider::Next, this, &TrackSlider::Next);

}

TrackSlider::~TrackSlider() {

  delete ui_;
#ifdef HAVE_MOODBAR
  if (moodbar_proxy_style_) moodbar_proxy_style_->deleteLater();
#endif

}

void TrackSlider::Init() {

#ifdef HAVE_MOODBAR
  if (!moodbar_proxy_style_) moodbar_proxy_style_ = new MoodbarProxyStyle(ui_->slider);
#endif

}

void TrackSlider::UpdateLabelWidth() {

  // We set the label's minimum size, so it won't resize itself when the user is dragging the slider.
  UpdateLabelWidth(ui_->elapsed, u"0:00:00"_s);
  UpdateLabelWidth(ui_->remaining, u"-0:00:00"_s);

}

void TrackSlider::UpdateLabelWidth(QLabel *label, const QString &text) {

  QString old_text = label->text();
  label->setText(text);
  label->setMinimumWidth(0);
  int width = label->sizeHint().width();
  label->setText(old_text);

  label->setMinimumWidth(width);

}

QSize TrackSlider::sizeHint() const {

  int width = 500;
  width += ui_->elapsed->sizeHint().width();
  width += ui_->remaining->sizeHint().width();

  int height = qMax(ui_->slider->sizeHint().height(), ui_->elapsed->sizeHint().height());

  return QSize(width, height);

}

void TrackSlider::SetValue(const int elapsed, const int total) {

  setting_value_ = true;  // This is so we don't emit from QAbstractSlider::valueChanged
  ui_->slider->setMaximum(total);
  if (!ui_->slider->isSliderDown()) {
    ui_->slider->setValue(elapsed);
  }

  setting_value_ = false;

  UpdateTimes(static_cast<int>(elapsed / kMsecPerSec));

}

void TrackSlider::UpdateTimes(const int elapsed) {

  ui_->elapsed->setText(Utilities::PrettyTime(elapsed));
  // Update normally if showing remaining time
  if (show_remaining_time_) {
    ui_->remaining->setText(QLatin1Char('-') + Utilities::PrettyTime(static_cast<int>(ui_->slider->maximum() / kMsecPerSec) - elapsed));
  }
  else {
    // Check if slider maximum value is changed before updating
    if (slider_maximum_value_ != ui_->slider->maximum() || !ui_->slider->isEnabled()) {
      slider_maximum_value_ = ui_->slider->maximum();
      ui_->remaining->setText(Utilities::PrettyTime(static_cast<int>(ui_->slider->maximum() / kMsecPerSec)));
    }
  }
  setEnabled(true);

}

void TrackSlider::SetStopped() {

  setEnabled(false);
  ui_->elapsed->setText(u"0:00:00"_s);
  ui_->remaining->setText(u"0:00:00"_s);

  setting_value_ = true;
  ui_->slider->setValue(0);
  slider_maximum_value_ = 0;
  setting_value_ = false;

}

void TrackSlider::SetCanSeek(const bool can_seek) {
  ui_->slider->setEnabled(can_seek);
}

void TrackSlider::Seek(const int gap) {

  if (ui_->slider->isEnabled()) {
    ui_->slider->setValue(static_cast<int>(ui_->slider->value() + gap * kMsecPerSec));
  }

}

void TrackSlider::ValueMaybeChanged(const int value) {

  if (setting_value_) return;

  UpdateTimes(static_cast<int>(value / kMsecPerSec));
  Q_EMIT ValueChangedSeconds(static_cast<quint64>(value / kMsecPerSec));

}

bool TrackSlider::event(QEvent *e) {

  switch (e->type()) {
    case QEvent::ApplicationFontChange:
    case QEvent::StyleChange:
      UpdateLabelWidth();
      break;
    default:
      break;
  }

  return QWidget::event(e);

}

void TrackSlider::ToggleTimeDisplay() {

  show_remaining_time_ = !show_remaining_time_;
  if (!show_remaining_time_) {
    // We set the value to -1 because the label must be updated
    slider_maximum_value_ = -1;
  }
  UpdateTimes(static_cast<int>(ui_->slider->value() / kMsecPerSec));

  // Save this setting
  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("show_remaining_time", show_remaining_time_);

}
