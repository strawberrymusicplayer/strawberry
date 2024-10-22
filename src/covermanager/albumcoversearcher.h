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

#ifndef ALBUMCOVERSEARCHER_H
#define ALBUMCOVERSEARCHER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QDialog>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QMap>
#include <QByteArray>
#include <QString>
#include <QImage>
#include <QIcon>

#include "albumcoverfetcher.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"
#include "albumcoverimageresult.h"

class QWidget;
class QStandardItem;
class QStandardItemModel;
class QPainter;
class QModelIndex;
class QKeyEvent;

class AlbumCoverLoader;
class Ui_AlbumCoverSearcher;

class SizeOverlayDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit SizeOverlayDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
};

// This is a dialog that lets the user search for album covers
class AlbumCoverSearcher : public QDialog {
  Q_OBJECT

 public:
  explicit AlbumCoverSearcher(const QIcon &no_cover_icon, const SharedPtr<AlbumCoverLoader> albumcover_loader, QWidget *parent);
  ~AlbumCoverSearcher() override;

  enum Role {
    Role_ImageURL = Qt::UserRole + 1,
    Role_ImageRequestId,
    Role_ImageFetchFinished,
    Role_ImageData,
    Role_Image,
    Role_ImageDimensions,
    Role_ImageSize
  };

  void Init(AlbumCoverFetcher *fetcher);

  AlbumCoverImageResult Exec(const QString &artist, const QString &album);

 protected:
  void keyPressEvent(QKeyEvent*) override;

 private Q_SLOTS:
  void Search();
  void SearchFinished(const quint64 id, const CoverProviderSearchResults &results);
  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result);

  void CoverDoubleClicked(const QModelIndex &idx);

 private:
  Ui_AlbumCoverSearcher *ui_;

  const SharedPtr<AlbumCoverLoader> albumcover_loader_;
  QStandardItemModel *model_;

  QIcon no_cover_icon_;
  AlbumCoverFetcher *fetcher_;

  quint64 id_;
  QMap<quint64, QStandardItem*> cover_loading_tasks_;
};

#endif  // ALBUMCOVERSEARCHER_H
