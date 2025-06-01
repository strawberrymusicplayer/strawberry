/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QString>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QPaintDevice>
#include <QRect>
#include <QSize>
#include <QStyle>
#include <QStyleOption>
#include <QToolButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QFlags>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QGuiApplication>

#include "core/iconloader.h"
#include "lineedit.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kClearIconSize = 16;
constexpr int kResetIconSize = 16;
}  // namespace

ExtendedEditor::ExtendedEditor(QWidget *widget, int extra_right_padding, bool draw_hint)
    : LineEditInterface(widget),
      has_clear_button_(true),
      clear_button_(new QToolButton(widget)),
      reset_button_(new QToolButton(widget)),
      extra_right_padding_(extra_right_padding),
      draw_hint_(draw_hint),
      font_point_size_(widget->font().pointSizeF() - 1),
      is_rtl_(false) {

  clear_button_->setIcon(IconLoader::Load(u"edit-clear-locationbar-ltr"_s));
  clear_button_->setIconSize(QSize(kClearIconSize, kClearIconSize));
  clear_button_->setCursor(Qt::ArrowCursor);
  clear_button_->setStyleSheet(u"QToolButton { border: none; padding: 0px; }"_s);
  clear_button_->setToolTip(QWidget::tr("Clear"));
  clear_button_->setFocusPolicy(Qt::NoFocus);

  QStyleOption opt;
  opt.initFrom(widget);

  reset_button_->setIcon(widget->style()->standardIcon(QStyle::SP_DialogResetButton, &opt, widget));
  reset_button_->setIconSize(QSize(kResetIconSize, kResetIconSize));
  reset_button_->setCursor(Qt::ArrowCursor);
  reset_button_->setStyleSheet(u"QToolButton { border: none; padding: 0px; }"_s);
  reset_button_->setToolTip(QWidget::tr("Reset"));
  reset_button_->setFocusPolicy(Qt::NoFocus);
  reset_button_->hide();

  if (LineEdit *lineedit = qobject_cast<LineEdit*>(widget)) {
    QObject::connect(clear_button_, &QToolButton::clicked, lineedit, &LineEdit::set_focus);
    QObject::connect(clear_button_, &QToolButton::clicked, lineedit, &LineEdit::clear);
  }
  else if (TextEdit *textedit = qobject_cast<TextEdit*>(widget)) {
    QObject::connect(clear_button_, &QToolButton::clicked, textedit, &TextEdit::set_focus);
    QObject::connect(clear_button_, &QToolButton::clicked, textedit, &TextEdit::clear);
  }
  else if (SpinBox *spinbox = qobject_cast<SpinBox*>(widget)) {
    QObject::connect(clear_button_, &QToolButton::clicked, spinbox, &SpinBox::set_focus);
    QObject::connect(clear_button_, &QToolButton::clicked, spinbox, &SpinBox::clear);
  }

  UpdateButtonGeometry();

}

void ExtendedEditor::set_hint(const QString &hint) {
  hint_ = hint;
  widget_->update();
}

void ExtendedEditor::set_clear_button(const bool visible) {
  has_clear_button_ = visible;
  clear_button_->setVisible(visible);
  UpdateButtonGeometry();
}

bool ExtendedEditor::has_reset_button() const {
  return reset_button_->isVisible();
}

void ExtendedEditor::set_reset_button(const bool visible) {
  reset_button_->setVisible(visible);
  UpdateButtonGeometry();
}

void ExtendedEditor::UpdateButtonGeometry() {

  const int frame_width = widget_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const int left = frame_width + 1 + (has_clear_button() ? clear_button_->sizeHint().width() : 0);
  const int right = frame_width + 1 + (has_reset_button() ? reset_button_->sizeHint().width() : 0);
  const char *const class_name = widget_->metaObject()->className();

  if (strcmp(class_name, "LineEdit") == 0) {
    // Seems Qt inverts left/right padding for QLineEdit if layout direction RTL
    const bool rtl = QGuiApplication::isRightToLeft();
    widget_->setStyleSheet(QStringLiteral("QLineEdit { padding-left: %1px; padding-right: %2px; }").arg(rtl ? right : left).arg(rtl ? left : right));
  }
  else if (strcmp(class_name, "TextEdit") == 0) {
    // But not for QPlainTextEdit
    widget_->setStyleSheet(QStringLiteral("QPlainTextEdit { padding-left: %1px; padding-right: %2px; }").arg(left).arg(right));
  }

  QSize msz = widget_->minimumSizeHint();
  widget_->setMinimumSize(msz.width() + (clear_button_->sizeHint().width() + frame_width + 1) * 2 + extra_right_padding_, qMax(msz.height(), clear_button_->sizeHint().height() + frame_width * 2 + 2));

}

void ExtendedEditor::Paint(QPaintDevice *device) {

  if (!widget_->hasFocus() && is_empty() && !hint_.isEmpty()) {
    clear_button_->hide();

    if (draw_hint_) {
      QPainter p(device);

      QFont font;
      font.setBold(false);
      font.setPointSizeF(font_point_size_);

      QFontMetrics m(font);
      const int kBorder = (device->height() - m.height()) / 2;

      p.setPen(widget_->palette().color(QPalette::Disabled, QPalette::Text));
      p.setFont(font);

      QRect r(5, kBorder, device->width() - 10, device->height() - kBorder * 2);
      p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, m.elidedText(hint_, Qt::ElideRight, r.width()));
    }
  }
  else {
    clear_button_->setVisible(has_clear_button_);
  }

}

void ExtendedEditor::Resize() {

  const QSize sz = clear_button_->sizeHint();
  const int frame_width = widget_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const int y = (widget_->rect().height() - sz.height()) / 2;

  clear_button_->move(frame_width, y);

  if (!is_rtl_) {
    reset_button_->move(widget_->width() - frame_width - sz.width() - extra_right_padding_, y);
  }
  else {
    reset_button_->move((has_clear_button() ? sz.width() + 4 : 0) + frame_width, y);
  }

}

LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent), ExtendedEditor(this) {
  QObject::connect(reset_button_, &QToolButton::clicked, this, &LineEdit::Reset);
  QObject::connect(this, &LineEdit::textChanged, this, &LineEdit::text_changed);
}

void LineEdit::text_changed(const QString &text) {

  if (text.isEmpty()) {
    // Consider empty string as LTR
    set_rtl(false);
  }
  else {
    // For some reason Qt will detect any text with LTR at the end as LTR, so instead compare only the first character
    set_rtl(QString(text.at(0)).isRightToLeft());
  }
  Resize();

}

void LineEdit::paintEvent(QPaintEvent *e) {
  QLineEdit::paintEvent(e);
  Paint(this);
}

void LineEdit::resizeEvent(QResizeEvent *e) {
  QLineEdit::resizeEvent(e);
  Resize();
}


TextEdit::TextEdit(QWidget *parent)
    : QPlainTextEdit(parent),
      ExtendedEditor(this) {

  QObject::connect(reset_button_, &QToolButton::clicked, this, &TextEdit::Reset);
  QObject::connect(this, &TextEdit::textChanged, [this]() { viewport()->update(); });  // To clear the hint

}

void TextEdit::paintEvent(QPaintEvent *e) {
  QPlainTextEdit::paintEvent(e);
  Paint(viewport());
}

void TextEdit::resizeEvent(QResizeEvent *e) {
  QPlainTextEdit::resizeEvent(e);
  Resize();
}


SpinBox::SpinBox(QWidget *parent)
    : QSpinBox(parent),
      ExtendedEditor(this, 14, false) {

  if (QGuiApplication::isRightToLeft()) {
    extra_right_padding_ = 0; // Up/down arrows on left
  }
  QObject::connect(reset_button_, &QToolButton::clicked, this, &SpinBox::Reset);
}

QString SpinBox::textFromValue(int val) const {

  if (val <= 0 && !hint_.isEmpty()) {
    return u"-"_s;
  }
  return QSpinBox::textFromValue(val);

}

void SpinBox::paintEvent(QPaintEvent *e) {
  QSpinBox::paintEvent(e);
  Paint(this);
}

void SpinBox::resizeEvent(QResizeEvent *e) {
  QSpinBox::resizeEvent(e);
  Resize();
}

CheckBox::CheckBox(QWidget *parent)
    : QCheckBox(parent), ExtendedEditor(this, 4, false) {

  has_clear_button_ = false;
  is_rtl_ = QGuiApplication::isRightToLeft();
  QObject::connect(reset_button_, &QToolButton::clicked, this, &CheckBox::Reset);

}

void CheckBox::paintEvent(QPaintEvent *e) {
  QCheckBox::paintEvent(e);
  Paint(this);
}

void CheckBox::resizeEvent(QResizeEvent *e) {
  QCheckBox::resizeEvent(e);
  Resize();
}

void CheckBox::Resize() {

  const QSize sz = widget_->sizeHint();
  const int frame_width = widget_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const int y = (rect().height() - sz.height()) / 2 - frame_width; // Less frame width as outside

  if (!is_rtl_) {
    reset_button_->move(frame_width + sz.width() + extra_right_padding_, y); // Using `extra_right_padding_` as how far to right of checkbox
  }
  else {
    reset_button_->move(rect().width() - (frame_width + sz.width() + kResetIconSize + extra_right_padding_), y);
  }

}

RatingBox::RatingBox(QWidget *parent)
    : RatingWidget(parent),
      ExtendedEditor(this, 6) {

  has_clear_button_ = false;
  QObject::connect(reset_button_, &QToolButton::clicked, this, &RatingBox::Reset);

}

void RatingBox::paintEvent(QPaintEvent *e) {
  RatingWidget::paintEvent(e);
  Paint(this);
}

void RatingBox::resizeEvent(QResizeEvent *e) {
  RatingWidget::resizeEvent(e);
  Resize();
}

void RatingBox::Resize() {

  const QSize sz = widget_->sizeHint();
  const int frame_width = widget_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const int y = (rect().height() - sz.height()) / 2 + frame_width; // Plus frame width as inside

  reset_button_->move(frame_width + rect().width() - (kResetIconSize + extra_right_padding_), y);

}
