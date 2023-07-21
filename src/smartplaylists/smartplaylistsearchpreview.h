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

#include "core/shared_ptr.h"

#include "smartplaylistsearch.h"
#include "playlistgenerator_fwd.h"

class QShowEvent;

class Application;
class CollectionBackend;
class Playlist;
class Ui_SmartPlaylistSearchPreview;

class SmartPlaylistSearchPreview : public QWidget {
  Q_OBJECT

 public:
  explicit SmartPlaylistSearchPreview(QWidget *parent = nullptr);
  ~SmartPlaylistSearchPreview() override;

  void set_application(Application *app);
  void set_collection(SharedPtr<CollectionBackend> backend);

  void Update(const SmartPlaylistSearch &search);

 protected:
  void showEvent(QShowEvent*) override;

 private:
  void RunSearch(const SmartPlaylistSearch &search);

 private slots:
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
