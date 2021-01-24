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

#ifndef PRETTYIMAGE_H
#define PRETTYIMAGE_H

#include <QWidget>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>

class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QContextMenuEvent;
class QPaintEvent;

class PrettyImage : public QWidget {
  Q_OBJECT

 public:
  PrettyImage(const QUrl &url, QNetworkAccessManager *network, QWidget *parent = nullptr);

  static const int kTotalHeight;
  static const int kReflectionHeight;
  static const int kImageHeight;

  static const int kMaxImageWidth;

  static const char *kSettingsGroup;

  QSize sizeHint() const override;
  QSize image_size() const;

signals:
  void Loaded();

 public slots:
  void LazyLoad();
  void SaveAs();
  void ShowFullsize();

 protected:
  void contextMenuEvent(QContextMenuEvent*) override;
  void paintEvent(QPaintEvent*) override;

 private slots:
  void ImageFetched(QNetworkReply *reply);
  void ImageScaled(QImage image);

 private:
  enum State {
    State_WaitingForLazyLoad,
    State_Fetching,
    State_CreatingThumbnail,
    State_Finished,
  };

  void DrawThumbnail(QPainter *p, const QRect &rect);

 private:
  QNetworkAccessManager *network_;
  State state_;
  QUrl url_;

  QImage image_;
  QPixmap thumbnail_;

  QMenu *menu_;
  QString last_save_dir_;
};

#endif  // PRETTYIMAGE_H
