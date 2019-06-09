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

#include "config.h"

#include <QtGlobal>
#include <QWidget>
#include <QString>
#include <QStackedWidget>
#include <QContextMenuEvent>
#include <QSettings>

#include "core/application.h"
#include "collection/collectionbackend.h"
#include "collection/collectionfilterwidget.h"
#include "internetservice.h"
#include "internettabsview.h"
#include "ui_internettabsview.h"

InternetTabsView::InternetTabsView(Application *app, InternetService *service, InternetSearch *engine, QString settings_group, SettingsDialog::Page settings_page, QWidget *parent)
    : QWidget(parent),
      app_(app),
      service_(service),
      engine_(engine),
      settings_group_(settings_group),
      settings_page_(settings_page),
      ui_(new Ui_InternetTabsView)
      {

  ui_->setupUi(this);

  ui_->search_view->Init(app, engine, settings_group, settings_page, service_->artists_collection_model(), service_->albums_collection_model(), service_->songs_collection_model());
  connect(ui_->search_view, SIGNAL(AddArtistsSignal(const SongList&)), service_, SIGNAL(AddArtists(const SongList&)));
  connect(ui_->search_view, SIGNAL(AddAlbumsSignal(const SongList&)), service_, SIGNAL(AddAlbums(const SongList&)));
  connect(ui_->search_view, SIGNAL(AddSongsSignal(const SongList&)), service_, SIGNAL(AddSongs(const SongList&)));

  if (service_->artists_collection_model()) {
    ui_->artists_collection->stacked()->setCurrentWidget(ui_->artists_collection->internetcollection_page());
    ui_->artists_collection->view()->Init(app_, service_->artists_collection_backend(), service_->artists_collection_model());
    ui_->artists_collection->view()->setModel(service_->artists_collection_sort_model());
    ui_->artists_collection->view()->SetFilter(ui_->artists_collection->filter());
    ui_->artists_collection->filter()->SetSettingsGroup(settings_group);
    ui_->artists_collection->filter()->SetSettingsPrefix("artists");
    ui_->artists_collection->filter()->SetCollectionModel(service_->artists_collection_model());

    connect(ui_->artists_collection->view(), SIGNAL(GetSongs()), SLOT(GetArtists()));
    connect(ui_->artists_collection->view(), SIGNAL(RemoveSongs(const SongList&)), service_, SIGNAL(RemoveArtists(const SongList&)));

    connect(ui_->artists_collection->button_refresh(), SIGNAL(clicked()), SLOT(GetArtists()));
    connect(ui_->artists_collection->button_close(), SIGNAL(clicked()), SLOT(AbortGetArtists()));
    connect(ui_->artists_collection->button_abort(), SIGNAL(clicked()), SLOT(AbortGetArtists()));
    connect(service_, SIGNAL(ArtistsResults(SongList)), SLOT(ArtistsFinished(SongList)));
    connect(service_, SIGNAL(ArtistsError(QString)), SLOT(ArtistsError(QString)));
    connect(service_, SIGNAL(ArtistsUpdateStatus(QString)), ui_->artists_collection->status(), SLOT(setText(QString)));
    connect(service_, SIGNAL(ArtistsProgressSetMaximum(int)), ui_->artists_collection->progressbar(), SLOT(setMaximum(int)));
    connect(service_, SIGNAL(ArtistsUpdateProgress(int)), ui_->artists_collection->progressbar(), SLOT(setValue(int)));

    connect(service_->artists_collection_model(), SIGNAL(TotalArtistCountUpdated(int)), ui_->artists_collection->view(), SLOT(TotalArtistCountUpdated(int)));
    connect(service_->artists_collection_model(), SIGNAL(TotalAlbumCountUpdated(int)), ui_->artists_collection->view(), SLOT(TotalAlbumCountUpdated(int)));
    connect(service_->artists_collection_model(), SIGNAL(TotalSongCountUpdated(int)), ui_->artists_collection->view(), SLOT(TotalSongCountUpdated(int)));
    connect(service_->artists_collection_model(), SIGNAL(modelAboutToBeReset()), ui_->artists_collection->view(), SLOT(SaveFocus()));
    connect(service_->artists_collection_model(), SIGNAL(modelReset()), ui_->artists_collection->view(), SLOT(RestoreFocus()));

  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->artists));
  }

  if (service_->albums_collection_model()) {
    ui_->albums_collection->stacked()->setCurrentWidget(ui_->albums_collection->internetcollection_page());
    ui_->albums_collection->view()->Init(app_, service_->albums_collection_backend(), service_->albums_collection_model());
    ui_->albums_collection->view()->setModel(service_->albums_collection_sort_model());
    ui_->albums_collection->view()->SetFilter(ui_->albums_collection->filter());
    ui_->albums_collection->filter()->SetSettingsGroup(settings_group);
    ui_->albums_collection->filter()->SetSettingsPrefix("albums");
    ui_->albums_collection->filter()->SetCollectionModel(service_->albums_collection_model());

    connect(ui_->albums_collection->view(), SIGNAL(GetSongs()), SLOT(GetAlbums()));
    connect(ui_->albums_collection->view(), SIGNAL(RemoveSongs(const SongList&)), service_, SIGNAL(RemoveAlbums(const SongList&)));

    connect(ui_->albums_collection->button_refresh(), SIGNAL(clicked()), SLOT(GetAlbums()));
    connect(ui_->albums_collection->button_close(), SIGNAL(clicked()), SLOT(AbortGetAlbums()));
    connect(ui_->albums_collection->button_abort(), SIGNAL(clicked()), SLOT(AbortGetAlbums()));
    connect(service_, SIGNAL(AlbumsResults(SongList)), SLOT(AlbumsFinished(SongList)));
    connect(service_, SIGNAL(AlbumsError(QString)), SLOT(AlbumsError(QString)));
    connect(service_, SIGNAL(AlbumsUpdateStatus(QString)), ui_->albums_collection->status(), SLOT(setText(QString)));
    connect(service_, SIGNAL(AlbumsProgressSetMaximum(int)), ui_->albums_collection->progressbar(), SLOT(setMaximum(int)));
    connect(service_, SIGNAL(AlbumsUpdateProgress(int)), ui_->albums_collection->progressbar(), SLOT(setValue(int)));

    connect(service_->albums_collection_model(), SIGNAL(TotalArtistCountUpdated(int)), ui_->albums_collection->view(), SLOT(TotalArtistCountUpdated(int)));
    connect(service_->albums_collection_model(), SIGNAL(TotalAlbumCountUpdated(int)), ui_->albums_collection->view(), SLOT(TotalAlbumCountUpdated(int)));
    connect(service_->albums_collection_model(), SIGNAL(TotalSongCountUpdated(int)), ui_->albums_collection->view(), SLOT(TotalSongCountUpdated(int)));
    connect(service_->albums_collection_model(), SIGNAL(modelAboutToBeReset()), ui_->albums_collection->view(), SLOT(SaveFocus()));
    connect(service_->albums_collection_model(), SIGNAL(modelReset()), ui_->albums_collection->view(), SLOT(RestoreFocus()));

  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->albums));
  }

  if (service_->songs_collection_model()) {
    ui_->songs_collection->stacked()->setCurrentWidget(ui_->songs_collection->internetcollection_page());
    ui_->songs_collection->view()->Init(app_, service_->songs_collection_backend(), service_->songs_collection_model());
    ui_->songs_collection->view()->setModel(service_->songs_collection_sort_model());
    ui_->songs_collection->view()->SetFilter(ui_->songs_collection->filter());
    ui_->songs_collection->filter()->SetSettingsGroup(settings_group);
    ui_->songs_collection->filter()->SetSettingsPrefix("songs");
    ui_->songs_collection->filter()->SetCollectionModel(service_->songs_collection_model());

    connect(ui_->songs_collection->view(), SIGNAL(GetSongs()), SLOT(GetSongs()));
    connect(ui_->songs_collection->view(), SIGNAL(RemoveSongs(const SongList&)), service_, SIGNAL(RemoveSongs(const SongList&)));

    connect(ui_->songs_collection->button_refresh(), SIGNAL(clicked()), SLOT(GetSongs()));
    connect(ui_->songs_collection->button_close(), SIGNAL(clicked()), SLOT(AbortGetSongs()));
    connect(ui_->songs_collection->button_abort(), SIGNAL(clicked()), SLOT(AbortGetSongs()));
    connect(service_, SIGNAL(SongsResults(SongList)), SLOT(SongsFinished(SongList)));
    connect(service_, SIGNAL(SongsError(QString)), SLOT(SongsError(QString)));
    connect(service_, SIGNAL(SongsUpdateStatus(QString)), ui_->songs_collection->status(), SLOT(setText(QString)));
    connect(service_, SIGNAL(SongsProgressSetMaximum(int)), ui_->songs_collection->progressbar(), SLOT(setMaximum(int)));
    connect(service_, SIGNAL(SongsUpdateProgress(int)), ui_->songs_collection->progressbar(), SLOT(setValue(int)));

    connect(service_->songs_collection_model(), SIGNAL(TotalArtistCountUpdated(int)), ui_->songs_collection->view(), SLOT(TotalArtistCountUpdated(int)));
    connect(service_->songs_collection_model(), SIGNAL(TotalAlbumCountUpdated(int)), ui_->songs_collection->view(), SLOT(TotalAlbumCountUpdated(int)));
    connect(service_->songs_collection_model(), SIGNAL(TotalSongCountUpdated(int)), ui_->songs_collection->view(), SLOT(TotalSongCountUpdated(int)));
    connect(service_->songs_collection_model(), SIGNAL(modelAboutToBeReset()), ui_->songs_collection->view(), SLOT(SaveFocus()));
    connect(service_->songs_collection_model(), SIGNAL(modelReset()), ui_->songs_collection->view(), SLOT(RestoreFocus()));

  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->songs));
  }

  QSettings s;
  s.beginGroup(settings_group_);
  QString tab = s.value("tab", "artists").toString().toLower();
  s.endGroup();

  if (tab == "artists") {
    ui_->tabs->setCurrentWidget(ui_->artists);
  }
  else if (tab == "albums") {
    ui_->tabs->setCurrentWidget(ui_->albums);
  }
  else if (tab == "songs") {
    ui_->tabs->setCurrentWidget(ui_->songs);
  }
  else if (tab == "search") {
    ui_->tabs->setCurrentWidget(ui_->search);
  }

  ReloadSettings();

}

InternetTabsView::~InternetTabsView() {

  QSettings s;
  s.beginGroup(settings_group_);
  s.setValue("tab", ui_->tabs->currentWidget()->objectName().toLower());
  s.endGroup();

  delete ui_;
}

void InternetTabsView::ReloadSettings() { ui_->search_view->ReloadSettings(); }

void InternetTabsView::contextMenuEvent(QContextMenuEvent *e) {
}

void InternetTabsView::GetArtists() {

  if (!service_->authenticated() && service_->oauth()) {
    service_->ShowConfig();
    return;
  }

  ui_->artists_collection->status()->clear();
  ui_->artists_collection->progressbar()->show();
  ui_->artists_collection->button_abort()->show();
  ui_->artists_collection->button_close()->hide();
  ui_->artists_collection->stacked()->setCurrentWidget(ui_->artists_collection->help_page());
  service_->GetArtists();

}

void InternetTabsView::AbortGetArtists() {

  service_->ResetArtistsRequest();
  ui_->artists_collection->progressbar()->setValue(0);
  ui_->artists_collection->status()->clear();
  ui_->artists_collection->stacked()->setCurrentWidget(ui_->artists_collection->internetcollection_page());

}

void InternetTabsView::ArtistsError(QString error) {

  ui_->artists_collection->status()->setText(error);
  ui_->artists_collection->progressbar()->setValue(0);
  ui_->artists_collection->progressbar()->hide();
  ui_->artists_collection->button_abort()->hide();
  ui_->artists_collection->button_close()->show();

}

void InternetTabsView::ArtistsFinished(SongList songs) {

  service_->artists_collection_backend()->DeleteAll();
  ui_->artists_collection->stacked()->setCurrentWidget(ui_->artists_collection->internetcollection_page());
  ui_->artists_collection->status()->clear();
  service_->artists_collection_backend()->AddOrUpdateSongs(songs);

}

void InternetTabsView::GetAlbums() {

  if (!service_->authenticated() && service_->oauth()) {
    service_->ShowConfig();
    return;
  }

  ui_->albums_collection->status()->clear();
  ui_->albums_collection->progressbar()->show();
  ui_->albums_collection->button_abort()->show();
  ui_->albums_collection->button_close()->hide();
  ui_->albums_collection->stacked()->setCurrentWidget(ui_->albums_collection->help_page());
  service_->GetAlbums();

}

void InternetTabsView::AbortGetAlbums() {

  service_->ResetAlbumsRequest();
  ui_->albums_collection->progressbar()->setValue(0);
  ui_->albums_collection->status()->clear();
  ui_->albums_collection->stacked()->setCurrentWidget(ui_->albums_collection->internetcollection_page());

}

void InternetTabsView::AlbumsError(QString error) {

  ui_->albums_collection->status()->setText(error);
  ui_->albums_collection->progressbar()->setValue(0);
  ui_->albums_collection->progressbar()->hide();
  ui_->albums_collection->button_abort()->hide();
  ui_->albums_collection->button_close()->show();

}

void InternetTabsView::AlbumsFinished(SongList songs) {

  service_->albums_collection_backend()->DeleteAll();
  ui_->albums_collection->stacked()->setCurrentWidget(ui_->albums_collection->internetcollection_page());
  ui_->albums_collection->status()->clear();
  service_->albums_collection_backend()->AddOrUpdateSongs(songs);

}

void InternetTabsView::GetSongs() {

  if (!service_->authenticated() && service_->oauth()) {
    service_->ShowConfig();
    return;
  }

  ui_->songs_collection->status()->clear();
  ui_->songs_collection->progressbar()->show();
  ui_->songs_collection->button_abort()->show();
  ui_->songs_collection->button_close()->hide();
  ui_->songs_collection->stacked()->setCurrentWidget(ui_->songs_collection->help_page());
  service_->GetSongs();

}

void InternetTabsView::AbortGetSongs() {

  service_->ResetSongsRequest();
  ui_->songs_collection->progressbar()->setValue(0);
  ui_->songs_collection->status()->clear();
  ui_->songs_collection->stacked()->setCurrentWidget(ui_->songs_collection->internetcollection_page());

}

void InternetTabsView::SongsError(QString error) {

  ui_->songs_collection->status()->setText(error);
  ui_->songs_collection->progressbar()->setValue(0);
  ui_->songs_collection->progressbar()->hide();
  ui_->songs_collection->button_abort()->hide();
  ui_->songs_collection->button_close()->show();

}

void InternetTabsView::SongsFinished(SongList songs) {

  service_->songs_collection_backend()->DeleteAll();
  ui_->songs_collection->stacked()->setCurrentWidget(ui_->songs_collection->internetcollection_page());
  ui_->songs_collection->status()->clear();
  service_->songs_collection_backend()->AddOrUpdateSongs(songs);

}
