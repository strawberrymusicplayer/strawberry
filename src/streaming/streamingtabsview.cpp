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
#include <QVariant>
#include <QString>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QContextMenuEvent>
#include <QAction>
#include <QSettings>

#include "core/iconloader.h"
#include "core/settings.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"
#include "collection/collectionfilterwidget.h"
#include "streamingservice.h"
#include "streamingtabsview.h"
#include "streamingcollectionview.h"
#include "streamingcollectionviewcontainer.h"
#include "ui_streamingtabsview.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kTab[] = "tab";
constexpr char kDefaultTab[] = "artists";
}  // namespace

StreamingTabsView::StreamingTabsView(const StreamingServicePtr service, const SharedPtr<AlbumCoverLoader> albumcover_loader, const QString &settings_group, QWidget *parent)
    : QWidget(parent),
      service_(service),
      settings_group_(settings_group),
      ui_(new Ui_StreamingTabsView) {

  ui_->setupUi(this);

  ui_->search_view->Init(service, albumcover_loader);
  QObject::connect(ui_->search_view, &StreamingSearchView::AddArtistsSignal, &*service_, &StreamingService::AddArtists);
  QObject::connect(ui_->search_view, &StreamingSearchView::AddAlbumsSignal, &*service_, &StreamingService::AddAlbums);
  QObject::connect(ui_->search_view, &StreamingSearchView::AddSongsSignal, &*service_, &StreamingService::AddSongs);

  QAction *action_configure = new QAction(IconLoader::Load(u"configure"_s), tr("Configure %1...").arg(Song::TextForSource(service_->source())), this);
  QObject::connect(action_configure, &QAction::triggered, this, &StreamingTabsView::Configure);

  if (service_->artists_collection_model()) {
    ui_->artists_collection->Init(service_->artists_collection_model(), service_->artists_collection_filter_model(), settings_group, u"artists"_s, true);
    ui_->artists_collection->filter_widget()->AddMenuAction(action_configure);

    QObject::connect(ui_->artists_collection->view(), &StreamingCollectionView::GetSongs, this, &StreamingTabsView::GetArtists);
    QObject::connect(ui_->artists_collection->view(), &StreamingCollectionView::RemoveSongs, &*service_, &StreamingService::RemoveArtists);

    QObject::connect(ui_->artists_collection->button_refresh(), &QPushButton::clicked, this, &StreamingTabsView::GetArtists);
    QObject::connect(ui_->artists_collection->button_close(), &QPushButton::clicked, this, &StreamingTabsView::AbortGetArtists);
    QObject::connect(ui_->artists_collection->button_abort(), &QPushButton::clicked, this, &StreamingTabsView::AbortGetArtists);
    QObject::connect(&*service_, &StreamingService::ArtistsResults, this, &StreamingTabsView::ArtistsFinished);
    QObject::connect(&*service_, &StreamingService::ArtistsUpdateStatus, ui_->artists_collection->status(), &QLabel::setText);
    QObject::connect(&*service_, &StreamingService::ArtistsProgressSetMaximum, ui_->artists_collection->progressbar(), &QProgressBar::setMaximum);
    QObject::connect(&*service_, &StreamingService::ArtistsUpdateProgress, ui_->artists_collection->progressbar(), &QProgressBar::setValue);
  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->artists));
  }

  if (service_->albums_collection_model()) {
    ui_->albums_collection->Init(service_->albums_collection_model(), service_->albums_collection_filter_model(), settings_group, u"albums"_s, true);
    ui_->albums_collection->filter_widget()->AddMenuAction(action_configure);

    QObject::connect(ui_->albums_collection->view(), &StreamingCollectionView::GetSongs, this, &StreamingTabsView::GetAlbums);
    QObject::connect(ui_->albums_collection->view(), &StreamingCollectionView::RemoveSongs, &*service_, &StreamingService::RemoveAlbums);

    QObject::connect(ui_->albums_collection->button_refresh(), &QPushButton::clicked, this, &StreamingTabsView::GetAlbums);
    QObject::connect(ui_->albums_collection->button_close(), &QPushButton::clicked, this, &StreamingTabsView::AbortGetAlbums);
    QObject::connect(ui_->albums_collection->button_abort(), &QPushButton::clicked, this, &StreamingTabsView::AbortGetAlbums);
    QObject::connect(&*service_, &StreamingService::AlbumsResults, this, &StreamingTabsView::AlbumsFinished);
    QObject::connect(&*service_, &StreamingService::AlbumsUpdateStatus, ui_->albums_collection->status(), &QLabel::setText);
    QObject::connect(&*service_, &StreamingService::AlbumsProgressSetMaximum, ui_->albums_collection->progressbar(), &QProgressBar::setMaximum);
    QObject::connect(&*service_, &StreamingService::AlbumsUpdateProgress, ui_->albums_collection->progressbar(), &QProgressBar::setValue);
  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->albums));
  }

  if (service_->songs_collection_model()) {
    ui_->songs_collection->Init(service_->songs_collection_model(), service_->songs_collection_filter_model(), settings_group, u"songs"_s, true);
    ui_->songs_collection->filter_widget()->AddMenuAction(action_configure);

    QObject::connect(ui_->songs_collection->view(), &StreamingCollectionView::GetSongs, this, &StreamingTabsView::GetSongs);
    QObject::connect(ui_->songs_collection->view(), &StreamingCollectionView::RemoveSongs, &*service_, &StreamingService::RemoveSongsByList);

    QObject::connect(ui_->songs_collection->button_refresh(), &QPushButton::clicked, this, &StreamingTabsView::GetSongs);
    QObject::connect(ui_->songs_collection->button_close(), &QPushButton::clicked, this, &StreamingTabsView::AbortGetSongs);
    QObject::connect(ui_->songs_collection->button_abort(), &QPushButton::clicked, this, &StreamingTabsView::AbortGetSongs);
    QObject::connect(&*service_, &StreamingService::SongsResults, this, &StreamingTabsView::SongsFinished);
    QObject::connect(&*service_, &StreamingService::SongsUpdateStatus, ui_->songs_collection->status(), &QLabel::setText);
    QObject::connect(&*service_, &StreamingService::SongsProgressSetMaximum, ui_->songs_collection->progressbar(), &QProgressBar::setMaximum);
    QObject::connect(&*service_, &StreamingService::SongsUpdateProgress, ui_->songs_collection->progressbar(), &QProgressBar::setValue);
  }
  else {
    ui_->tabs->removeTab(ui_->tabs->indexOf(ui_->songs));
  }

  Settings s;
  s.beginGroup(settings_group_);
  const QString tab = s.value(kTab, QLatin1String(kDefaultTab)).toString().toLower();
  s.endGroup();

  if (tab == "artists"_L1) {
    ui_->tabs->setCurrentWidget(ui_->artists);
  }
  else if (tab == "albums"_L1) {
    ui_->tabs->setCurrentWidget(ui_->albums);
  }
  else if (tab == "songs"_L1) {
    ui_->tabs->setCurrentWidget(ui_->songs);
  }
  else if (tab == "search"_L1) {
    ui_->tabs->setCurrentWidget(ui_->search);
  }

  ReloadSettings();

}

StreamingTabsView::~StreamingTabsView() {

  Settings s;
  s.beginGroup(settings_group_);
  if (QWidget *current = ui_->tabs->currentWidget()) {
    s.setValue(kTab, current->objectName().toLower());
  }
  s.endGroup();

  delete ui_;

}

void StreamingTabsView::ReloadSettings() {

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

bool StreamingTabsView::SearchFieldHasFocus() const {

  return ((ui_->tabs->currentWidget() == ui_->artists && ui_->artists_collection->SearchFieldHasFocus()) ||
      (ui_->tabs->currentWidget() == ui_->albums && ui_->albums_collection->SearchFieldHasFocus()) ||
      (ui_->tabs->currentWidget() == ui_->songs && ui_->songs_collection->SearchFieldHasFocus()) ||
      (ui_->tabs->currentWidget() == ui_->search && ui_->search_view->SearchFieldHasFocus()));

}

void StreamingTabsView::FocusSearchField() {

  if (ui_->tabs->currentWidget() == ui_->artists) {
    ui_->artists_collection->FocusSearchField();
  }
  else if (ui_->tabs->currentWidget() == ui_->albums) {
    ui_->albums_collection->FocusSearchField();
  }
  else if (ui_->tabs->currentWidget() == ui_->songs) {
    ui_->songs_collection->FocusSearchField();
  }
  else if (ui_->tabs->currentWidget() == ui_->search) {
    ui_->search_view->FocusSearchField();
  }

}

void StreamingTabsView::GetArtists() {

  if (!service_->authenticated() && service_->oauth()) {
    Configure();
    return;
  }

  ui_->artists_collection->ShowProgress();
  service_->GetArtists();

}

void StreamingTabsView::AbortGetArtists() {

  service_->ResetArtistsRequest();
  ui_->artists_collection->ShowCollection();

}

void StreamingTabsView::ArtistsFinished(const SongMap &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->artists_collection->ShowError(error);
  }
  else {
    ui_->artists_collection->ShowCollection();
    service_->artists_collection_backend()->UpdateSongsBySongIDAsync(songs);
  }

}

void StreamingTabsView::GetAlbums() {

  if (!service_->authenticated() && service_->oauth()) {
    Configure();
    return;
  }

  ui_->albums_collection->ShowProgress();
  service_->GetAlbums();

}

void StreamingTabsView::AbortGetAlbums() {

  service_->ResetAlbumsRequest();
  ui_->albums_collection->ShowCollection();

}

void StreamingTabsView::AlbumsFinished(const SongMap &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->albums_collection->ShowError(error);
  }
  else {
    ui_->albums_collection->ShowCollection();
    service_->albums_collection_backend()->UpdateSongsBySongIDAsync(songs);
  }

}

void StreamingTabsView::GetSongs() {

  if (!service_->authenticated() && service_->oauth()) {
    Configure();
    return;
  }

  ui_->songs_collection->ShowProgress();
  service_->GetSongs();

}

void StreamingTabsView::AbortGetSongs() {

  service_->ResetSongsRequest();
  ui_->songs_collection->ShowCollection();

}

void StreamingTabsView::SongsFinished(const SongMap &songs, const QString &error) {

  if (songs.isEmpty() && !error.isEmpty()) {
    ui_->songs_collection->ShowError(error);
  }
  else {
    ui_->songs_collection->ShowCollection();
    service_->songs_collection_backend()->UpdateSongsBySongIDAsync(songs);
  }

}

void StreamingTabsView::Configure() {
  Q_EMIT OpenSettingsDialog(service_->source());
}
