/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOMODEL_H
#define RADIOMODEL_H

#include <QObject>
#include <QPair>
#include <QSet>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QPixmap>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/simpletreemodel.h"
#include "covermanager/albumcoverloaderresult.h"
#include "radioitem.h"
#include "radiochannel.h"

class QMimeData;

class AlbumCoverLoader;
class RadioServices;

class RadioModel : public SimpleTreeModel<RadioItem> {
  Q_OBJECT

 public:
  explicit RadioModel(const SharedPtr<AlbumCoverLoader> albumcover_loader, const SharedPtr<RadioServices> radio_services, QObject *parent = nullptr);
  ~RadioModel() override;

  enum Role {
    Role_Type = Qt::UserRole + 1,
    Role_SortText,
    Role_Source,
    Role_Url,
    Role_Homepage,
    Role_Donate,
    RoleCount
  };

  // QAbstractItemModel
  Qt::ItemFlags flags(const QModelIndex &idx) const override;
  QVariant data(const QModelIndex &idx, int role) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  QVariant data(const RadioItem *item, int role) const;

  void Reset();
  void AddChannels(const RadioChannelList &channels);

 private:
  bool IsPlayable(const QModelIndex &idx) const;
  bool CompareItems(const RadioItem *a, const RadioItem *b) const;
  void GetChildSongs(RadioItem *item, QList<QUrl> *urls, SongList *songs) const;
  SongList GetChildSongs(const QModelIndexList &indexes) const;
  SongList GetChildSongs(const QModelIndex &idx) const;
  QString ChannelIconPixmapCacheKey(const QModelIndex &idx) const;
  QPixmap ServiceIcon(const QModelIndex &idx) const;
  QPixmap ServiceIcon(RadioItem *item) const;
  QPixmap ChannelIcon(const QModelIndex &idx);
  QString SortText(QString text);

 private Q_SLOTS:
  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result);

 private:
  using ItemAndCacheKey = QPair<RadioItem*, QString>;

  const SharedPtr<AlbumCoverLoader> albumcover_loader_;
  const SharedPtr<RadioServices> radio_services_;

  QMap<Song::Source, RadioItem*> container_nodes_;
  QList<RadioItem*> items_;
  QMap<quint64, ItemAndCacheKey> pending_art_;
  QSet<QString> pending_cache_keys_;
};

#endif  // RADIOMODEL_H
