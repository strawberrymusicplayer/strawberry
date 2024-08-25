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

#ifndef SEARCHFIELD_H
#define SEARCHFIELD_H

#include <QWidget>
#include <QPointer>
#include <QMenu>

class QResizeEvent;
class SearchFieldPrivate;

class SearchField : public QWidget {
  Q_OBJECT

  Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
  Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)

 public:
  explicit SearchField(QWidget *parent);

  void setIconSize(const int iconsize);

  QString text() const;
  QString placeholderText() const;

#ifndef Q_OS_MACOS
  bool hasFocus() const;
#endif
  void setFocus(Qt::FocusReason);

 public Q_SLOTS:
  void setText(const QString &new_text);
  void setPlaceholderText(const QString &text);
  void clear();
  void selectAll();
  void setFocus();

 Q_SIGNALS:
  void textChanged(const QString &text);
  void editingFinished();
  void returnPressed();

 protected:
  void resizeEvent(QResizeEvent*) override;
  bool eventFilter(QObject *o, QEvent *e) override;

 private:
  friend class SearchFieldPrivate;
  SearchFieldPrivate *pimpl;
};

#endif  // SEARCHFIELD_H
