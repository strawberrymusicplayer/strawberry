/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef STREAMINGCOLLECTIONVIEW_H
#define STREAMINGCOLLECTIONVIEW_H

#include "config.h"

#include <QObject>
#include <QString>

#include "core/song.h"
#include "collection/collectiontreeview.h"

class QWidget;
class QMenu;
class QAction;

class CollectionModel;

class StreamingCollectionView : public CollectionTreeView {
  Q_OBJECT

 public:
  explicit StreamingCollectionView(QWidget *parent = nullptr);

  void Init(CollectionModel *collection_model, const bool favorite = false);

 public Q_SLOTS:
  void ReloadSettings();

 Q_SIGNALS:
  void GetSongs();
  void RemoveSongs(const SongList &songs);

 protected:
  QString EmptyTitleText() const override;
  QString EmptyText() const override;
  void EmptyClicked() override;
  void AddContextMenuActions(QMenu *menu) override;
  void UpdateContextMenuActions(const bool has_selection) override;

 private Q_SLOTS:
  void RemoveSelectedSongs();

 private:
  bool favorite_;
  QAction *action_remove_songs_;
};

#endif  // STREAMINGCOLLECTIONVIEW_H
