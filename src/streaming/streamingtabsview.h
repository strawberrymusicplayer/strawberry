/*
 * Strawberry Music Player
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

#ifndef STREAMINGTABSVIEW_H
#define STREAMINGTABSVIEW_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QString>

#include "includes/shared_ptr.h"
#include "streamingcollectionviewcontainer.h"
#include "core/song.h"

#include "ui_streamingtabsview.h"

class QContextMenuEvent;

class StreamingService;
class StreamingCollectionView;
class StreamingSearchView;
class AlbumCoverLoader;

class StreamingTabsView : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingTabsView(const SharedPtr<StreamingService> service, const SharedPtr<AlbumCoverLoader> albumcover_loader, const QString &settings_group, QWidget *parent = nullptr);
  ~StreamingTabsView() override;

  void ReloadSettings();

  StreamingCollectionView *artists_collection_view() const { return ui_->artists_collection->view(); }
  StreamingCollectionView *albums_collection_view() const { return ui_->albums_collection->view(); }
  StreamingCollectionView *songs_collection_view() const { return ui_->songs_collection->view(); }
  StreamingSearchView *search_view() const { return ui_->search_view; }

  bool SearchFieldHasFocus() const;
  void FocusSearchField();

 private Q_SLOTS:
  void Configure();
  void GetArtists();
  void GetAlbums();
  void GetSongs();
  void AbortGetArtists();
  void AbortGetAlbums();
  void AbortGetSongs();
  void ArtistsFinished(const SongMap &songs, const QString &error);
  void AlbumsFinished(const SongMap &songs, const QString &error);
  void SongsFinished(const SongMap &songs, const QString &error);

 Q_SIGNALS:
  void OpenSettingsDialog(const Song::Source source);

 private:
  const SharedPtr <StreamingService> service_;
  QString settings_group_;
  Ui_StreamingTabsView *ui_;
};

#endif  // STREAMINGTABSVIEW_H
