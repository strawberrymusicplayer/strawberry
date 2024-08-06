/*
 * Strawberry Music Player
 * Copyright 2022, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RESIZABLETEXTEDIT_H
#define RESIZABLETEXTEDIT_H

#include <QTextEdit>

class QResizeEvent;

class ResizableTextEdit : public QTextEdit {
  Q_OBJECT

 public:
  explicit ResizableTextEdit(QWidget *parent = nullptr);

  virtual QSize sizeHint() const override;

  QString text() const { return Text(); }
  QString Text() const { return text_; }
  void setText(const QString &text) { SetText(text); }
  void SetText(const QString &text);

 protected:
  virtual void resizeEvent(QResizeEvent *event) override;

 private:
  QString text_;
};

#endif  // RESIZABLETEXTEDIT_H
