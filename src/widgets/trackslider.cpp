/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QPoint>
#include <QStyle>

#include "core/settings.h"
#include "utilities/timeutils.h"
#include "constants/timeconstants.h"
#include "trackslider.h"
#include "ui_trackslider.h"
#include "clickablelabel.h"
#include "tracksliderslider.h"

#ifdef HAVE_WAVEFORM
#  include "waveform/waveformproxystyle.h"
#endif

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarproxystyle.h"
#  include "moodbar/moodbarrenderer.h"
#  include "constants/moodbarsettings.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kMainWindowSettingsGroup[] = "MainWindow";
constexpr char kShowRemainingTime[] = "show_remaining_time";
}

TrackSlider::TrackSlider(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_TrackSlider),
#ifdef HAVE_MOODBAR
      moodbar_proxy_style_(nullptr),
#endif
#ifdef HAVE_WAVEFORM
      waveform_proxy_style_(nullptr),
#endif
#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
      seekbar_mode_(SeekbarMode::Normal),
      seekbar_menu_(new QMenu(this)),
      seekbar_mode_group_(new QActionGroup(seekbar_menu_)),
      normal_action_(seekbar_mode_group_->addAction(tr("Normal"))),
#endif
#ifdef HAVE_MOODBAR
      moodbar_action_(seekbar_mode_group_->addAction(tr("Moodbar"))),
      moodbar_style_menu_(new QMenu(tr("Moodbar style"), seekbar_menu_)),
      moodbar_style_group_(new QActionGroup(moodbar_style_menu_)),
#endif
#ifdef HAVE_WAVEFORM
      waveform_action_(seekbar_mode_group_->addAction(tr("Waveform"))),
#endif
      setting_value_(false),
      show_remaining_time_(true),
      slider_maximum_value_(0) {

  ui_->setupUi(this);

  UpdateLabelWidth();

  Settings s;
  s.beginGroup(kMainWindowSettingsGroup);
  show_remaining_time_ = s.value(kShowRemainingTime).toBool();
  s.endGroup();

  QObject::connect(ui_->slider, &TrackSliderSlider::sliderMoved, this, &TrackSlider::ValueChanged);
  QObject::connect(ui_->slider, &TrackSliderSlider::valueChanged, this, &TrackSlider::ValueMaybeChanged);
  QObject::connect(ui_->remaining, &ClickableLabel::Clicked, this, &TrackSlider::ToggleTimeDisplay);
  QObject::connect(ui_->slider, &TrackSliderSlider::SeekForward, this, &TrackSlider::SeekForward);
  QObject::connect(ui_->slider, &TrackSliderSlider::SeekBackward, this, &TrackSlider::SeekBackward);
  QObject::connect(ui_->slider, &TrackSliderSlider::Previous, this, &TrackSlider::Previous);
  QObject::connect(ui_->slider, &TrackSliderSlider::Next, this, &TrackSlider::Next);

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
  // Configure the unified seekbar context menu (the menu, action group and actions are allocated in the initializer list).
  seekbar_mode_group_->setExclusive(true);
  normal_action_->setCheckable(true);
#ifdef HAVE_MOODBAR
  moodbar_action_->setCheckable(true);
#endif
#ifdef HAVE_WAVEFORM
  waveform_action_->setCheckable(true);
#endif
  seekbar_menu_->addActions(seekbar_mode_group_->actions());

  QObject::connect(normal_action_, &QAction::triggered, this, [this]() {
    SetSeekbarMode(SeekbarMode::Normal);
  });

#ifdef HAVE_MOODBAR
  QObject::connect(moodbar_action_, &QAction::triggered, this, [this]() {
    SetSeekbarMode(SeekbarMode::Moodbar);
  });

  seekbar_menu_->addMenu(moodbar_style_menu_);
  moodbar_style_group_->setExclusive(true);

  for (int i = 0; i < static_cast<int>(MoodbarSettings::Style::StyleCount); ++i) {
    const MoodbarSettings::Style style = static_cast<MoodbarSettings::Style>(i);
    QAction *action = moodbar_style_group_->addAction(MoodbarRenderer::StyleName(style));
    action->setCheckable(true);
    action->setData(i);
  }

  moodbar_style_menu_->addActions(moodbar_style_group_->actions());

  QObject::connect(moodbar_style_group_, &QActionGroup::triggered, this, [this](QAction *action) {
    if (moodbar_proxy_style_) {
      moodbar_proxy_style_->SetStyle(static_cast<MoodbarSettings::Style>(action->data().toInt()));
    }
  });
#endif

#ifdef HAVE_WAVEFORM
  QObject::connect(waveform_action_, &QAction::triggered, this, [this]() {
    SetSeekbarMode(SeekbarMode::Waveform);
  });
#endif
#endif

}

TrackSlider::~TrackSlider() {

  // Destroy the proxy styles before ui_ (which owns the slider).
  // Their fade timelines can have a queued valueChanged; deleteLater() would let that fire FaderValueChanged -> slider_->update() after the slider is already gone.
  // A synchronous delete stops the timelines first, avoiding a shutdown crash.
#ifdef HAVE_MOODBAR
  if (moodbar_proxy_style_) {
    delete moodbar_proxy_style_;
    moodbar_proxy_style_ = nullptr;
  }
#endif
#ifdef HAVE_WAVEFORM
  if (waveform_proxy_style_) {
    delete waveform_proxy_style_;
    waveform_proxy_style_ = nullptr;
  }
#endif

  delete ui_;

}

void TrackSlider::Init() {

#ifdef HAVE_MOODBAR
  if (!moodbar_proxy_style_) {
    moodbar_proxy_style_ = new MoodbarProxyStyle(ui_->slider);
  }
#endif

#ifdef HAVE_WAVEFORM
  // The waveform proxy's constructor attaches itself as the slider's style.
  // show_ defaults to false, so the slider renders normally until the user enables the waveform.
  if (!waveform_proxy_style_) {
    waveform_proxy_style_ = new WaveformProxyStyle(ui_->slider);
  }
#endif

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
  // The unified context menu is owned here; install this widget as an event filter after both proxies so the LIFO delivery gives us first refusal on ContextMenu events — the proxies never see them.
  ui_->slider->installEventFilter(this);

  // Both proxies installed themselves as the slider's style in their constructors, but only one QStyle can be active at a time.
  // Apply the persisted mode so the renderer that should be visible owns the slider from startup.
  SetSeekbarMode(LoadSeekbarMode());
#endif

}

TrackSlider::SeekbarMode TrackSlider::LoadSeekbarMode() {

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)

  Settings s;
  s.beginGroup(SeekbarSettings::kSettingsGroup);
  const SeekbarMode seekbar_mode = static_cast<SeekbarMode>(s.value(QLatin1String(SeekbarSettings::kMode), static_cast<int>(SeekbarSettings::kDefaultMode)).toInt());
  s.endGroup();

  switch (seekbar_mode) {
    case SeekbarMode::Normal:
    case SeekbarMode::Moodbar:
    case SeekbarMode::Waveform:
      return seekbar_mode;
  }

#endif  // defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)

  return SeekbarMode::Normal;

}

void TrackSlider::SetSeekbarMode(const SeekbarMode seekbar_mode) {

  seekbar_mode_ = seekbar_mode;

  // The seekbar mode is the single source of truth, persisted here so the controllers (which gate generation on it) and the next startup agree on what to show.
  Settings s;
  s.beginGroup(SeekbarSettings::kSettingsGroup);
  s.setValue(QLatin1String(SeekbarSettings::kMode), static_cast<int>(seekbar_mode));
  s.endGroup();

  // Both proxy styles drive the slider's vertical size policy in NextState() (MinimumExpanding when shown, Fixed when hidden), and the last writer wins.
  // So disable the inactive proxy first and enable the active one last — otherwise switching to a mode would leave the just-disabled proxy's Fixed policy in force and the visualization would render too small.
#ifdef HAVE_MOODBAR
  if (moodbar_proxy_style_ && seekbar_mode != SeekbarMode::Moodbar) moodbar_proxy_style_->SetShowMoodbar(false);
#endif
#ifdef HAVE_WAVEFORM
  if (waveform_proxy_style_ && seekbar_mode != SeekbarMode::Waveform) waveform_proxy_style_->SetShowWaveform(false);
#endif
#ifdef HAVE_MOODBAR
  if (moodbar_proxy_style_ && seekbar_mode == SeekbarMode::Moodbar) moodbar_proxy_style_->SetShowMoodbar(true);
#endif
#ifdef HAVE_WAVEFORM
  if (waveform_proxy_style_ && seekbar_mode == SeekbarMode::Waveform) waveform_proxy_style_->SetShowWaveform(true);
#endif

  // Only one QProxyStyle can be the slider's active style at a time, so point the slider at the renderer for the active mode — otherwise the proxy that happened to call setStyle() last would always win.
  // Both proxies fall back to a normal slider when their show flag is off, so Normal can use either.
  QStyle *active_style = nullptr;
#ifdef HAVE_WAVEFORM
  active_style = waveform_proxy_style_;
#endif
#ifdef HAVE_MOODBAR
  if (seekbar_mode == SeekbarMode::Moodbar || !active_style) active_style = moodbar_proxy_style_;
#endif

  if (active_style) ui_->slider->setStyle(active_style);

}

void TrackSlider::ReloadSettings() {

  // The seekbar mode is changed through the right-click menu (which persists it live), so re-applying the persisted value here keeps the slider consistent after a settings change elsewhere.
  SetSeekbarMode(LoadSeekbarMode());

}

bool TrackSlider::eventFilter(QObject *object, QEvent *event) {

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
  if (object == ui_->slider && event->type() == QEvent::ContextMenu) {
    ShowSeekbarContextMenu(static_cast<QContextMenuEvent *>(event)->globalPos());
    return true;
  }
#endif

  return QWidget::eventFilter(object, event);

}

void TrackSlider::contextMenuEvent(QContextMenuEvent *e) {

  Q_UNUSED(e)

  // When the slider child is enabled the eventFilter above handles the menu, but while nothing is playing the slider is disabled and the event propagates up to this container instead, so show the menu here too.
#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
  ShowSeekbarContextMenu(e->globalPos());
  e->accept();
#endif

}

void TrackSlider::ShowSeekbarContextMenu(const QPoint pos) {

  Q_UNUSED(pos)

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)

  // The menu and its actions are built in the constructor.
  // Update radio checks and submenu state before showing.
  normal_action_->setChecked(seekbar_mode_ == SeekbarMode::Normal);
#ifdef HAVE_MOODBAR
  moodbar_action_->setChecked(seekbar_mode_ == SeekbarMode::Moodbar);
  moodbar_style_menu_->setEnabled(seekbar_mode_ == SeekbarMode::Moodbar);

  if (moodbar_proxy_style_) {
    const MoodbarSettings::Style current_style = moodbar_proxy_style_->moodbar_style();
    const QList<QAction *> style_actions = moodbar_style_group_->actions();
    for (QAction *action : style_actions) {
      action->setChecked(static_cast<MoodbarSettings::Style>(action->data().toInt()) == current_style);
    }
  }
#endif
#ifdef HAVE_WAVEFORM
  waveform_action_->setChecked(seekbar_mode_ == SeekbarMode::Waveform);
#endif

  seekbar_menu_->popup(pos);

#endif  // defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)

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
  // Re-enable the time labels (the slider's enabled state is governed separately by SetCanSeek()).
  ui_->elapsed->setEnabled(true);
  ui_->remaining->setEnabled(true);

}

void TrackSlider::SetStopped() {

  // Disable seeking and grey out the slider and time labels, but keep this container widget enabled
  // so the right-click context menu (seekbar display mode) still works when nothing is playing.
  ui_->slider->setEnabled(false);
  ui_->elapsed->setEnabled(false);
  ui_->remaining->setEnabled(false);
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

  Settings s;
  s.beginGroup(kMainWindowSettingsGroup);
  s.setValue(kShowRemainingTime, show_remaining_time_);
  s.endGroup();

}
