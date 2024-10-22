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

#ifndef SMARTPLAYLISTSEARCHPREVIEW_H
#define SMARTPLAYLISTSEARCHPREVIEW_H

#include "config.h"

#include <QWidget>
#include <QList>

#include "includes/shared_ptr.h"

#include "smartplaylistsearch.h"
#include "playlistgenerator_fwd.h"

class QShowEvent;

class Player;
class PlaylistManager;
class CollectionBackend;
class CurrentAlbumCoverLoader;
class Playlist;
class Ui_SmartPlaylistSearchPreview;

#ifdef HAVE_MOODBAR
class MoodbarLoader;
#endif

class SmartPlaylistSearchPreview : public QWidget {
  Q_OBJECT

 public:
  explicit SmartPlaylistSearchPreview(QWidget *parent = nullptr);
  ~SmartPlaylistSearchPreview() override;

  void Init(const SharedPtr<Player> player,
            const SharedPtr<PlaylistManager> playlist_manager,
            const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
            const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
            const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader);

  void Update(const SmartPlaylistSearch &search);

 protected:
  void showEvent(QShowEvent*) override;

 private:
  void RunSearch(const SmartPlaylistSearch &search);

 private Q_SLOTS:
  void SearchFinished();

 private:
  Ui_SmartPlaylistSearchPreview *ui_;
  QList<SmartPlaylistSearchTerm::Field> fields_;

  SharedPtr<CollectionBackend> collection_backend_;
  Playlist *model_;

  SmartPlaylistSearch pending_search_;
  SmartPlaylistSearch last_search_;
  PlaylistGeneratorPtr generator_;
};

#endif  // SMARTPLAYLISTSEARCHPREVIEW_H
