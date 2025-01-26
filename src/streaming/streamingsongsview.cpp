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

#include "core/iconloader.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"
#include "streamingservice.h"
#include "streamingsongsview.h"
#include "streamingcollectionview.h"
#include "ui_streamingcollectionviewcontainer.h"

using namespace Qt::Literals::StringLiterals;

StreamingSongsView::StreamingSongsView(const StreamingServicePtr service, const QString &settings_group, QWidget *parent)
    : QWidget(parent),
      service_(service),
      settings_group_(settings_group),
      ui_(new Ui_StreamingCollectionViewContainer) {

  ui_->setupUi(this);

  ui_->stacked->setCurrentWidget(ui_->streamingcollection_page);
  ui_->view->Init(service_->songs_collection_backend(), service_->songs_collection_model(), false);
  ui_->view->setModel(service_->songs_collection_filter_model());
  ui_->view->SetFilter(ui_->filter_widget);
  ui_->filter_widget->SetSettingsGroup(settings_group);
  ui_->filter_widget->Init(service_->songs_collection_model(), service_->songs_collection_filter_model());
  ui_->refresh->setVisible(service_->enable_refresh_button());

  QAction *action_configure = new QAction(IconLoader::Load(u"configure"_s), tr("Configure %1...").arg(Song::DescriptionForSource(service_->source())), this);
  QObject::connect(action_configure, &QAction::triggered, this, &StreamingSongsView::Configure);
  ui_->filter_widget->AddMenuAction(action_configure);

  QObject::connect(ui_->view, &StreamingCollectionView::GetSongs, this, &StreamingSongsView::GetSongs);
  QObject::connect(ui_->view, &StreamingCollectionView::RemoveSongs, &*service_, &StreamingService::RemoveSongsByList);

  QObject::connect(ui_->refresh, &QPushButton::clicked, this, &StreamingSongsView::GetSongs);
  QObject::connect(ui_->close, &QPushButton::clicked, this, &StreamingSongsView::AbortGetSongs);
  QObject::connect(ui_->abort, &QPushButton::clicked, this, &StreamingSongsView::AbortGetSongs);
  QObject::connect(&*service_, &StreamingService::ShowErrorDialog, this, &StreamingSongsView::ShowErrorDialog);
  QObject::connect(&*service_, &StreamingService::SongsResults, this, &StreamingSongsView::SongsFinished);
  QObject::connect(&*service_, &StreamingService::SongsUpdateStatus, ui_->status, &QLabel::setText);
  QObject::connect(&*service_, &StreamingService::SongsProgressSetMaximum, ui_->progressbar, &QProgressBar::setMaximum);
  QObject::connect(&*service_, &StreamingService::SongsUpdateProgress, ui_->progressbar, &QProgressBar::setValue);

  QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalArtistCountUpdated, ui_->view, &StreamingCollectionView::TotalArtistCountUpdated);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalAlbumCountUpdated, ui_->view, &StreamingCollectionView::TotalAlbumCountUpdated);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::TotalSongCountUpdated, ui_->view, &StreamingCollectionView::TotalSongCountUpdated);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::modelAboutToBeReset, ui_->view, &StreamingCollectionView::SaveFocus);
  QObject::connect(service_->songs_collection_model(), &CollectionModel::modelReset, ui_->view, &StreamingCollectionView::RestoreFocus);

  ReloadSettings();

}

StreamingSongsView::~StreamingSongsView() { delete ui_; }

void StreamingSongsView::ReloadSettings() {

  ui_->filter_widget->ReloadSettings();
  ui_->view->ReloadSettings();

}

void StreamingSongsView::Configure() {
  Q_EMIT OpenSettingsDialog(service_->source());
}

void StreamingSongsView::GetSongs() {

  if (!service_->authenticated() && service_->oauth()) {
    Configure();
    return;
  }

  if (service_->show_progress()) {
    ui_->status->clear();
    ui_->progressbar->show();
    ui_->abort->show();
    ui_->close->hide();
    ui_->stacked->setCurrentWidget(ui_->help_page);
  }

  service_->GetSongs();

}

void StreamingSongsView::AbortGetSongs() {

  service_->ResetSongsRequest();

  if (service_->show_progress()) {
    ui_->progressbar->setValue(0);
    ui_->status->clear();
    ui_->stacked->setCurrentWidget(ui_->streamingcollection_page);
  }

}

void StreamingSongsView::SongsFinished(const SongMap &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->status->setText(error);
    ui_->progressbar->setValue(0);
    ui_->progressbar->hide();
    ui_->abort->hide();
    ui_->close->show();
  }
  else {
    ui_->stacked->setCurrentWidget(ui_->streamingcollection_page);
    ui_->status->clear();
    service_->songs_collection_backend()->UpdateSongsBySongIDAsync(songs);
  }

}
