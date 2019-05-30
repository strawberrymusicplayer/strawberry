/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef INTERNETTABSVIEW_H
#define INTERNETTABSVIEW_H

#include "config.h"

#include <QWidget>
#include <QString>

#include "settings/settingsdialog.h"
#include "internetcollectionviewcontainer.h"
#include "internetcollectionview.h"
#include "ui_internettabsview.h"
#include "core/song.h"

class QContextMenuEvent;

class Application;
class InternetService;
class InternetSearch;
class Ui_InternetTabsView;
class InternetCollectionView;
class InternetSearchView;

class InternetTabsView : public QWidget {
  Q_OBJECT

 public:
  InternetTabsView(Application *app, InternetService *service, InternetSearch *engine, QString settings_group, SettingsDialog::Page settings_page, QWidget *parent = nullptr);
  ~InternetTabsView();

  void ReloadSettings();

  InternetCollectionView *artists_collection_view() const { return ui_->artists_collection->view(); }
  InternetCollectionView *albums_collection_view() const { return ui_->albums_collection->view(); }
  InternetCollectionView *songs_collection_view() const { return ui_->songs_collection->view(); }
  InternetSearchView *search_view() const { return ui_->search_view; }

 private slots:
  void contextMenuEvent(QContextMenuEvent *e);
  void GetArtists();
  void GetAlbums();
  void GetSongs();
  void AbortGetArtists();
  void AbortGetAlbums();
  void AbortGetSongs();
  void ArtistsError(QString error);
  void AlbumsError(QString error);
  void SongsError(QString error);
  void ArtistsFinished(SongList songs);
  void AlbumsFinished(SongList songs);
  void SongsFinished(SongList songs);

 private:
  Application *app_;
  InternetService *service_;
  InternetSearch *engine_;
  QString settings_group_;
  SettingsDialog::Page settings_page_;
  Ui_InternetTabsView *ui_;

};

#endif  // INTERNETTABSVIEW_H
