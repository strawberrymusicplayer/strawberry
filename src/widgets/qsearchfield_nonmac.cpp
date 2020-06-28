/*
Copyright (C) 2011 by Mike McQuaid

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

#include "qsearchfield.h"

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QApplication>
#include <QString>
#include <QIcon>
#include <QPointer>
#include <QLineEdit>
#include <QToolButton>
#include <QStyle>
#include <QSize>
#include <QBoxLayout>
#include <QEvent>
#include <QResizeEvent>

#include "core/iconloader.h"

class QSearchFieldPrivate : public QObject {
 public:
  QSearchFieldPrivate(QSearchField *searchField, QLineEdit *lineedit, QToolButton *clearbutton)
      : QObject(searchField), lineedit_(lineedit), clearbutton_(clearbutton) {}

  int lineEditFrameWidth() const {
    return lineedit_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  }

  int clearButtonPaddedWidth() const {
    return clearbutton_->width() + lineEditFrameWidth() * 2;
  }

  int clearButtonPaddedHeight() const {
    return clearbutton_->height() + lineEditFrameWidth() * 2;
  }

  QPointer<QLineEdit> lineedit_;
  QPointer<QToolButton> clearbutton_;

};

QSearchField::QSearchField(QWidget *parent) : QWidget(parent) {

  QLineEdit *lineEdit = new QLineEdit(this);
  connect(lineEdit, SIGNAL(textChanged(QString)), this, SIGNAL(textChanged(QString)));
  connect(lineEdit, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));
  connect(lineEdit, SIGNAL(returnPressed()), this, SIGNAL(returnPressed()));
  connect(lineEdit, SIGNAL(textChanged(QString)), this, SLOT(setText(QString)));

  QToolButton *clearbutton = new QToolButton(this);
  QIcon clearIcon(IconLoader::Load("edit-clear-locationbar-ltr"));

  clearbutton->setIcon(clearIcon);
  clearbutton->setIconSize(QSize(20, 20));
  clearbutton->setStyleSheet("border: none; padding: 0px;");
  clearbutton->resize(clearbutton->sizeHint());

  connect(clearbutton, SIGNAL(clicked()), this, SLOT(clear()));

  pimpl = new QSearchFieldPrivate(this, lineEdit, clearbutton);

  const int frame_width = lineEdit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

  lineEdit->setStyleSheet(QString("QLineEdit { padding-left: %1px; } ").arg(clearbutton->width()));
  const int width = frame_width + qMax(lineEdit->minimumSizeHint().width(), pimpl->clearButtonPaddedWidth());
  const int height = frame_width + qMax(lineEdit->minimumSizeHint().height(), pimpl->clearButtonPaddedHeight());
  lineEdit->setMinimumSize(width, height);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->addWidget(lineEdit);

  lineEdit->installEventFilter(this);

}

void QSearchField::setIconSize(const int iconsize) {

  pimpl->clearbutton_->setIconSize(QSize(iconsize, iconsize));
  pimpl->clearbutton_->resize(pimpl->clearbutton_->sizeHint());

  pimpl->lineedit_->setStyleSheet(QString("QLineEdit { padding-left: %1px; } ").arg(pimpl->clearbutton_->width()));
  const int frame_width = pimpl->lineedit_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const int width = frame_width + qMax(pimpl->lineedit_->minimumSizeHint().width(), pimpl->clearButtonPaddedWidth());
  const int height = frame_width + qMax(pimpl->lineedit_->minimumSizeHint().height(), pimpl->clearButtonPaddedHeight());
  pimpl->lineedit_->setMinimumSize(width, height);

}

void QSearchField::setText(const QString &text) {

  Q_ASSERT(pimpl && pimpl->clearbutton_ && pimpl->lineedit_);
  if (!(pimpl && pimpl->clearbutton_ && pimpl->lineedit_)) return;
  if (text != this->text()) pimpl->lineedit_->setText(text);

}

void QSearchField::setPlaceholderText(const QString &text) {

  Q_ASSERT(pimpl && pimpl->lineedit_);
  if (!(pimpl && pimpl->lineedit_)) return;
  pimpl->lineedit_->setPlaceholderText(text);

}

QString QSearchField::placeholderText() const {
  return pimpl->lineedit_->placeholderText();
}

void QSearchField::setFocus(Qt::FocusReason reason) {
  Q_ASSERT(pimpl && pimpl->lineedit_);
  if (pimpl && pimpl->lineedit_) pimpl->lineedit_->setFocus(reason);
}

void QSearchField::setFocus() {
  setFocus(Qt::OtherFocusReason);
}

void QSearchField::clear() {

  Q_ASSERT(pimpl && pimpl->lineedit_);

  if (!(pimpl && pimpl->lineedit_)) return;
  pimpl->lineedit_->clear();

}

void QSearchField::selectAll() {

  Q_ASSERT(pimpl && pimpl->lineedit_);

  if (!(pimpl && pimpl->lineedit_)) return;
  pimpl->lineedit_->selectAll();

}

QString QSearchField::text() const {

  Q_ASSERT(pimpl && pimpl->lineedit_);

  if (!(pimpl && pimpl->lineedit_)) return QString();
  return pimpl->lineedit_->text();

}

void QSearchField::resizeEvent(QResizeEvent *resizeEvent) {

  Q_ASSERT(pimpl && pimpl->clearbutton_ && pimpl->lineedit_);
  if (!(pimpl && pimpl->clearbutton_ && pimpl->lineedit_)) return;

  QWidget::resizeEvent(resizeEvent);
  const int x = pimpl->lineEditFrameWidth();
  const int y = (height() - pimpl->clearbutton_->height())/2;
  pimpl->clearbutton_->move(x, y);

}

bool QSearchField::eventFilter(QObject *o, QEvent *e) {

  if (pimpl && pimpl->lineedit_ && o == pimpl->lineedit_) {
    // Forward some lineEdit events to QSearchField (only those we need for
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
