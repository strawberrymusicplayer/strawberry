/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef INFOTEXTVIEW_H
#define INFOTEXTVIEW_H

#include <QTextBrowser>
#include <QString>
#include <QUrl>

class QResizeEvent;
class QWheelEvent;

class InfoTextView : public QTextBrowser {
  Q_OBJECT

 public:
  explicit InfoTextView(QWidget *parent = nullptr);

  QSize sizeHint() const override;

 public slots:
  void SetHtml(const QString &html);

 protected:
  void resizeEvent(QResizeEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  QVariant loadResource(int type, const QUrl &name) override;

 private:
  int last_width_;
  bool recursion_filter_;
};

#endif  // INFOTEXTVIEW_H
