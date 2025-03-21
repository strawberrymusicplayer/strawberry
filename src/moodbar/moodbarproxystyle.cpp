/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QProxyStyle>
#include <QSettings>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
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
#include <QActionGroup>
#include <QEvent>
#include <QContextMenuEvent>

#include "core/settings.h"

#include "moodbarproxystyle.h"
#include "moodbarrenderer.h"
#include "constants/moodbarsettings.h"

namespace {
constexpr int kMarginSize = 3;
constexpr int kBorderSize = 1;
constexpr int kArrowWidth = 17;
constexpr int kArrowHeight = 13;
}  // namespace

MoodbarProxyStyle::MoodbarProxyStyle(QSlider *slider, QObject *parent)
    : QProxyStyle(nullptr),
      slider_(slider),
      show_(true),
      moodbar_style_(MoodbarSettings::Style::Normal),
      state_(State::MoodbarOff),
      fade_timeline_(new QTimeLine(1000, this)),
      moodbar_colors_dirty_(true),
      moodbar_pixmap_dirty_(true),
      context_menu_(nullptr),
      show_moodbar_action_(nullptr),
      style_action_group_(nullptr) {

  Q_UNUSED(parent)

  slider->setStyle(this);
  slider->installEventFilter(this);

  QObject::connect(fade_timeline_, &QTimeLine::valueChanged, this, &MoodbarProxyStyle::FaderValueChanged);

  ReloadSettings();

}

void MoodbarProxyStyle::ReloadSettings() {

  Settings s;
  s.beginGroup(MoodbarSettings::kSettingsGroup);
  show_ = s.value(MoodbarSettings::kEnabled, false).toBool() && s.value(MoodbarSettings::kShow, false).toBool();

  NextState();

  // Get the style, and redraw if there's a change.
  const MoodbarSettings::Style new_style = static_cast<MoodbarSettings::Style>(s.value(MoodbarSettings::kStyle, static_cast<int>(MoodbarSettings::Style::Normal)).toInt());

  s.endGroup();

  if (new_style != moodbar_style_) {
    moodbar_style_ = new_style;
    moodbar_colors_dirty_ = true;
    slider_->update();
  }

}

void MoodbarProxyStyle::SetMoodbarData(const QByteArray &data) {

  data_ = data;
  moodbar_colors_dirty_ = true;  // Redraw next time
  NextState();

}

void MoodbarProxyStyle::SetShowMoodbar(const bool show) {

  if (show != show_) {

    show_ = show;

    Settings s;
    s.beginGroup(MoodbarSettings::kSettingsGroup);
    s.setValue(MoodbarSettings::kShow, show);
    s.endGroup();

    ReloadSettings();
  }

}

void MoodbarProxyStyle::NextState() {

  const bool visible = show_ && !data_.isEmpty();

  // While the regular slider should stay at the standard size (Fixed),
  // moodbars should use all available space (MinimumExpanding).
  slider_->setSizePolicy(QSizePolicy::Expanding, visible ? QSizePolicy::MinimumExpanding : QSizePolicy::Fixed);
  slider_->updateGeometry();

  if (show_moodbar_action_) {
    show_moodbar_action_->setChecked(show_);
  }

  if ((visible && (state_ == State::MoodbarOn || state_ == State::FadingToOn)) || (!visible && (state_ == State::MoodbarOff || state_ == State::FadingToOff))) {
    return;
  }

  const QTimeLine::Direction direction = visible ? QTimeLine::Direction::Forward : QTimeLine::Direction::Backward;

  if (state_ == State::MoodbarOn || state_ == State::MoodbarOff) {
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

void MoodbarProxyStyle::FaderValueChanged(const qreal value) {
  Q_UNUSED(value);
  slider_->update();
}

bool MoodbarProxyStyle::eventFilter(QObject *object, QEvent *event) {

  if (object == slider_) {
    switch (event->type()) {
      case QEvent::Resize:
        // The widget was resized, we've got to render a new pixmap.
        moodbar_pixmap_dirty_ = true;
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

void MoodbarProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {

  if (control != CC_Slider || widget != slider_) {
    QProxyStyle::drawComplexControl(control, option, painter, widget);
    return;
  }

  const_cast<MoodbarProxyStyle*>(this)->Render(control, qstyleoption_cast<const QStyleOptionSlider*>(option), painter, widget);

}

void MoodbarProxyStyle::Render(const ComplexControl control, const QStyleOptionSlider *option, QPainter *painter, const QWidget *widget) {

  const qreal fade_value = fade_timeline_->currentValue();

  // Have we finished fading?
  if (state_ == State::FadingToOn && fade_value == 1.0) {
    state_ = State::MoodbarOn;
  }
  else if (state_ == State::FadingToOff && fade_value == 0.0) {
    state_ = State::MoodbarOff;
  }

  switch (state_) {
    case State::FadingToOn:
    case State::FadingToOff:
      // Update the cached pixmaps if necessary
      if (fade_source_.isNull()) {
        // Draw the normal slider into the fade source pixmap.
        fade_source_ = QPixmap(option->rect.size());
        fade_source_.fill(option->palette.color(QPalette::Active, QPalette::Window));

        QPainter p(&fade_source_);
        QStyleOptionSlider opt_copy(*option);
        opt_copy.rect.moveTo(0, 0);

        QProxyStyle::drawComplexControl(control, &opt_copy, &p, widget);

        p.end();
      }

      if (fade_target_.isNull()) {
        if (state_ == State::FadingToOn) {
          EnsureMoodbarRendered(option);
        }
        fade_target_ = moodbar_pixmap_;
        QPainter p(&fade_target_);
        DrawArrow(option, &p);
        p.end();
      }

      // Blend the pixmaps into each other
      painter->drawPixmap(option->rect, fade_source_);
      painter->setOpacity(fade_value);
      painter->drawPixmap(option->rect, fade_target_);
      painter->setOpacity(1.0);
      break;

    case State::MoodbarOff:
      // It's a normal slider widget.
      QProxyStyle::drawComplexControl(control, option, painter, widget);
      break;

    case State::MoodbarOn:
      EnsureMoodbarRendered(option);
      painter->drawPixmap(option->rect, moodbar_pixmap_);
      DrawArrow(option, painter);
      break;
  }

}

void MoodbarProxyStyle::EnsureMoodbarRendered(const QStyleOptionSlider *opt) {

  if (moodbar_colors_dirty_) {
    moodbar_colors_ = MoodbarRenderer::Colors(data_, moodbar_style_, slider_->palette());
    moodbar_colors_dirty_ = false;
    moodbar_pixmap_dirty_ = true;
  }

  if (moodbar_pixmap_dirty_) {
    moodbar_pixmap_ = MoodbarPixmap(moodbar_colors_, slider_->size(), slider_->palette(), opt);
    moodbar_pixmap_dirty_ = false;
  }

}

QRect MoodbarProxyStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget) const {

  if (cc != QStyle::CC_Slider || widget != slider_) {
    return QProxyStyle::subControlRect(cc, opt, sc, widget);
  }

  switch (state_) {
    case State::MoodbarOff:
    case State::FadingToOff:
      break;

    case State::MoodbarOn:
    case State::FadingToOn:
      switch (sc) {
        case SC_SliderGroove:
          return opt->rect.adjusted(kMarginSize, kMarginSize, -kMarginSize, -kMarginSize);

        case SC_SliderHandle:{
          const QStyleOptionSlider *slider_opt = qstyleoption_cast<const QStyleOptionSlider*>(opt);
          int x_offset = 0;

          // slider_opt->{maximum,minimum} can have the value 0 (their default values), so this check avoids a division by 0.
          if (slider_opt->maximum > slider_opt->minimum) {
            qint64 slider_delta = slider_opt->sliderValue - slider_opt->minimum;
            qint64 slider_range = slider_opt->maximum - slider_opt->minimum;
            int rectangle_effective_width = opt->rect.width() - kArrowWidth;

            qint64 x = slider_delta * rectangle_effective_width / slider_range;
            x_offset = static_cast<int>(x);
          }

          return QRect(QPoint(opt->rect.left() + x_offset, opt->rect.top()), QSize(kArrowWidth, kArrowHeight));
        }

        default:
          break;
      }
  }

  return QProxyStyle::subControlRect(cc, opt, sc, widget);

}

void MoodbarProxyStyle::DrawArrow(const QStyleOptionSlider *option, QPainter *painter) const {

  // Get the dimensions of the arrow
  const QRect rect = subControlRect(CC_Slider, option, SC_SliderHandle, slider_);

  // Make a polygon
  QPolygon poly;
  poly << rect.topLeft() << rect.topRight() << QPoint(rect.center().x(), rect.bottom());

  // Draw it
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->translate(0.5, 0.5);
  painter->setPen(Qt::black);
  painter->setBrush(slider_->palette().brush(QPalette::Active, QPalette::Base));
  painter->drawPolygon(poly);
  painter->restore();

}

QPixmap MoodbarProxyStyle::MoodbarPixmap(const ColorVector &colors, const QSize size, const QPalette &palette, const QStyleOptionSlider *opt) {

  Q_UNUSED(opt);

  QRect rect(QPoint(0, 0), size);
  QRect border_rect(rect);
  border_rect.adjust(kMarginSize, kMarginSize, -kMarginSize, -kMarginSize);

  QRect inner_rect(border_rect);
  inner_rect.adjust(kBorderSize, kBorderSize, -kBorderSize, -kBorderSize);

  QPixmap ret(size);
  QPainter p(&ret);

  // Draw the moodbar
  MoodbarRenderer::Render(colors, &p, inner_rect);

  // Draw the border
  p.setPen(QPen(Qt::black, kBorderSize, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  p.drawRect(border_rect.adjusted(0, 0, -1, -1));

  // Draw the outer bit
  p.setPen(QPen(palette.brush(QPalette::Active, QPalette::Window), kMarginSize, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));

  p.drawRect(rect.adjusted(1, 1, -2, -2));

  p.end();

  return ret;

}

void MoodbarProxyStyle::ShowContextMenu(const QPoint pos) {

  if (!context_menu_) {
    context_menu_ = new QMenu(slider_);
    show_moodbar_action_ = context_menu_->addAction(tr("Show moodbar"), this, &MoodbarProxyStyle::SetShowMoodbar);

    show_moodbar_action_->setCheckable(true);
    show_moodbar_action_->setChecked(show_);

    QMenu *styles_menu = context_menu_->addMenu(tr("Moodbar style"));
    style_action_group_ = new QActionGroup(styles_menu);

    for (int i = 0; i < static_cast<int>(MoodbarSettings::Style::StyleCount); ++i) {
      const MoodbarSettings::Style style = static_cast<MoodbarSettings::Style>(i);

      QAction *action = style_action_group_->addAction(MoodbarRenderer::StyleName(style));
      action->setCheckable(true);
      action->setData(i);
    }

    styles_menu->addActions(style_action_group_->actions());

    QObject::connect(styles_menu, &QMenu::triggered, this, &MoodbarProxyStyle::SetStyle);
  }

  // Update the currently selected style
  const QList<QAction*> actions = style_action_group_->actions();
  for (QAction *action : actions) {
    if (static_cast<MoodbarSettings::Style>(action->data().toInt()) == moodbar_style_) {
      action->setChecked(true);
      break;
    }
  }

  context_menu_->popup(pos);

}

void MoodbarProxyStyle::SetStyle(QAction *action) {

  Settings s;
  s.beginGroup(MoodbarSettings::kSettingsGroup);
  s.setValue(MoodbarSettings::kStyle, action->data().toInt());
  s.endGroup();

  ReloadSettings();

  Q_EMIT StyleChanged();

}
