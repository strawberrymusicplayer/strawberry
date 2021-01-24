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

#ifndef PRETTYIMAGEVIEW_H
#define PRETTYIMAGEVIEW_H

#include <QScrollArea>
#include <QMap>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QMenu;
class QHBoxLayout;
class QPropertyAnimation;
class QTimeLine;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;

class PrettyImageView : public QScrollArea {
  Q_OBJECT

 public:
  PrettyImageView(QNetworkAccessManager *network, QWidget *parent = nullptr);

  static const char* kSettingsGroup;

 public slots:
  void AddImage(const QUrl& url);

 protected:
  void mouseReleaseEvent(QMouseEvent*) override;
  void resizeEvent(QResizeEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;

 private slots:
  void ScrollBarReleased();
  void ScrollBarAction(const int action);
  void ScrollTo(const int index, const bool smooth = true);
  void ScrollToCurrent();

 private:
  bool eventFilter(QObject*, QEvent*) override;

  QNetworkAccessManager *network_;

  QWidget *container_;
  QHBoxLayout *layout_;

  int current_index_;
  QPropertyAnimation *scroll_animation_;

  bool recursion_filter_;
};

#endif  // PRETTYIMAGEVIEW_H
