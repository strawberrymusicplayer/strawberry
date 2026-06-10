/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include <algorithm>

#include <QtGlobal>
#include <QProxyStyle>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QColor>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStyleOption>
#include <QStyleOptionComplex>
#include <QStyleOptionSlider>
#include <QTimeLine>
#include <QStyle>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QContextMenuEvent>

#include "core/settings.h"
#include "constants/waveformsettings.h"
#include "waveform/waveformrenderer.h"
#include "waveformproxystyle.h"

namespace {
// Width of the playhead cursor line (D-06: thin vertical line, no arrow handle).
constexpr int kCursorWidth = 1;
// Alpha applied to the Highlight color for the played region: same hue, dimmed.
constexpr float kPlayedAlpha = 0.55f;
// Duration of the waveform fade-in / fade-out transition in milliseconds.
constexpr int kFadeDurationMs = 1000;
}  // namespace

WaveformProxyStyle::WaveformProxyStyle(QSlider *slider, QObject *parent)
    : QProxyStyle(nullptr),
      slider_(slider),
      show_(false),
      color_(),
      state_(State::WaveformOff),
      fade_timeline_(new QTimeLine(kFadeDurationMs, this)),
      waveform_pixmap_dirty_(true),
      context_menu_(nullptr),
      show_waveform_action_(nullptr) {

  Q_UNUSED(parent)

  slider->setStyle(this);
  slider->installEventFilter(this);

  QObject::connect(fade_timeline_, &QTimeLine::valueChanged, this, &WaveformProxyStyle::FaderValueChanged);

  // Seed show_ from QSettings and establish the initial size policy, mirroring
  // how MoodbarProxyStyle normalizes the slider through ReloadSettings on creation.
  ReloadSettings();

}

void WaveformProxyStyle::ReloadSettings() {

  Settings s;
  s.beginGroup(WaveformSettings::kSettingsGroup);
  show_ = s.value(WaveformSettings::kEnabled, false).toBool();
  color_ = s.value(WaveformSettings::kColor).value<QColor>();
  s.endGroup();

  // A color change requires a fresh render of the bar pixmaps.
  waveform_pixmap_dirty_ = true;

  NextState();

}

void WaveformProxyStyle::SetWaveformData(const QByteArray &data) {

  data_ = data;
  waveform_pixmap_dirty_ = true;  // Redraw next time
  NextState();
  slider_->update();

}

void WaveformProxyStyle::SetShowWaveform(const bool show) {

  if (show == show_) return;

  Settings s;
  s.beginGroup(WaveformSettings::kSettingsGroup);
  // kEnabled is the single source of truth shared by the context-menu toggle
  // here and the Preferences checkbox (WaveformSettingsPage::Save). Both
  // writers use last-writer-wins semantics on the same key; consistent state
  // is guaranteed because every reader re-reads the key. Concurrent writes
  // (e.g. context-menu toggle while the Preferences dialog is open) are an
  // accepted edge case, consistent with the same pattern in MoodbarProxyStyle.
  s.setValue(WaveformSettings::kEnabled, show);
  s.endGroup();

  ReloadSettings();  // reads back kEnabled, calls NextState()

}

void WaveformProxyStyle::NextState() {

  const bool visible = show_ && !data_.isEmpty();

  // While the regular slider should stay at the standard size (Fixed), the
  // waveform should use all available space (MinimumExpanding) so it expands
  // vertically when active (REN-03).
  slider_->setSizePolicy(QSizePolicy::Expanding, visible ? QSizePolicy::MinimumExpanding : QSizePolicy::Fixed);
  slider_->updateGeometry();

  if (show_waveform_action_) {
    show_waveform_action_->setChecked(show_);
  }

  if ((visible && (state_ == State::WaveformOn || state_ == State::FadingToOn)) || (!visible && (state_ == State::WaveformOff || state_ == State::FadingToOff))) {
    return;
  }

  const QTimeLine::Direction direction = visible ? QTimeLine::Direction::Forward : QTimeLine::Direction::Backward;

  if (state_ == State::WaveformOn || state_ == State::WaveformOff) {
    // Start the fade from the beginning.
    fade_timeline_->setDirection(direction);
    if (fade_timeline_->state() != QTimeLine::State::NotRunning) {
      fade_timeline_->stop();
    }
    fade_timeline_->start();

    fade_source_ = QPixmap();
    fade_target_ = QPixmap();
  }
  else {
    // Stop an existing fade and start fading the other direction from the same place.
    fade_timeline_->stop();
    fade_timeline_->setDirection(direction);
    fade_timeline_->resume();
  }

  state_ = visible ? State::FadingToOn : State::FadingToOff;

}

void WaveformProxyStyle::FaderValueChanged(const qreal value) {
  Q_UNUSED(value);
  slider_->update();
}

bool WaveformProxyStyle::eventFilter(QObject *object, QEvent *event) {

  if (object == slider_) {
    switch (event->type()) {
      case QEvent::Resize:
        // The widget was resized, we've got to render a new pixmap.
        waveform_pixmap_dirty_ = true;
        break;

      case QEvent::ContextMenu:
        ShowContextMenu(static_cast<QContextMenuEvent*>(event)->globalPos());
        return true;

      default:
        break;
    }
  }

  return QProxyStyle::eventFilter(object, event);

}

void WaveformProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {

  const QStyleOptionSlider *slider_option = qstyleoption_cast<const QStyleOptionSlider*>(option);

  if (control != CC_Slider || widget != slider_ || !slider_option) {
    QProxyStyle::drawComplexControl(control, option, painter, widget);
    return;
  }

  // const_cast is required because Render must update mutable state (the dirty
  // flag, the pixmap cache and the fade state machine).
  const_cast<WaveformProxyStyle*>(this)->Render(control, slider_option, painter, widget);

}

void WaveformProxyStyle::Render(const ComplexControl control, const QStyleOptionSlider *option, QPainter *painter, const QWidget *widget) {

  const qreal fade_value = fade_timeline_->currentValue();

  // Have we finished fading?
  if (state_ == State::FadingToOn && fade_value == 1.0) {
    state_ = State::WaveformOn;
  }
  else if (state_ == State::FadingToOff && fade_value == 0.0) {
    state_ = State::WaveformOff;
  }

  // Live split-pixel computation (D-06/D-07): drives both the played/unplayed
  // color boundary and the cursor overdraw. Recomputed every repaint so it
  // tracks playback while the cached bar pixmaps stay position-independent.
  int x_split = 0;
  if (option->maximum > option->minimum) {
    const qint64 delta = static_cast<qint64>(option->sliderValue) - static_cast<qint64>(option->minimum);
    const qint64 range = static_cast<qint64>(option->maximum) - static_cast<qint64>(option->minimum);
    x_split = static_cast<int>(delta * static_cast<qint64>(option->rect.width()) / range);
    // sliderValue is not guaranteed to be within [minimum, maximum] (TrackSlider::SetValue
    // sets the maximum then the value, briefly leaving sliderValue > maximum), which would
    // otherwise yield a negative-width unplayed clip rect and a cursor past the groove edge.
    x_split = std::clamp(x_split, 0, option->rect.width());
  }

  // Use the slider's palette consistently for every paint path so the fade
  // source, the rendered bars and the cursor all share the same colors and no
  // seam appears during the crossfade.
  const QPalette palette = slider_->palette();
  const QColor cursor_color = palette.color(QPalette::Active, QPalette::WindowText);

  switch (state_) {
    case State::FadingToOn:
    case State::FadingToOff:
      // Update the cached pixmaps if necessary.
      if (fade_source_.isNull()) {
        // Draw the normal slider into the fade source pixmap.
        fade_source_ = QPixmap(option->rect.size());
        fade_source_.fill(palette.color(QPalette::Active, QPalette::Window));

        QPainter p(&fade_source_);
        QStyleOptionSlider opt_copy(*option);
        opt_copy.rect.moveTo(0, 0);

        QProxyStyle::drawComplexControl(control, &opt_copy, &p, widget);

        p.end();
      }

      if (fade_target_.isNull()) {
        if (state_ == State::FadingToOn) {
          EnsureWaveformRendered();
        }
        // Fade towards the unplayed bars; the split need not animate during the
        // crossfade (the live composite takes over once the fade completes).
        fade_target_ = waveform_pixmap_unplayed_;
      }

      // Blend the pixmaps into each other.
      painter->drawPixmap(option->rect, fade_source_);
      painter->setOpacity(fade_value);
      painter->drawPixmap(option->rect, fade_target_);
      painter->setOpacity(1.0);
      break;

    case State::WaveformOff:
      // It's a normal slider widget.
      QProxyStyle::drawComplexControl(control, option, painter, widget);
      break;

    case State::WaveformOn: {
      EnsureWaveformRendered();

      // Composite the two position-independent pixmaps around the live split:
      // played bars left of x_split, unplayed bars at/right of it. The QSlider
      // repaints its groove on every value change, so this tracks playback.
      const QRect played_rect(option->rect.left(), option->rect.top(), x_split, option->rect.height());
      const QRect unplayed_rect(option->rect.left() + x_split, option->rect.top(), option->rect.width() - x_split, option->rect.height());

      painter->save();
      painter->setClipRect(played_rect);
      painter->drawPixmap(option->rect, waveform_pixmap_played_);
      painter->restore();

      painter->save();
      painter->setClipRect(unplayed_rect);
      painter->drawPixmap(option->rect, waveform_pixmap_unplayed_);
      painter->restore();

      // Overlay the cursor line at the split.
      painter->setPen(QPen(cursor_color, kCursorWidth));
      painter->drawLine(option->rect.left() + x_split, option->rect.top(), option->rect.left() + x_split, option->rect.bottom());
      break;
    }
  }

}

void WaveformProxyStyle::EnsureWaveformRendered() {

  if (!waveform_pixmap_dirty_) return;

  // Render at the device-pixel-ratio-scaled size so the waveform stays sharp on
  // HiDPI displays, then tag each pixmap with the ratio so it draws back at the
  // device-independent geometry.
  const qreal dpr = slider_->devicePixelRatioF();
  const QSize dpr_size = slider_->size() * dpr;
  const QPalette palette = slider_->palette();

  // Use the user's custom color when set; fall back to the theme Highlight so
  // the default look follows the current theme without any explicit preference.
  const QColor unplayed_color = color_.isValid() ? color_ : palette.color(QPalette::Active, QPalette::Highlight);
  QColor played_color = unplayed_color;
  played_color.setAlphaF(kPlayedAlpha);

  QPixmap unplayed = WaveformRenderer::RenderToPixmap(data_, dpr_size, palette, unplayed_color);
  unplayed.setDevicePixelRatio(dpr);
  waveform_pixmap_unplayed_ = unplayed;

  QPixmap played = WaveformRenderer::RenderToPixmap(data_, dpr_size, palette, played_color);
  played.setDevicePixelRatio(dpr);
  waveform_pixmap_played_ = played;

  waveform_pixmap_dirty_ = false;

}

QRect WaveformProxyStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget) const {

  if (cc != QStyle::CC_Slider || widget != slider_) {
    return QProxyStyle::subControlRect(cc, opt, sc, widget);
  }

  switch (state_) {
    case State::WaveformOff:
    case State::FadingToOff:
      break;

    case State::WaveformOn:
    case State::FadingToOn:
      switch (sc) {
        case SC_SliderGroove:
          return opt->rect.adjusted(kWaveformMarginSize, kWaveformMarginSize, -kWaveformMarginSize, -kWaveformMarginSize);

        case SC_SliderHandle:{
          const QStyleOptionSlider *slider_opt = qstyleoption_cast<const QStyleOptionSlider*>(opt);
          if (!slider_opt) break;

          int x_offset = 0;

          // slider_opt->{maximum,minimum} can have the value 0 (their default values), so this check avoids a division by 0.
          if (slider_opt->maximum > slider_opt->minimum) {
            const qint64 slider_delta = static_cast<qint64>(slider_opt->sliderValue) - static_cast<qint64>(slider_opt->minimum);
            const qint64 slider_range = static_cast<qint64>(slider_opt->maximum) - static_cast<qint64>(slider_opt->minimum);
            // No arrow width subtracted: the waveform uses a thin line cursor (D-06).
            const qint64 x = slider_delta * static_cast<qint64>(opt->rect.width()) / slider_range;
            // Clamp to the groove width so a transient sliderValue > maximum cannot
            // place the cursor handle outside the groove (see CR-02 in the Render path).
            x_offset = std::clamp(static_cast<int>(x), 0, opt->rect.width());
          }

          return QRect(QPoint(opt->rect.left() + x_offset, opt->rect.top()), QSize(kCursorWidth, opt->rect.height()));
        }

        default:
          break;
      }
  }

  return QProxyStyle::subControlRect(cc, opt, sc, widget);

}

void WaveformProxyStyle::ShowContextMenu(const QPoint pos) {

  if (!context_menu_) {
    context_menu_ = new QMenu(slider_);
    show_waveform_action_ = context_menu_->addAction(tr("Show waveform"), this, &WaveformProxyStyle::SetShowWaveform);

    show_waveform_action_->setCheckable(true);
    show_waveform_action_->setChecked(show_);

    // Toggling the action both updates this style (SetShowWaveform via the
    // action's slot) and notifies TrackSlider so it can enforce mutual
    // exclusivity against the moodbar. No MoodbarProxyStyle reference here.
    QObject::connect(show_waveform_action_, &QAction::triggered, this, &WaveformProxyStyle::WaveformShow);
  }

  context_menu_->popup(pos);

}
