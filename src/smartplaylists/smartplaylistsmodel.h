/*
 * Strawberry Music Player
 * This code was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SMARTPLAYLISTSMODEL_H
#define SMARTPLAYLISTSMODEL_H

#include "config.h"

#include <QAbstractItemModel>
#include <QList>
#include <QVariant>
#include <QStringList>
#include <QSettings>
#include <QIcon>

#include "includes/shared_ptr.h"
#include "core/simpletreemodel.h"
#include "core/settings.h"
#include "smartplaylistsitem.h"
#include "playlistgenerator_fwd.h"

class CollectionBackend;

class QModelIndex;
class QMimeData;

class SmartPlaylistsModel : public SimpleTreeModel<SmartPlaylistsItem> {
  Q_OBJECT

 public:
  explicit SmartPlaylistsModel(SharedPtr<CollectionBackend> backend, QObject *parent = nullptr);
  ~SmartPlaylistsModel();

  void Init();

  enum Role {
    Role_Type = Qt::UserRole + 1,
    Role_SortText,
    Role_DisplayText,
    Role_Smartplaylist,
    LastRole
  };

  using GeneratorList = QList<PlaylistGeneratorPtr>;
  using DefaultGenerators = QList<GeneratorList>;

  PlaylistGeneratorPtr CreateGenerator(const QModelIndex &idx) const;
  void AddGenerator(PlaylistGeneratorPtr gen);
  void UpdateGenerator(const QModelIndex &idx, PlaylistGeneratorPtr gen);
  void DeleteGenerator(const QModelIndex &idx);

 private:
  QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;

 private:
  static const char *kSettingsGroup;
  static const char *kSmartPlaylistsMimeType;
  static const int kSmartPlaylistsVersion;

  static void SaveGenerator(Settings *s, const int i, PlaylistGeneratorPtr generator);
  void ItemFromSmartPlaylist(const Settings &s, const bool notify);

 private:
  SharedPtr<CollectionBackend> collection_backend_;
  QIcon icon_;
  DefaultGenerators default_smart_playlists_;
  QList<SmartPlaylistsItem*> items_;
};

#endif  // SMARTPLAYLISTSMODEL_H
