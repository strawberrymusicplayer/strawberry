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

#include "config.h"

#include <QtGlobal>
#include <QWidget>
#include <QString>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QAction>

#include "core/iconloader.h"
#include "collection/collectionbackend.h"
#include "collection/collectionfilterwidget.h"
#include "streamingservice.h"
#include "streamingsongsview.h"
#include "streamingcollectionview.h"
#include "streamingcollectionviewcontainer.h"

using namespace Qt::Literals::StringLiterals;

StreamingSongsView::StreamingSongsView(const StreamingServicePtr service, const QString &settings_group, QWidget *parent)
    : QWidget(parent),
      service_(service),
      container_(new StreamingCollectionViewContainer(this)) {

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(container_);

  container_->Init(service_->songs_collection_model(), service_->songs_collection_filter_model(), settings_group, QString(), false);
  container_->button_refresh()->setVisible(service_->enable_refresh_button());

  QAction *action_configure = new QAction(IconLoader::Load(u"configure"_s), tr("Configure %1...").arg(Song::DescriptionForSource(service_->source())), this);
  QObject::connect(action_configure, &QAction::triggered, this, &StreamingSongsView::Configure);
  container_->filter_widget()->AddMenuAction(action_configure);

  QObject::connect(container_->view(), &StreamingCollectionView::GetSongs, this, &StreamingSongsView::GetSongs);
  QObject::connect(container_->view(), &StreamingCollectionView::RemoveSongs, &*service_, &StreamingService::RemoveSongsByList);

  QObject::connect(container_->button_refresh(), &QPushButton::clicked, this, &StreamingSongsView::GetSongs);
  QObject::connect(container_->button_close(), &QPushButton::clicked, this, &StreamingSongsView::AbortGetSongs);
  QObject::connect(container_->button_abort(), &QPushButton::clicked, this, &StreamingSongsView::AbortGetSongs);
  QObject::connect(&*service_, &StreamingService::ShowErrorDialog, this, &StreamingSongsView::ShowErrorDialog);
  QObject::connect(&*service_, &StreamingService::SongsResults, this, &StreamingSongsView::SongsFinished);
  QObject::connect(&*service_, &StreamingService::SongsUpdateStatus, container_->status(), &QLabel::setText);
  QObject::connect(&*service_, &StreamingService::SongsProgressSetMaximum, container_->progressbar(), &QProgressBar::setMaximum);
  QObject::connect(&*service_, &StreamingService::SongsUpdateProgress, container_->progressbar(), &QProgressBar::setValue);

  ReloadSettings();

}

StreamingCollectionView *StreamingSongsView::view() const {
  return container_->view();
}

bool StreamingSongsView::SearchFieldHasFocus() const {
  return container_->SearchFieldHasFocus();
}

void StreamingSongsView::FocusSearchField() {
  container_->FocusSearchField();
}

void StreamingSongsView::ReloadSettings() {
  container_->ReloadSettings();
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
    container_->ShowProgress();
  }

  service_->GetSongs();

}

void StreamingSongsView::AbortGetSongs() {

  service_->ResetSongsRequest();

  if (service_->show_progress()) {
    container_->ShowCollection();
  }

}

void StreamingSongsView::SongsFinished(const SongMap &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    container_->ShowError(error);
  }
  else {
    container_->ShowCollection();
    service_->songs_collection_backend()->UpdateSongsBySongIDAsync(songs);
  }

}
