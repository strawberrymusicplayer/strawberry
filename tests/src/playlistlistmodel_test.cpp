/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <memory>

#include "gtest_include.h"

#include <QMap>
#include <QString>
#include <QMimeData>
#include <QModelIndex>
#include <QStandardItem>
#include <QSignalSpy>

#include "playlist/playlistlistmodel.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// The last path sent for each playlist, this is what is saved in the database.
QMap<int, QString> PlaylistPaths(const QSignalSpy &spy) {

  QMap<int, QString> playlist_paths;
  for (const QList<QVariant> &arguments : spy) {
    playlist_paths.insert(arguments.value(0).toInt(), arguments.value(1).toString());
  }
  return playlist_paths;

}

// Moves the item to the root, like dragging it in the Playlists panel.
bool MoveToRoot(PlaylistListModel *model, QStandardItem *item) {

  std::unique_ptr<QMimeData> mime_data(model->mimeData(QModelIndexList() << item->index()));
  if (!mime_data) return false;
  return model->dropMimeData(mime_data.get(), Qt::MoveAction, model->rowCount(), 0, QModelIndex());

}

// Regression test: the playlists in a dragged folder were all saved with the path of the folder, also the playlists in its subfolders, so after a restart they were in the wrong folder.
TEST(PlaylistListModelTest, DroppingFolderSendsPathsOfNestedPlaylists) {

  PlaylistListModel model;

  QStandardItem *folder_b = model.FolderByPath(u"A/B"_s);
  QStandardItem *folder_c = model.FolderByPath(u"A/B/C"_s);
  folder_b->appendRow(model.NewPlaylist(u"Playlist 1"_s, 1));
  folder_c->appendRow(model.NewPlaylist(u"Playlist 2"_s, 2));
  folder_c->appendRow(model.NewPlaylist(u"Playlist 3"_s, 3));
  ASSERT_EQ(u"A/B/C"_s, PlaylistListModel::ItemPath(model.PlaylistById(2)));

  QSignalSpy spy(&model, &PlaylistListModel::PlaylistPathChanged);
  ASSERT_TRUE(MoveToRoot(&model, folder_b));

  const QMap<int, QString> playlist_paths = PlaylistPaths(spy);
  EXPECT_EQ(u"B"_s, playlist_paths.value(1));
  EXPECT_EQ(u"B/C"_s, playlist_paths.value(2));
  EXPECT_EQ(u"B/C"_s, playlist_paths.value(3));

  // The paths match where the playlists are in the model.
  for (const int id : {1, 2, 3}) {
    ASSERT_NE(nullptr, model.PlaylistById(id));
    EXPECT_EQ(playlist_paths.value(id), PlaylistListModel::ItemPath(model.PlaylistById(id)));
  }

}

// Dragging a single playlist out of a folder sends its new path.
TEST(PlaylistListModelTest, DroppingPlaylistSendsPath) {

  PlaylistListModel model;

  QStandardItem *folder_a = model.FolderByPath(u"A"_s);
  QStandardItem *playlist_item = model.NewPlaylist(u"Playlist 1"_s, 1);
  folder_a->appendRow(playlist_item);

  QSignalSpy spy(&model, &PlaylistListModel::PlaylistPathChanged);
  ASSERT_TRUE(MoveToRoot(&model, playlist_item));

  const QMap<int, QString> playlist_paths = PlaylistPaths(spy);
  ASSERT_TRUE(playlist_paths.contains(1));
  EXPECT_EQ(QString(), playlist_paths.value(1));

}

}  // namespace
