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

#include "config.h"

#include <QtGlobal>
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QStackedWidget>
#include <QContextMenuEvent>
#include <QAction>
#include <QSettings>

#include "core/application.h"
#include "core/iconloader.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"
#include "collection/collectionfilterwidget.h"
#include "internetservice.h"
#include "internettabsview.h"
#include "internetcollectionview.h"
#include "internetcollectionviewcontainer.h"
#include "ui_internettabsview.h"

InternetTabsView::InternetTabsView(Application *app, InternetService *service, const QString &settings_group, const SettingsDialog::Page settings_page, QWidget *parent)
    : QWidget(parent),
      app_(app),
      service_(service),
      settings_group_(settings_group),
      settings_page_(settings_page),
      ui_(new Ui_InternetTabsView) {

  ui_->setupUi(this);

  ui_->search_view->Init(app, service);
  QObject::connect(ui_->search_view, &InternetSearchView::AddArtistsSignal, service_, &InternetService::AddArtists);
  QObject::connect(ui_->search_view, &InternetSearchView::AddAlbumsSignal, service_, &InternetService::AddAlbums);
  QObject::connect(ui_->search_view, &InternetSearchView::AddSongsSignal, service_, &InternetService::AddSongs);

  QAction *action_configure = new QAction(IconLoader::Load("configure"), tr("Configure %1...").arg(Song::TextForSource(service_->source())), this);
  QObject::connect(action_configure, &QAction::triggered, this, &InternetTabsView::OpenSettingsDialog);

  if (service_->artists_collection_model()) {
    ui_->artists_collection->stacked()->setCurrentWidget(ui_->artists_collection->internetcollection_page());
    ui_->artists_collection->view()->Init(app_, service_->artists_collection_backend(), service_->artists_collection_model(), true);
    ui_->artists_collection->view()->setModel(service_->artists_collection_filter_model());
    ui_->artists_collection->view()->SetFilter(ui_->artists_collection->filter_widget());
    ui_->artists_collection->filter_widget()->SetSettingsGroup(settings_group);
    ui_->artists_collection->filter_widget()->SetSettingsPrefix("artists");
    ui_->artists_collection->filter_widget()->Init(service_->artists_collection_model(), service_->artists_collection_filter_model());
    ui_->artists_collection->filter_widget()->AddMenuAction(action_configure);

    QObject::connect(ui_->artists_collection->view(), &InternetCollectionView::GetSongs, this, &InternetTabsView::GetArtists);
    QObject::connect(ui_->artists_collection->view(), &InternetCollectionView::RemoveSongs, service_, &InternetService::RemoveArtists);

    QObject::connect(ui_->artists_collection->button_refresh(), &QPushButton::clicked, this, &InternetTabsView::GetArtists);
    QObject::connect(ui_->artists_collection->button_close(), &QPushButton::clicked, this, &InternetTabsView::AbortGetArtists);
    QObject::connect(ui_->artists_collection->button_abort(), &QPushButton::clicked, this, &InternetTabsView::AbortGetArtists);
    QObject::connect(service_, &InternetService::ArtistsResults, this, &InternetTabsView::ArtistsFinished);
    QObject::connect(service_, &InternetService::ArtistsUpdateStatus, ui_->artists_collection->status(), &QLabel::setText);
    QObject::connect(service_, &InternetService::ArtistsProgressSetMaximum, ui_->artists_collection->progressbar(), &QProgressBar::setMaximum);
    QObject::connect(service_, &InternetService::ArtistsUpdateProgress, ui_->artists_collection->progressbar(), &QProgressBar::setValue);

    QObject::connect(service_->artists_collection_model(), &CollectionModel::TotalArtistCountUpdated, ui_->artists_collection->view(), &InternetCollectionView::TotalArtistCountUpdated);
    QObject::connect(service_->artists_collection_model(), &CollectionModel::TotalAlbumCountUpdated, ui_->artists_collection->view(), &InternetCollectionView::TotalAlbumCountUpdated);
    QObject::connect(service_->artists_collection_model(), &CollectionModel::TotalSongCountUpdated, ui_->artists_collection->view(), &InternetCollectionView::TotalSongCountUpdated);
    QObject::connect(service_->artists_collection_model(), &CollectionModel::modelAboutToBeReset, ui_->artists_collection->view(), &InternetCollectionView::SaveFocus);
    QObject::connect(service_->artists_collection_model(), &CollectionModel::modelReset, ui_->artists_collection->view(), &InternetCollectionView::RestoreFocus);

  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->artists));
  }

  if (service_->albums_collection_model()) {
    ui_->albums_collection->stacked()->setCurrentWidget(ui_->albums_collection->internetcollection_page());
    ui_->albums_collection->view()->Init(app_, service_->albums_collection_backend(), service_->albums_collection_model(), true);
    ui_->albums_collection->view()->setModel(service_->albums_collection_filter_model());
    ui_->albums_collection->view()->SetFilter(ui_->albums_collection->filter_widget());
    ui_->albums_collection->filter_widget()->SetSettingsGroup(settings_group);
    ui_->albums_collection->filter_widget()->SetSettingsPrefix("albums");
    ui_->albums_collection->filter_widget()->Init(service_->albums_collection_model(), service_->albums_collection_filter_model());
    ui_->albums_collection->filter_widget()->AddMenuAction(action_configure);

    QObject::connect(ui_->albums_collection->view(), &InternetCollectionView::GetSongs, this, &InternetTabsView::GetAlbums);
    QObject::connect(ui_->albums_collection->view(), &InternetCollectionView::RemoveSongs, service_, &InternetService::RemoveAlbums);

    QObject::connect(ui_->albums_collection->button_refresh(), &QPushButton::clicked, this, &InternetTabsView::GetAlbums);
    QObject::connect(ui_->albums_collection->button_close(), &QPushButton::clicked, this, &InternetTabsView::AbortGetAlbums);
    QObject::connect(ui_->albums_collection->button_abort(), &QPushButton::clicked, this, &InternetTabsView::AbortGetAlbums);
    QObject::connect(service_, &InternetService::AlbumsResults, this, &InternetTabsView::AlbumsFinished);
    QObject::connect(service_, &InternetService::AlbumsUpdateStatus, ui_->albums_collection->status(), &QLabel::setText);
    QObject::connect(service_, &InternetService::AlbumsProgressSetMaximum, ui_->albums_collection->progressbar(), &QProgressBar::setMaximum);
    QObject::connect(service_, &InternetService::AlbumsUpdateProgress, ui_->albums_collection->progressbar(), &QProgressBar::setValue);

    QObject::connect(service_->albums_collection_model(), &CollectionModel::TotalArtistCountUpdated, ui_->albums_collection->view(), &InternetCollectionView::TotalArtistCountUpdated);
    QObject::connect(service_->albums_collection_model(), &CollectionModel::TotalAlbumCountUpdated, ui_->albums_collection->view(), &InternetCollectionView::TotalAlbumCountUpdated);
    QObject::connect(service_->albums_collection_model(), &CollectionModel::TotalSongCountUpdated, ui_->albums_collection->view(), &InternetCollectionView::TotalSongCountUpdated);
    QObject::connect(service_->albums_collection_model(), &CollectionModel::modelAboutToBeReset, ui_->albums_collection->view(), &InternetCollectionView::SaveFocus);
    QObject::connect(service_->albums_collection_model(), &CollectionModel::modelReset, ui_->albums_collection->view(), &InternetCollectionView::RestoreFocus);

  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->albums));
  }

  if (service_->songs_collection_model()) {
    ui_->songs_collection->stacked()->setCurrentWidget(ui_->songs_collection->internetcollection_page());
    ui_->songs_collection->view()->Init(app_, service_->songs_collection_backend(), service_->songs_collection_model(), true);
    ui_->songs_collection->view()->setModel(service_->songs_collection_filter_model());
    ui_->songs_collection->view()->SetFilter(ui_->songs_collection->filter_widget());
    ui_->songs_collection->filter_widget()->SetSettingsGroup(settings_group);
    ui_->songs_collection->filter_widget()->SetSettingsPrefix("songs");
    ui_->songs_collection->filter_widget()->Init(service_->songs_collection_model(), service_->songs_collection_filter_model());
    ui_->songs_collection->filter_widget()->AddMenuAction(action_configure);

    QObject::connect(ui_->songs_collection->view(), &InternetCollectionView::GetSongs, this, &InternetTabsView::GetSongs);
    QObject::connect(ui_->songs_collection->view(), &InternetCollectionView::RemoveSongs, service_, &InternetService::RemoveSongs);

    QObject::connect(ui_->songs_collection->button_refresh(), &QPushButton::clicked, this, &InternetTabsView::GetSongs);
    QObject::connect(ui_->songs_collection->button_close(), &QPushButton::clicked, this, &InternetTabsView::AbortGetSongs);
    QObject::connect(ui_->songs_collection->button_abort(), &QPushButton::clicked, this, &InternetTabsView::AbortGetSongs);
    QObject::connect(service_, &InternetService::SongsResults, this, &InternetTabsView::SongsFinished);
    QObject::connect(service_, &InternetService::SongsUpdateStatus, ui_->songs_collection->status(), &QLabel::setText);
    QObject::connect(service_, &InternetService::SongsProgressSetMaximum, ui_->songs_collection->progressbar(), &QProgressBar::setMaximum);
    QObject::connect(service_, &InternetService::SongsUpdateProgress, ui_->songs_collection->progressbar(), &QProgressBar::setValue);

    QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalArtistCountUpdated, ui_->songs_collection->view(), &InternetCollectionView::TotalArtistCountUpdated);
    QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalAlbumCountUpdated, ui_->songs_collection->view(), &InternetCollectionView::TotalAlbumCountUpdated);
    QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalSongCountUpdated, ui_->songs_collection->view(), &InternetCollectionView::TotalSongCountUpdated);
    QObject::connect(service_->songs_collection_model(), &CollectionModel::modelAboutToBeReset, ui_->songs_collection->view(), &InternetCollectionView::SaveFocus);
    QObject::connect(service_->songs_collection_model(), &CollectionModel::modelReset, ui_->songs_collection->view(), &InternetCollectionView::RestoreFocus);

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

void InternetTabsView::ReloadSettings() {

  if (service_->artists_collection_model()) {
    ui_->artists_collection->view()->ReloadSettings();
  }
  if (service_->albums_collection_model()) {
    ui_->albums_collection->view()->ReloadSettings();
  }
  if (service_->songs_collection_model()) {
    ui_->songs_collection->view()->ReloadSettings();
  }
  ui_->search_view->ReloadSettings();

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

void InternetTabsView::ArtistsFinished(const SongList &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->artists_collection->status()->setText(error);
    ui_->artists_collection->progressbar()->setValue(0);
    ui_->artists_collection->progressbar()->hide();
    ui_->artists_collection->button_abort()->hide();
    ui_->artists_collection->button_close()->show();
  }
  else {
    service_->artists_collection_backend()->DeleteAll();
    ui_->artists_collection->stacked()->setCurrentWidget(ui_->artists_collection->internetcollection_page());
    ui_->artists_collection->status()->clear();
    service_->artists_collection_backend()->AddOrUpdateSongsAsync(songs);
  }

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

void InternetTabsView::AlbumsFinished(const SongList &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->albums_collection->status()->setText(error);
    ui_->albums_collection->progressbar()->setValue(0);
    ui_->albums_collection->progressbar()->hide();
    ui_->albums_collection->button_abort()->hide();
    ui_->albums_collection->button_close()->show();
  }
  else {
    service_->albums_collection_backend()->DeleteAll();
    ui_->albums_collection->stacked()->setCurrentWidget(ui_->albums_collection->internetcollection_page());
    ui_->albums_collection->status()->clear();
    service_->albums_collection_backend()->AddOrUpdateSongsAsync(songs);
  }

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

void InternetTabsView::SongsFinished(const SongList &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->songs_collection->status()->setText(error);
    ui_->songs_collection->progressbar()->setValue(0);
    ui_->songs_collection->progressbar()->hide();
    ui_->songs_collection->button_abort()->hide();
    ui_->songs_collection->button_close()->show();
  }
  else {
    service_->songs_collection_backend()->DeleteAll();
    ui_->songs_collection->stacked()->setCurrentWidget(ui_->songs_collection->internetcollection_page());
    ui_->songs_collection->status()->clear();
    service_->songs_collection_backend()->AddOrUpdateSongsAsync(songs);
  }

}

void InternetTabsView::OpenSettingsDialog() {
  app_->OpenSettingsDialogAtPage(service_->settings_page());
}
