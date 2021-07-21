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
#include <QString>
#include <QStackedWidget>
#include <QContextMenuEvent>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QAction>

#include "core/application.h"
#include "core/iconloader.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionfilter.h"
#include "internetservice.h"
#include "internetsongsview.h"
#include "internetcollectionview.h"
#include "ui_internetcollectionviewcontainer.h"

InternetSongsView::InternetSongsView(Application *app, InternetService *service, const QString &settings_group, const SettingsDialog::Page settings_page, QWidget *parent)
    : QWidget(parent),
      app_(app),
      service_(service),
      settings_group_(settings_group),
      settings_page_(settings_page),
      ui_(new Ui_InternetCollectionViewContainer) {

  ui_->setupUi(this);

  ui_->stacked->setCurrentWidget(ui_->internetcollection_page);
  ui_->view->Init(app_, service_->songs_collection_backend(), service_->songs_collection_model(), false);
  ui_->view->setModel(service_->songs_collection_filter_model());
  ui_->view->SetFilter(ui_->filter_widget);
  ui_->filter_widget->SetSettingsGroup(settings_group);
  ui_->filter_widget->Init(service_->songs_collection_model(), service_->songs_collection_filter_model());

  QAction *action_configure = new QAction(IconLoader::Load("configure"), tr("Configure %1...").arg(Song::TextForSource(service_->source())), this);
  QObject::connect(action_configure, &QAction::triggered, this, &InternetSongsView::OpenSettingsDialog);
  ui_->filter_widget->AddMenuAction(action_configure);

  QObject::connect(ui_->view, &InternetCollectionView::GetSongs, this, &InternetSongsView::GetSongs);
  QObject::connect(ui_->view, &InternetCollectionView::RemoveSongs, service_, &InternetService::RemoveSongs);

  QObject::connect(ui_->refresh, &QPushButton::clicked, this, &InternetSongsView::GetSongs);
  QObject::connect(ui_->close, &QPushButton::clicked, this, &InternetSongsView::AbortGetSongs);
  QObject::connect(ui_->abort, &QPushButton::clicked, this, &InternetSongsView::AbortGetSongs);
  QObject::connect(service_, &InternetService::SongsResults, this, &InternetSongsView::SongsFinished);
  QObject::connect(service_, &InternetService::SongsUpdateStatus, ui_->status, &QLabel::setText);
  QObject::connect(service_, &InternetService::SongsProgressSetMaximum, ui_->progressbar, &QProgressBar::setMaximum);
  QObject::connect(service_, &InternetService::SongsUpdateProgress, ui_->progressbar, &QProgressBar::setValue);

  QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalArtistCountUpdated, ui_->view, &InternetCollectionView::TotalArtistCountUpdated);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalAlbumCountUpdated, ui_->view, &InternetCollectionView::TotalAlbumCountUpdated);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalSongCountUpdated, ui_->view, &InternetCollectionView::TotalSongCountUpdated);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::modelAboutToBeReset, ui_->view, &InternetCollectionView::SaveFocus);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::modelReset, ui_->view, &InternetCollectionView::RestoreFocus);

  ReloadSettings();

}

InternetSongsView::~InternetSongsView() { delete ui_; }

void InternetSongsView::ReloadSettings() {

  ui_->filter_widget->ReloadSettings();
  ui_->view->ReloadSettings();

}

void InternetSongsView::OpenSettingsDialog() {
  app_->OpenSettingsDialogAtPage(service_->settings_page());
}


void InternetSongsView::GetSongs() {

  if (!service_->authenticated() && service_->oauth()) {
    service_->ShowConfig();
    return;
  }

  ui_->status->clear();
  ui_->progressbar->show();
  ui_->abort->show();
  ui_->close->hide();
  ui_->stacked->setCurrentWidget(ui_->help_page);
  service_->GetSongs();

}

void InternetSongsView::AbortGetSongs() {

  service_->ResetSongsRequest();
  ui_->progressbar->setValue(0);
  ui_->status->clear();
  ui_->stacked->setCurrentWidget(ui_->internetcollection_page);

}

void InternetSongsView::SongsFinished(const SongList &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->status->setText(error);
    ui_->progressbar->setValue(0);
    ui_->progressbar->hide();
    ui_->abort->hide();
    ui_->close->show();
  }
  else {
    service_->songs_collection_backend()->DeleteAll();
    ui_->stacked->setCurrentWidget(ui_->internetcollection_page);
    ui_->status->clear();
    service_->songs_collection_backend()->AddOrUpdateSongsAsync(songs);
  }

}
