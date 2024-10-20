/*
Copyright (C) 2011 by Mike McQuaid
Copyright (C) 2018-2024 by Jonas Kvinge <jonas@jkvinge.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "searchfield.h"
#include "searchfield_qt_private.h"

#include <QWidget>
#include <QApplication>
#include <QString>
#include <QIcon>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStyle>
#include <QSize>
#include <QEvent>
#include <QResizeEvent>

#include "core/iconloader.h"

using namespace Qt::Literals::StringLiterals;

SearchField::SearchField(QWidget *parent) : QWidget(parent) {

  QLineEdit *lineEdit = new QLineEdit(this);
  QObject::connect(lineEdit, &QLineEdit::textChanged, this, &SearchField::textChanged);
  QObject::connect(lineEdit, &QLineEdit::editingFinished, this, &SearchField::editingFinished);
  QObject::connect(lineEdit, &QLineEdit::returnPressed, this, &SearchField::returnPressed);
  QObject::connect(lineEdit, &QLineEdit::textChanged, this, &SearchField::setText);

  QPushButton *clearbutton = new QPushButton(this);
  QIcon clearIcon(IconLoader::Load(u"edit-clear-locationbar-ltr"_s));

  clearbutton->setIcon(clearIcon);
  clearbutton->setIconSize(QSize(20, 20));
  clearbutton->setStyleSheet(u"border: none; padding: 2px;"_s);
  clearbutton->resize(clearbutton->sizeHint());

  QObject::connect(clearbutton, &QPushButton::clicked, this, &SearchField::clear);

  pimpl = new SearchFieldPrivate(this, lineEdit, clearbutton);

  const int frame_width = lineEdit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

  lineEdit->setStyleSheet(QStringLiteral("QLineEdit { padding-left: %1px; } ").arg(clearbutton->width()));
  const int width = frame_width + qMax(lineEdit->minimumSizeHint().width(), pimpl->clearButtonPaddedWidth());
  const int height = frame_width + qMax(lineEdit->minimumSizeHint().height(), pimpl->clearButtonPaddedHeight());
  lineEdit->setMinimumSize(width, height);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(lineEdit);

  lineEdit->installEventFilter(this);

}

void SearchField::setIconSize(const int iconsize) {

  pimpl->clearbutton_->setIconSize(QSize(iconsize, iconsize));
  pimpl->clearbutton_->resize(pimpl->clearbutton_->sizeHint());

  pimpl->lineedit_->setStyleSheet(QStringLiteral("QLineEdit { padding-left: %1px; } ").arg(pimpl->clearbutton_->width()));
  const int frame_width = pimpl->lineedit_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const int width = frame_width + qMax(pimpl->lineedit_->minimumSizeHint().width(), pimpl->clearButtonPaddedWidth());
  const int height = frame_width + qMax(pimpl->lineedit_->minimumSizeHint().height(), pimpl->clearButtonPaddedHeight());
  pimpl->lineedit_->setMinimumSize(width, height);

}

void SearchField::setText(const QString &new_text) {

  Q_ASSERT(pimpl && pimpl->clearbutton_ && pimpl->lineedit_);
  if (!(pimpl && pimpl->clearbutton_ && pimpl->lineedit_)) return;
  if (new_text != text()) pimpl->lineedit_->setText(new_text);

}

void SearchField::setPlaceholderText(const QString &text) {

  Q_ASSERT(pimpl && pimpl->lineedit_);
  if (!(pimpl && pimpl->lineedit_)) return;
  pimpl->lineedit_->setPlaceholderText(text);

}

QString SearchField::placeholderText() const {
  return pimpl->lineedit_->placeholderText();
}

bool SearchField::hasFocus() const {
  Q_ASSERT(pimpl && pimpl->lineedit_);
  return pimpl && pimpl->lineedit_ && pimpl->lineedit_->hasFocus();
}

void SearchField::setFocus(Qt::FocusReason reason) {
  Q_ASSERT(pimpl && pimpl->lineedit_);
  if (pimpl && pimpl->lineedit_) pimpl->lineedit_->setFocus(reason);
}

void SearchField::setFocus() {
  setFocus(Qt::OtherFocusReason);
}

void SearchField::clear() {

  Q_ASSERT(pimpl && pimpl->lineedit_);

  if (!(pimpl && pimpl->lineedit_)) return;
  pimpl->lineedit_->clear();

}

void SearchField::selectAll() {

  Q_ASSERT(pimpl && pimpl->lineedit_);

  if (!(pimpl && pimpl->lineedit_)) return;
  pimpl->lineedit_->selectAll();

}

QString SearchField::text() const {

  Q_ASSERT(pimpl && pimpl->lineedit_);

  if (!(pimpl && pimpl->lineedit_)) return QString();
  return pimpl->lineedit_->text();

}

void SearchField::resizeEvent(QResizeEvent *resizeEvent) {

  Q_ASSERT(pimpl && pimpl->clearbutton_ && pimpl->lineedit_);
  if (!(pimpl && pimpl->clearbutton_ && pimpl->lineedit_)) return;

  QWidget::resizeEvent(resizeEvent);
  const int x = pimpl->lineEditFrameWidth();
  const int y = (height() - pimpl->clearbutton_->height()) / 2;
  pimpl->clearbutton_->move(x, y);

}

bool SearchField::eventFilter(QObject *o, QEvent *e) {

  if (pimpl && pimpl->lineedit_ && o == pimpl->lineedit_) {
    // Forward some lineEdit events to SearchField (only those we need for
    // now, but some might be added later if needed)
    switch (e->type()) {
      case QEvent::FocusIn:
      case QEvent::FocusOut:
        QApplication::sendEvent(this, e);
        break;
      default:
        break;
    }
  }

  return QWidget::eventFilter(o, e);

}
