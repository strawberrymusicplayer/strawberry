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

#include <QLineEdit>
#include <QPushButton>
#include <QStyle>

#include "searchfield_qt_private.h"
#include "searchfield.h"

SearchFieldPrivate::SearchFieldPrivate(SearchField *searchField, QLineEdit *lineedit, QPushButton *clearbutton)
      : QObject(searchField), lineedit_(lineedit), clearbutton_(clearbutton) {}

int SearchFieldPrivate::lineEditFrameWidth() const {
  return lineedit_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
}

int SearchFieldPrivate::clearButtonPaddedWidth() const {
  return clearbutton_->width() + lineEditFrameWidth() * 2;
}

int SearchFieldPrivate::clearButtonPaddedHeight() const {
  return clearbutton_->height() + lineEditFrameWidth() * 2;
}
