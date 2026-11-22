// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>

#include <QButtonGroup>
#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJniObject>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QRectF>
#include <QResizeEvent>
#include <QString>
#include <QVBoxLayout>
#include <QtCore/qcoreapplication_platform.h>

#include "androidwidgets.h"
#include "timedialmath_android.h"
#include "timedial_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr double kPi = 3.14159265358979323846;
// Landscape phones leave little height below the breadcrumb, so the dial may shrink considerably.
constexpr int kMinimumDialSize = 120;
constexpr int kPreferredDialSize = 360;

QPointF PointAt(const QPointF &center, const double angle, const double distance) {

  const double radians = angle * kPi / 180.0;
  return center + QPointF(std::sin(radians) * distance, -std::cos(radians) * distance);

}

QString TwoDigits(const int value) {
  return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
}

// The selected part of the digital time and the AM or PM choice use the theme's highlight colors.
const QString kSegmentStyle =
    u"QPushButton { border: none; border-radius: 12px; padding: 4px 10px; background: palette(button); color: palette(button-text); }"
     "QPushButton:checked { background: palette(highlight); color: palette(highlighted-text); }"_s;

}  // namespace

TimeDial::TimeDial(QWidget *parent)
    : QWidget(parent),
      mode_(Mode::Hours),
      use_24_hour_(true),
      hour_(0),
      minute_(0),
      tracking_(false) {

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  // Keeping the focus on the page lets Back reach it.
  setFocusPolicy(Qt::NoFocus);

}

QSize TimeDial::sizeHint() const {
  return QSize(kPreferredDialSize, kPreferredDialSize);
}

QSize TimeDial::minimumSizeHint() const {
  return QSize(kMinimumDialSize, kMinimumDialSize);
}

void TimeDial::SetUse24Hour(const bool use_24_hour) {

  use_24_hour_ = use_24_hour;
  update();

}

void TimeDial::SetTime(const int hour, const int minute) {

  hour_ = ((hour % 24) + 24) % 24;
  minute_ = ((minute % 60) + 60) % 60;
  update();

}

void TimeDial::SetMode(const Mode mode) {

  mode_ = mode;
  tracking_ = false;
  setAccessibleName(mode_ == Mode::Hours ? tr("Hour dial") : tr("Minute dial"));
  update();

}

bool TimeDial::ShowsInnerRing() const {
  return mode_ == Mode::Hours && use_24_hour_;
}

TimeDial::Geometry TimeDial::DialGeometry() const {

  Geometry geometry;
  const double side = qMin(width(), height());
  geometry.center = QRectF(rect()).center();
  geometry.radius = side / 2.0 - 2.0;
  // The numbers are touch targets, so they are as large as the ring allows.
  geometry.number_radius = geometry.radius * (ShowsInnerRing() ? 0.13 : 0.15);
  geometry.outer_radius = geometry.radius - geometry.number_radius - geometry.radius * 0.03;
  geometry.inner_radius = geometry.outer_radius - geometry.number_radius * 2.1;
  return geometry;

}

void TimeDial::paintEvent(QPaintEvent *event) {

  Q_UNUSED(event)

  const Geometry g = DialGeometry();
  const QPalette &pal = palette();
  QColor face = pal.color(QPalette::AlternateBase);
  if (face == pal.color(QPalette::Base)) face = pal.color(QPalette::Button);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  painter.setPen(Qt::NoPen);
  painter.setBrush(face);
  painter.drawEllipse(g.center, g.radius, g.radius);

  QFont outer_font = font();
  outer_font.setPixelSize(qMax(10, static_cast<int>(g.number_radius * 0.9)));
  QFont inner_font = font();
  inner_font.setPixelSize(qMax(9, static_cast<int>(g.number_radius * 0.72)));

  const auto draw_numbers = [this, &painter, &g, &outer_font, &inner_font](const QColor &color) {
    painter.setPen(color);
    for (int position = 0; position < TimeDialMath::kPositions; ++position) {
      const double angle = TimeDialMath::AngleOfPosition(position);
      QString outer_text;
      if (mode_ == Mode::Minutes) {
        outer_text = TwoDigits(position * 5);
      }
      else {
        outer_text = QString::number(position == 0 ? 12 : position);
      }
      const QPointF outer = PointAt(g.center, angle, g.outer_radius);
      painter.setFont(outer_font);
      painter.drawText(QRectF(outer.x() - g.number_radius, outer.y() - g.number_radius, g.number_radius * 2, g.number_radius * 2), Qt::AlignCenter, outer_text);
      if (ShowsInnerRing()) {
        const QPointF inner = PointAt(g.center, angle, g.inner_radius);
        painter.setFont(inner_font);
        painter.drawText(QRectF(inner.x() - g.number_radius, inner.y() - g.number_radius, g.number_radius * 2, g.number_radius * 2), Qt::AlignCenter, TwoDigits(position == 0 ? 0 : position + 12));
      }
    }
  };

  draw_numbers(pal.color(QPalette::Text));

  double angle = 0.0;
  double distance = g.outer_radius;
  if (mode_ == Mode::Minutes) {
    angle = TimeDialMath::AngleOfMinute(minute_);
  }
  else {
    angle = TimeDialMath::AngleOfPosition(TimeDialMath::PositionOfHour(hour_));
    if (use_24_hour_ && TimeDialMath::IsInnerRingHour(hour_)) distance = g.inner_radius;
  }
  const QPointF selected = PointAt(g.center, angle, distance);
  const QColor highlight = pal.color(QPalette::Highlight);

  QPen hand(highlight);
  hand.setWidthF(qMax(2.0, g.radius * 0.012));
  painter.setPen(hand);
  painter.drawLine(g.center, selected);
  painter.setPen(Qt::NoPen);
  painter.setBrush(highlight);
  painter.drawEllipse(g.center, qMax(3.0, g.radius * 0.025), qMax(3.0, g.radius * 0.025));
  painter.drawEllipse(selected, g.number_radius, g.number_radius);

  // The numbers covered by the selection circle are drawn again in the highlighted text color.
  QPainterPath clip;
  clip.addEllipse(selected, g.number_radius, g.number_radius);
  painter.save();
  painter.setClipPath(clip);
  draw_numbers(pal.color(QPalette::HighlightedText));
  painter.restore();

  // A minute between the labels is marked with a dot.
  if (mode_ == Mode::Minutes && minute_ % 5 != 0) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(pal.color(QPalette::HighlightedText));
    painter.drawEllipse(selected, qMax(2.5, g.number_radius * 0.15), qMax(2.5, g.number_radius * 0.15));
  }

}

void TimeDial::Pick(const QPointF &position) {

  const Geometry g = DialGeometry();
  const QPointF offset = position - g.center;
  const double distance = std::hypot(offset.x(), offset.y());
  // The direction is undefined at the center.
  if (distance < g.radius * 0.08) return;
  const double angle = TimeDialMath::AngleFromOffset(offset.x(), offset.y());

  int hour = hour_;
  int minute = minute_;
  if (mode_ == Mode::Hours) {
    hour = TimeDialMath::HourAt(angle, distance, g.outer_radius, g.inner_radius, use_24_hour_, TimeDialMath::IsPm(hour_));
  }
  else {
    minute = TimeDialMath::MinuteFromAngle(angle);
  }
  if (hour == hour_ && minute == minute_) return;
  hour_ = hour;
  minute_ = minute;
  update();
  Q_EMIT TimeChanged(hour_, minute_);

}

void TimeDial::mousePressEvent(QMouseEvent *event) {

  const Geometry g = DialGeometry();
  const QPointF offset = event->position() - g.center;
  if (event->button() != Qt::LeftButton || std::hypot(offset.x(), offset.y()) > g.radius * 1.1) {
    QWidget::mousePressEvent(event);
    return;
  }
  tracking_ = true;
  Pick(event->position());
  event->accept();

}

void TimeDial::mouseMoveEvent(QMouseEvent *event) {

  if (!tracking_) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  Pick(event->position());
  event->accept();

}

void TimeDial::mouseReleaseEvent(QMouseEvent *event) {

  if (!tracking_ || event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  Pick(event->position());
  tracking_ = false;
  event->accept();
  if (mode_ == Mode::Hours) Q_EMIT HourPicked();

}

TimePicker::TimePicker(const bool use_24_hour, QWidget *parent)
    : QWidget(parent),
      use_24_hour_(use_24_hour),
      layout_(new QGridLayout(this)),
      header_(new QWidget(this)),
      dial_(new TimeDial(this)),
      hour_button_(new QPushButton(header_)),
      minute_button_(new QPushButton(header_)),
      am_button_(new QPushButton(header_)),
      pm_button_(new QPushButton(header_)),
      done_button_(new QPushButton(tr("Done"), this)),
      colon_(new QLabel(u":"_s, header_)),
      base_point_size_(font().pointSizeF()),
      landscape_(-1) {

  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(12);
  dial_->SetUse24Hour(use_24_hour_);

  QHBoxLayout *header_layout = new QHBoxLayout(header_);
  header_layout->setContentsMargins(0, 0, 0, 0);
  header_layout->setSpacing(6);
  header_layout->addStretch();

  QButtonGroup *segments = new QButtonGroup(this);
  for (QPushButton *button : {hour_button_, minute_button_}) {
    button->setCheckable(true);
    button->setStyleSheet(kSegmentStyle);
    button->setFocusPolicy(Qt::NoFocus);
    segments->addButton(button);
  }
  header_layout->addWidget(hour_button_);
  header_layout->addWidget(colon_);
  header_layout->addWidget(minute_button_);

  QFont period_font = am_button_->font();
  period_font.setPointSizeF(period_font.pointSizeF() * 1.2);
  QVBoxLayout *period_layout = new QVBoxLayout;
  period_layout->setSpacing(4);
  QButtonGroup *periods = new QButtonGroup(this);
  const QLocale locale;
  am_button_->setText(locale.amText().isEmpty() ? u"AM"_s : locale.amText());
  pm_button_->setText(locale.pmText().isEmpty() ? u"PM"_s : locale.pmText());
  for (QPushButton *button : {am_button_, pm_button_}) {
    button->setCheckable(true);
    button->setFont(period_font);
    button->setStyleSheet(kSegmentStyle);
    button->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
    button->setFocusPolicy(Qt::NoFocus);
    button->setVisible(!use_24_hour_);
    periods->addButton(button);
    period_layout->addWidget(button);
  }
  header_layout->addSpacing(8);
  header_layout->addLayout(period_layout);
  header_layout->addStretch();

  done_button_->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  done_button_->setFocusPolicy(Qt::NoFocus);

  QObject::connect(hour_button_, &QPushButton::clicked, this, [this]() { ShowMode(TimeDial::Mode::Hours); });
  QObject::connect(minute_button_, &QPushButton::clicked, this, [this]() { ShowMode(TimeDial::Mode::Minutes); });
  const auto set_pm = [this](const bool pm) {
    const int hour = TimeDialMath::WithPm(dial_->hour(), pm);
    if (hour == dial_->hour()) return;
    dial_->SetTime(hour, dial_->minute());
    UpdateTexts();
    Q_EMIT TimeChanged(dial_->hour(), dial_->minute());
  };
  QObject::connect(am_button_, &QPushButton::clicked, this, [set_pm]() { set_pm(false); });
  QObject::connect(pm_button_, &QPushButton::clicked, this, [set_pm]() { set_pm(true); });
  QObject::connect(dial_, &TimeDial::TimeChanged, this, [this](const int hour, const int minute) {
    UpdateTexts();
    Q_EMIT TimeChanged(hour, minute);
  });
  QObject::connect(dial_, &TimeDial::HourPicked, this, [this]() { ShowMode(TimeDial::Mode::Minutes); });
  QObject::connect(done_button_, &QPushButton::clicked, this, &TimePicker::Done);

  ShowMode(TimeDial::Mode::Hours);
  UpdateLayout();

}

bool TimePicker::DeviceUses24HourFormat() {

  const QJniObject context = QNativeInterface::QAndroidApplication::context();
  return QJniObject::callStaticMethod<jboolean>("android/text/format/DateFormat", "is24HourFormat", "(Landroid/content/Context;)Z", context.object());

}

void TimePicker::SetTime(const int hour, const int minute) {

  dial_->SetTime(hour, minute);
  UpdateTexts();

}

int TimePicker::hour() const {
  return dial_->hour();
}

int TimePicker::minute() const {
  return dial_->minute();
}

void TimePicker::ShowMode(const TimeDial::Mode mode) {

  dial_->SetMode(mode);
  hour_button_->setChecked(mode == TimeDial::Mode::Hours);
  minute_button_->setChecked(mode == TimeDial::Mode::Minutes);

}

void TimePicker::UpdateTexts() {

  const int hour = dial_->hour();
  hour_button_->setText(use_24_hour_ ? TwoDigits(hour) : QString::number(TimeDialMath::To12Hour(hour)));
  minute_button_->setText(TwoDigits(dial_->minute()));
  hour_button_->setAccessibleName(tr("Hour %1").arg(hour_button_->text()));
  minute_button_->setAccessibleName(tr("Minute %1").arg(minute_button_->text()));
  const bool pm = TimeDialMath::IsPm(hour);
  am_button_->setChecked(!pm);
  pm_button_->setChecked(pm);

}

void TimePicker::resizeEvent(QResizeEvent *event) {

  QWidget::resizeEvent(event);
  UpdateLayout();

}

void TimePicker::UpdateLayout() {

  const int landscape = width() > height() * 6 / 5 ? 1 : 0;
  if (landscape == landscape_) return;
  landscape_ = landscape;

  // The digital time is smaller in landscape, so the dial keeps most of the little height.
  QFont time_font = font();
  time_font.setPointSizeF(base_point_size_ * (landscape_ ? 2.0 : 3.2));
  const int segment_size = landscape_ ? AndroidWidgets::kMinimumTouchSize : AndroidWidgets::kMinimumTouchSize * 3 / 2;
  for (QPushButton *button : {hour_button_, minute_button_}) {
    button->setFont(time_font);
    button->setMinimumSize(segment_size, segment_size);
  }
  colon_->setFont(time_font);

  layout_->removeWidget(header_);
  layout_->removeWidget(dial_);
  layout_->removeWidget(done_button_);
  for (int i = 0; i < 3; ++i) {
    layout_->setRowStretch(i, 0);
    layout_->setColumnStretch(i, 0);
  }
  if (landscape_) {
    // The time and Done stay at the left, next to the dial, which takes the full height.
    layout_->addWidget(header_, 0, 0, Qt::AlignVCenter);
    layout_->addWidget(done_button_, 2, 0);
    layout_->addWidget(dial_, 0, 1, 3, 1);
    layout_->setRowStretch(1, 1);
    layout_->setColumnStretch(1, 1);
  }
  else {
    layout_->addWidget(header_, 0, 0);
    layout_->addWidget(dial_, 1, 0);
    layout_->addWidget(done_button_, 2, 0);
    layout_->setRowStretch(1, 1);
  }

}
