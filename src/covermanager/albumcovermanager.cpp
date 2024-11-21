/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>
#include <chrono>
#include <memory>

#include <QObject>
#include <QMainWindow>
#include <QWidget>
#include <QScreen>
#include <QItemSelectionModel>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QFile>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QImageWriter>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QKeySequence>
#include <QSettings>
#include <QFlags>
#include <QSize>
#include <QtEvents>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "core/songmimedata.h"
#include "utilities/strutils.h"
#include "utilities/fileutils.h"
#include "utilities/imageutils.h"
#include "utilities/mimeutils.h"
#include "utilities/screenutils.h"
#include "widgets/forcescrollperpixel.h"
#include "widgets/searchfield.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionbackend.h"
#include "collection/collectionquery.h"
#include "albumcovermanager.h"
#include "albumcoversearcher.h"
#include "albumcoverchoicecontroller.h"
#include "albumcoverexport.h"
#include "albumcoverexporter.h"
#include "albumcoverfetcher.h"
#include "albumcoverloader.h"
#include "albumcoverloaderresult.h"
#include "coverproviders.h"
#include "coversearchstatistics.h"
#include "coversearchstatisticsdialog.h"
#include "albumcoverimageresult.h"

#include "ui_albumcovermanager.h"

using namespace std::literals::chrono_literals;
using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {
constexpr char kSettingsGroup[] = "CoverManager";
constexpr int kThumbnailSize = 120;
}

AlbumCoverManager::AlbumCoverManager(const SharedPtr<NetworkAccessManager> network,
                                     const SharedPtr<CollectionBackend> collection_backend,
                                     const SharedPtr<TagReaderClient> tagreader_client,
                                     const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                     const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                     const SharedPtr<CoverProviders> cover_providers,
                                     const SharedPtr<StreamingServices> streaming_services,
                                     QMainWindow *mainwindow, QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui_CoverManager),
      mainwindow_(mainwindow),
      network_(network),
      collection_backend_(collection_backend),
      tagreader_client_(tagreader_client),
      albumcover_loader_(albumcover_loader),
      cover_providers_(cover_providers),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
      timer_album_cover_load_(new QTimer(this)),
      filter_all_(nullptr),
      filter_with_covers_(nullptr),
      filter_without_covers_(nullptr),
      cover_fetcher_(new AlbumCoverFetcher(cover_providers, network, this)),
      cover_searcher_(nullptr),
      cover_export_(nullptr),
      cover_exporter_(new AlbumCoverExporter(tagreader_client_, this)),
      artist_icon_(IconLoader::Load(u"folder-sound"_s)),
      all_artists_icon_(IconLoader::Load(u"library-music"_s)),
      image_nocover_thumbnail_(ImageUtils::GenerateNoCoverImage(QSize(120, 120), devicePixelRatio())),
      icon_nocover_item_(QPixmap::fromImage(image_nocover_thumbnail_)),
      context_menu_(new QMenu(this)),
      progress_bar_(new QProgressBar(this)),
      abort_progress_(new QPushButton(this)),
      jobs_(0),
      all_artists_(nullptr) {

  ui_->setupUi(this);
  ui_->albums->set_cover_manager(this);

  timer_album_cover_load_->setSingleShot(false);
  timer_album_cover_load_->setInterval(10ms);
  QObject::connect(timer_album_cover_load_, &QTimer::timeout, this, &AlbumCoverManager::LoadAlbumCovers);

  // Icons
  ui_->action_fetch->setIcon(IconLoader::Load(u"download"_s));
  ui_->export_covers->setIcon(IconLoader::Load(u"document-save"_s));
  ui_->view->setIcon(IconLoader::Load(u"view-choose"_s));
  ui_->button_fetch->setIcon(IconLoader::Load(u"download"_s));
  ui_->action_add_to_playlist->setIcon(IconLoader::Load(u"media-playback-start"_s));
  ui_->action_load->setIcon(IconLoader::Load(u"media-playback-start"_s));

  album_cover_choice_controller_->Init(network, tagreader_client, collection_backend, albumcover_loader, current_albumcover_loader, cover_providers, streaming_services);

  cover_searcher_ = new AlbumCoverSearcher(icon_nocover_item_, albumcover_loader_, this);
  cover_export_ = new AlbumCoverExport(this);

  // Set up the status bar
  statusBar()->addPermanentWidget(progress_bar_);
  statusBar()->addPermanentWidget(abort_progress_);
  progress_bar_->hide();
  abort_progress_->hide();
  abort_progress_->setText(tr("Abort"));
  QObject::connect(abort_progress_, &QPushButton::clicked, this, &AlbumCoverManager::CancelRequests);

  ui_->albums->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui_->artists->setAttribute(Qt::WA_MacShowFocusRect, false);

  QShortcut *close = new QShortcut(QKeySequence::Close, this);
  QObject::connect(close, &QShortcut::activated, this, &AlbumCoverManager::close);

  EnableCoversButtons();

}

AlbumCoverManager::~AlbumCoverManager() {

  CancelRequests();
  delete ui_;

}

void AlbumCoverManager::Init() {

  // View menu
  QActionGroup *filter_group = new QActionGroup(this);
  filter_all_ = filter_group->addAction(tr("All albums"));
  filter_with_covers_ = filter_group->addAction(tr("Albums with covers"));
  filter_without_covers_ = filter_group->addAction(tr("Albums without covers"));
  filter_all_->setCheckable(true);
  filter_with_covers_->setCheckable(true);
  filter_without_covers_->setCheckable(true);
  filter_group->setExclusive(true);
  filter_all_->setChecked(true);

  QMenu *view_menu = new QMenu(this);
  view_menu->addActions(filter_group->actions());

  ui_->view->setMenu(view_menu);

  // Context menu

  QList<QAction*> actions = album_cover_choice_controller_->GetAllActions();

  QObject::connect(album_cover_choice_controller_, &AlbumCoverChoiceController::Error, this, &AlbumCoverManager::Error);
  QObject::connect(album_cover_choice_controller_->cover_from_file_action(), &QAction::triggered, this, &AlbumCoverManager::LoadCoverFromFile);
  QObject::connect(album_cover_choice_controller_->cover_to_file_action(), &QAction::triggered, this, &AlbumCoverManager::SaveCoverToFile);
  QObject::connect(album_cover_choice_controller_->cover_from_url_action(), &QAction::triggered, this, &AlbumCoverManager::LoadCoverFromURL);
  QObject::connect(album_cover_choice_controller_->search_for_cover_action(), &QAction::triggered, this, &AlbumCoverManager::SearchForCover);
  QObject::connect(album_cover_choice_controller_->unset_cover_action(), &QAction::triggered, this, &AlbumCoverManager::UnsetCover);
  QObject::connect(album_cover_choice_controller_->clear_cover_action(), &QAction::triggered, this, &AlbumCoverManager::ClearCover);
  QObject::connect(album_cover_choice_controller_->delete_cover_action(), &QAction::triggered, this, &AlbumCoverManager::DeleteCover);
  QObject::connect(album_cover_choice_controller_->show_cover_action(), &QAction::triggered, this, &AlbumCoverManager::ShowCover);

  QObject::connect(cover_exporter_, &AlbumCoverExporter::AlbumCoversExportUpdate, this, &AlbumCoverManager::UpdateExportStatus);

  context_menu_->addActions(actions);
  context_menu_->addSeparator();
  context_menu_->addAction(ui_->action_load);
  context_menu_->addAction(ui_->action_add_to_playlist);

  ui_->albums->installEventFilter(this);

  // Connections
  QObject::connect(ui_->artists, &QListWidget::currentItemChanged, this, &AlbumCoverManager::ArtistChanged);
  QObject::connect(ui_->filter, &SearchField::textChanged, this, &AlbumCoverManager::UpdateFilter);
  QObject::connect(filter_group, &QActionGroup::triggered, this, &AlbumCoverManager::UpdateFilter);
  QObject::connect(ui_->view, &QToolButton::clicked, ui_->view, &QToolButton::showMenu);
  QObject::connect(ui_->button_fetch, &QPushButton::clicked, this, &AlbumCoverManager::FetchAlbumCovers);
  QObject::connect(ui_->export_covers, &QPushButton::clicked, this, &AlbumCoverManager::ExportCovers);
  QObject::connect(cover_fetcher_, &AlbumCoverFetcher::AlbumCoverFetched, this, &AlbumCoverManager::AlbumCoverFetched);
  QObject::connect(ui_->action_fetch, &QAction::triggered, this, &AlbumCoverManager::FetchSingleCover);
  QObject::connect(ui_->albums, &QListWidget::doubleClicked, this, &AlbumCoverManager::AlbumDoubleClicked);
  QObject::connect(ui_->action_add_to_playlist, &QAction::triggered, this, &AlbumCoverManager::AddSelectedToPlaylist);
  QObject::connect(ui_->action_load, &QAction::triggered, this, &AlbumCoverManager::LoadSelectedToPlaylist);

  // Restore settings
  Settings s;
  s.beginGroup(kSettingsGroup);

  if (s.contains("geometry")) {
    restoreGeometry(s.value("geometry").toByteArray());
  }

  if (!s.contains("splitter_state") || !ui_->splitter->restoreState(s.value("splitter_state").toByteArray())) {
    // Sensible default size for the artists view
    ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);
  }

  s.endGroup();

  QObject::connect(&*albumcover_loader_, &AlbumCoverLoader::AlbumCoverLoaded, this, &AlbumCoverManager::AlbumCoverLoaded);

  cover_searcher_->Init(cover_fetcher_);

  new ForceScrollPerPixel(ui_->albums, this);

}

void AlbumCoverManager::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {
    LoadGeometry();
    cover_types_ = AlbumCoverLoaderOptions::LoadTypes();
    album_cover_choice_controller_->ReloadSettings();
    Reset();
  }

  QMainWindow::showEvent(e);

}

void AlbumCoverManager::closeEvent(QCloseEvent *e) {

  if (!cover_fetching_tasks_.isEmpty()) {
    ScopedPtr<QMessageBox> message_box(new QMessageBox(QMessageBox::Question, tr("Really cancel?"), tr("Closing this window will stop searching for album covers."), QMessageBox::Abort, this));
    message_box->addButton(tr("Don't stop!"), QMessageBox::AcceptRole);

    if (message_box->exec() != QMessageBox::Abort) {
      e->ignore();
      return;
    }
  }

  SaveSettings();

  // Cancel any outstanding requests
  CancelRequests();

  ui_->artists->clear();
  ui_->albums->clear();

  QMainWindow::closeEvent(e);

}

void AlbumCoverManager::LoadGeometry() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  if (s.contains("geometry"_L1)) {
    restoreGeometry(s.value("geometry").toByteArray());
  }
  if (s.contains("splitter_state"_L1)) {
    ui_->splitter->restoreState(s.value("splitter_state").toByteArray());
  }
  else {
    // Sensible default size for the artists view
    ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);
  }
  s.endGroup();

  // Center the window on the same screen as the mainwindow.
  Utilities::CenterWidgetOnScreen(Utilities::GetScreen(mainwindow_), this);

}

void AlbumCoverManager::SaveSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("geometry", saveGeometry());
  s.setValue("splitter_state", ui_->splitter->saveState());
  s.setValue("save_cover_type", static_cast<int>(album_cover_choice_controller_->get_save_album_cover_type()));
  s.endGroup();

}

void AlbumCoverManager::CancelRequests() {

  albumcover_loader_->CancelTasks(QSet<quint64>(cover_loading_tasks_.keyBegin(), cover_loading_tasks_.keyEnd()));
  cover_loading_pending_.clear();
  cover_loading_tasks_.clear();
  cover_save_tasks_.clear();

  cover_exporter_->Cancel();

  cover_fetching_tasks_.clear();
  cover_fetcher_->Clear();
  progress_bar_->hide();
  abort_progress_->hide();
  statusBar()->clearMessage();
  EnableCoversButtons();

}

static bool CompareNocase(const QString &left, const QString &right) {
  return QString::localeAwareCompare(left, right) < 0;
}

static bool CompareAlbumNameNocase(const CollectionBackend::Album &left, const CollectionBackend::Album &right) {
  return CompareNocase(left.album, right.album);
}

void AlbumCoverManager::Reset() {

  EnableCoversButtons();

  ui_->artists->clear();
  all_artists_ = new QListWidgetItem(all_artists_icon_, tr("All artists"), ui_->artists, All_Artists);
  new AlbumItem(artist_icon_, tr("Various artists"), ui_->artists, Various_Artists);

  QStringList artists = collection_backend_->GetAllArtistsWithAlbums();
  std::stable_sort(artists.begin(), artists.end(), CompareNocase);

  for (const QString &artist : std::as_const(artists)) {
    if (artist.isEmpty()) continue;
    new QListWidgetItem(artist_icon_, artist, ui_->artists, Specific_Artist);
  }

}

void AlbumCoverManager::EnableCoversButtons() {
  ui_->button_fetch->setEnabled(cover_providers_->HasAnyProviders());
  ui_->export_covers->setEnabled(true);
}

void AlbumCoverManager::DisableCoversButtons() {
  ui_->button_fetch->setEnabled(false);
  ui_->export_covers->setEnabled(false);
}

void AlbumCoverManager::ArtistChanged(QListWidgetItem *current) {

  if (!current) return;

  ui_->albums->clear();
  context_menu_items_.clear();
  CancelRequests();

  // Get the list of albums.  How we do it depends on what thing we have selected in the artist list.
  CollectionBackend::AlbumList albums;
  switch (current->type()) {
    case Various_Artists: albums = collection_backend_->GetCompilationAlbums(); break;
    case Specific_Artist: albums = collection_backend_->GetAlbumsByArtist(current->text()); break;
    case All_Artists:
    default:              albums = collection_backend_->GetAllAlbums(); break;
  }

  // Sort by album name.  The list is already sorted by sqlite but it was done case sensitively.
  std::stable_sort(albums.begin(), albums.end(), CompareAlbumNameNocase);

  for (const CollectionBackend::Album &album_info : std::as_const(albums)) {

    // Don't show songs without an album, obviously
    if (album_info.album.isEmpty()) continue;

    QString display_text;

    if (current->type() == Specific_Artist) {
      display_text = album_info.album;
    }
    else {
      display_text = album_info.album_artist + " - "_L1 + album_info.album;
    }

    AlbumItem *album_item = new AlbumItem(icon_nocover_item_, display_text, ui_->albums);
    album_item->setData(Role_AlbumArtist, album_info.album_artist);
    album_item->setData(Role_Album, album_info.album);
    album_item->setData(Role_Filetype, QVariant::fromValue(album_info.filetype));
    album_item->setData(Role_CuePath, album_info.cue_path);
    album_item->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignTop | Qt::AlignHCenter));
    album_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
    album_item->urls = album_info.urls;

    if (album_info.album_artist.isEmpty()) {
      album_item->setToolTip(album_info.album);
    }
    else {
      album_item->setToolTip(album_info.album_artist + " - "_L1 + album_info.album);
    }

    album_item->setData(Role_ArtEmbedded, album_info.art_embedded);
    album_item->setData(Role_ArtAutomatic, album_info.art_automatic);
    album_item->setData(Role_ArtManual, album_info.art_manual);
    album_item->setData(Role_ArtUnset, album_info.art_unset);

    if (album_info.art_embedded || !album_info.art_automatic.isEmpty() || !album_info.art_manual.isEmpty()) {
      QueueAlbumCoverLoad(album_item);
    }

  }

  UpdateFilter();

}

void AlbumCoverManager::QueueAlbumCoverLoad(AlbumItem *album_item) {

  cover_loading_pending_.enqueue(album_item);

  if (!timer_album_cover_load_->isActive()) {
    timer_album_cover_load_->start();
  }

}

void AlbumCoverManager::LoadAlbumCovers() {

  if (cover_loading_pending_.isEmpty()) {
    if (timer_album_cover_load_->isActive()) {
      timer_album_cover_load_->stop();
    }
    return;
  }

  LoadAlbumCoverAsync(cover_loading_pending_.dequeue());

}

void AlbumCoverManager::LoadAlbumCoverAsync(AlbumItem *album_item) {

  AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
  cover_options.types = cover_types_;
  cover_options.desired_scaled_size = QSize(kThumbnailSize, kThumbnailSize);
  cover_options.device_pixel_ratio = devicePixelRatioF();
  quint64 cover_load_id = albumcover_loader_->LoadImageAsync(cover_options, album_item->data(Role_ArtEmbedded).toBool(), album_item->data(Role_ArtAutomatic).toUrl(), album_item->data(Role_ArtManual).toUrl(), album_item->data(Role_ArtUnset).toBool(), album_item->urls.constFirst());
  cover_loading_tasks_.insert(cover_load_id, album_item);

}

void AlbumCoverManager::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!cover_loading_tasks_.contains(id)) return;

  AlbumItem *album_item = cover_loading_tasks_.take(id);

  if (!result.success || result.image_scaled.isNull() || result.type == AlbumCoverLoaderResult::Type::Unset) {
    album_item->setIcon(icon_nocover_item_);
  }
  else {
    album_item->setIcon(QPixmap::fromImage(result.image_scaled));
  }

  UpdateFilter();

}

void AlbumCoverManager::UpdateFilter() {

  const QString filter = ui_->filter->text().toLower();
  const bool hide_with_covers = filter_without_covers_->isChecked();
  const bool hide_without_covers = filter_with_covers_->isChecked();

  HideCovers hide_covers = HideCovers::None;
  if (hide_with_covers) {
    hide_covers = HideCovers::WithCovers;
  }
  else if (hide_without_covers) {
    hide_covers = HideCovers::WithoutCovers;
  }

  qint32 total_count = 0;
  qint32 without_cover = 0;

  for (int i = 0; i < ui_->albums->count(); ++i) {
    AlbumItem *album_item = static_cast<AlbumItem*>(ui_->albums->item(i));
    bool should_hide = ShouldHide(*album_item, filter, hide_covers);
    album_item->setHidden(should_hide);

    if (!should_hide) {
      ++total_count;
      if (!ItemHasCover(*album_item)) {
        ++without_cover;
      }
    }
  }

  ui_->total_albums->setText(QString::number(total_count));
  ui_->without_cover->setText(QString::number(without_cover));

}

bool AlbumCoverManager::ShouldHide(const AlbumItem &album_item, const QString &filter, const HideCovers hide_covers) const {

  bool has_cover = ItemHasCover(album_item);
  if (hide_covers == HideCovers::WithCovers && has_cover) {
    return true;
  }
  else if (hide_covers == HideCovers::WithoutCovers && !has_cover) {
    return true;
  }

  if (filter.isEmpty()) {
    return false;
  }

  const QStringList query = filter.split(u' ');
  for (const QString &s : query) {
    bool in_text = album_item.text().contains(s, Qt::CaseInsensitive);
    bool in_albumartist = album_item.data(Role_AlbumArtist).toString().contains(s, Qt::CaseInsensitive);
    if (!in_text && !in_albumartist) {
      return true;
    }
  }

  return false;

}

void AlbumCoverManager::FetchAlbumCovers() {

  for (int i = 0; i < ui_->albums->count(); ++i) {
    AlbumItem *album_item = static_cast<AlbumItem*>(ui_->albums->item(i));
    if (album_item->isHidden()) continue;
    if (ItemHasCover(*album_item)) continue;

    quint64 id = cover_fetcher_->FetchAlbumCover(album_item->data(Role_AlbumArtist).toString(), album_item->data(Role_Album).toString(), QString(), true);
    cover_fetching_tasks_[id] = album_item;
    jobs_++;
  }

  if (!cover_fetching_tasks_.isEmpty()) ui_->button_fetch->setEnabled(false);

  progress_bar_->setMaximum(jobs_);
  progress_bar_->show();
  abort_progress_->show();
  fetch_statistics_ = CoverSearchStatistics();
  UpdateStatusText();

}

void AlbumCoverManager::AlbumCoverFetched(const quint64 id, const AlbumCoverImageResult &result, const CoverSearchStatistics &statistics) {

  if (!cover_fetching_tasks_.contains(id)) return;

  AlbumItem *album_item = cover_fetching_tasks_.take(id);
  if (!result.image.isNull()) {
    SaveAndSetCover(album_item, result);
  }

  if (cover_fetching_tasks_.isEmpty()) {
    EnableCoversButtons();
  }

  fetch_statistics_ += statistics;
  UpdateStatusText();

}

void AlbumCoverManager::UpdateStatusText() {

  QString message = tr("Got %1 covers out of %2 (%3 failed)")
                        .arg(fetch_statistics_.chosen_images_)
                        .arg(jobs_)
                        .arg(fetch_statistics_.missing_images_);

  if (fetch_statistics_.bytes_transferred_ > 0) {
    message += ", "_L1 + tr("%1 transferred").arg(Utilities::PrettySize(fetch_statistics_.bytes_transferred_));
  }

  statusBar()->showMessage(message);
  progress_bar_->setValue(static_cast<int>(fetch_statistics_.chosen_images_ + fetch_statistics_.missing_images_));

  if (cover_fetching_tasks_.isEmpty()) {
    QTimer::singleShot(2000, statusBar(), &QStatusBar::clearMessage);
    progress_bar_->hide();
    abort_progress_->hide();

    CoverSearchStatisticsDialog *dialog = new CoverSearchStatisticsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->Show(fetch_statistics_);

    jobs_ = 0;
  }

}

bool AlbumCoverManager::eventFilter(QObject *obj, QEvent *e) {

  if (obj == ui_->albums && e->type() == QEvent::ContextMenu) {
    context_menu_items_ = ui_->albums->selectedItems();
    if (context_menu_items_.isEmpty()) return QMainWindow::eventFilter(obj, e);

    bool some_with_covers = false;
    bool some_unset = false;
    bool some_clear = false;

    for (QListWidgetItem *list_widget_item : std::as_const(context_menu_items_)) {
      AlbumItem *album_item = static_cast<AlbumItem*>(list_widget_item);
      if (ItemHasCover(*album_item)) some_with_covers = true;
      if (album_item->data(Role_ArtUnset).toBool()) {
        some_unset = true;
      }
      else if (!album_item->data(Role_ArtEmbedded).toBool() && album_item->data(Role_ArtAutomatic).toUrl().isEmpty() && album_item->data(Role_ArtManual).toUrl().isEmpty()) {
        some_clear = true;
      }
    }

    album_cover_choice_controller_->show_cover_action()->setEnabled(some_with_covers && context_menu_items_.size() == 1);
    album_cover_choice_controller_->cover_to_file_action()->setEnabled(some_with_covers);
    album_cover_choice_controller_->cover_from_file_action()->setEnabled(context_menu_items_.size() == 1);
    album_cover_choice_controller_->cover_from_url_action()->setEnabled(context_menu_items_.size() == 1);
    album_cover_choice_controller_->search_for_cover_action()->setEnabled(cover_providers_->HasAnyProviders());
    album_cover_choice_controller_->unset_cover_action()->setEnabled(some_with_covers || some_clear);
    album_cover_choice_controller_->clear_cover_action()->setEnabled(some_with_covers || some_unset);
    album_cover_choice_controller_->delete_cover_action()->setEnabled(some_with_covers);

    QContextMenuEvent *context_menu_event = static_cast<QContextMenuEvent*>(e);
    context_menu_->popup(context_menu_event->globalPos());
    return true;
  }

  return QMainWindow::eventFilter(obj, e);

}

Song AlbumCoverManager::GetSingleSelectionAsSong() {
  return context_menu_items_.size() != 1 ? Song() : AlbumItemAsSong(context_menu_items_.value(0));
}

Song AlbumCoverManager::GetFirstSelectedAsSong() {
  return context_menu_items_.isEmpty() ? Song() : AlbumItemAsSong(context_menu_items_.value(0));
}

Song AlbumCoverManager::AlbumItemAsSong(AlbumItem *album_item) {

  Song result(Song::Source::Collection);

  QString title = album_item->data(Role_Album).toString();
  QString artist_name = album_item->data(Role_AlbumArtist).toString();
  if (!artist_name.isEmpty()) {
    result.set_title(artist_name + " - "_L1 + title);
  }
  else {
    result.set_title(title);
  }

  result.set_artist(album_item->data(Role_AlbumArtist).toString());
  result.set_albumartist(album_item->data(Role_AlbumArtist).toString());
  result.set_album(album_item->data(Role_Album).toString());

  result.set_filetype(static_cast<Song::FileType>(album_item->data(Role_Filetype).toInt()));
  result.set_url(album_item->urls.constFirst());
  result.set_cue_path(album_item->data(Role_CuePath).toString());

  result.set_art_embedded(album_item->data(Role_ArtEmbedded).toBool());
  result.set_art_automatic(album_item->data(Role_ArtAutomatic).toUrl());
  result.set_art_manual(album_item->data(Role_ArtManual).toUrl());
  result.set_art_unset(album_item->data(Role_ArtUnset).toBool());

  // force validity
  result.set_valid(true);
  result.set_id(0);

  return result;

}

void AlbumCoverManager::ShowCover() {

  const Song song = GetSingleSelectionAsSong();
  if (!song.is_valid()) return;

  album_cover_choice_controller_->ShowCover(song);

}

void AlbumCoverManager::FetchSingleCover() {

  for (QListWidgetItem *list_widget_item : std::as_const(context_menu_items_)) {
    AlbumItem *album_item = static_cast<AlbumItem*>(list_widget_item);
    quint64 id = cover_fetcher_->FetchAlbumCover(album_item->data(Role_AlbumArtist).toString(), album_item->data(Role_Album).toString(), QString(), false);
    cover_fetching_tasks_[id] = album_item;
    jobs_++;
  }

  progress_bar_->setMaximum(jobs_);
  progress_bar_->show();
  abort_progress_->show();
  UpdateStatusText();

}

void AlbumCoverManager::UpdateCoverInList(AlbumItem *album_item, const QUrl &cover_url) {

  album_item->setData(Role_ArtManual, cover_url);
  album_item->setData(Role_ArtUnset, false);
  LoadAlbumCoverAsync(album_item);

}

void AlbumCoverManager::LoadCoverFromFile() {

  Song song = GetSingleSelectionAsSong();
  if (!song.is_valid()) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromFile(&song);
  if (!result.image.isNull()) {
    SaveImageToAlbums(&song, result);
  }

}

void AlbumCoverManager::SaveCoverToFile() {

  Song song = GetSingleSelectionAsSong();
  if (!song.is_valid() || song.art_unset()) return;

  // Load the image from disk
  AlbumCoverImageResult result;
  for (const AlbumCoverLoaderOptions::Type cover_type : std::as_const(cover_types_)) {
    switch (cover_type) {
      case AlbumCoverLoaderOptions::Type::Unset:
        return;
      case AlbumCoverLoaderOptions::Type::Embedded:
        if (song.art_embedded()) {
          const TagReaderResult tagreaderclient_result = tagreader_client_->LoadCoverDataBlocking(song.url().toLocalFile(), result.image_data);
          if (!tagreaderclient_result.success()) {
            qLog(Error) << "Could not load embedded art from" << song.url() << tagreaderclient_result.error_string();
          }
        }
        break;
      case AlbumCoverLoaderOptions::Type::Automatic:
        if (song.art_automatic_is_valid()) {
          result.image_data = Utilities::ReadDataFromFile(song.art_automatic().toLocalFile());
        }
        break;
      case AlbumCoverLoaderOptions::Type::Manual:
        if (song.art_manual_is_valid()) {
          result.image_data = Utilities::ReadDataFromFile(song.art_manual().toLocalFile());
        }
        break;
    }
    if (result.is_valid()) break;
  }

  if (!result.is_valid()) return;

  result.mime_type = Utilities::MimeTypeFromData(result.image_data);

  if (!result.image_data.isEmpty()) {
    result.image.loadFromData(result.image_data);
  }

  album_cover_choice_controller_->SaveCoverToFileManual(song, result);

}

void AlbumCoverManager::LoadCoverFromURL() {

  Song song = GetSingleSelectionAsSong();
  if (!song.is_valid()) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromURL();
  if (result.is_valid()) {
    SaveImageToAlbums(&song, result);
  }

}

void AlbumCoverManager::SearchForCover() {

  Song song = GetFirstSelectedAsSong();
  if (!song.is_valid()) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->SearchForImage(&song);
  if (result.is_valid()) {
    SaveImageToAlbums(&song, result);
  }

}

void AlbumCoverManager::SaveImageToAlbums(Song *song, const AlbumCoverImageResult &result) {

  QUrl cover_url = result.cover_url;
  switch (album_cover_choice_controller_->get_save_album_cover_type()) {
    case CoverOptions::CoverType::Cache:
    case CoverOptions::CoverType::Album:
      if (cover_url.isEmpty() || !cover_url.isValid() || !cover_url.isLocalFile()) {
        cover_url = album_cover_choice_controller_->SaveCoverToFileAutomatic(song, result);
      }
      break;
    case CoverOptions::CoverType::Embedded:
      cover_url.clear();
      break;
  }

  // Force the found cover on all of the selected items
  QList<QUrl> urls;
  QList<AlbumItem*> album_items;
  for (QListWidgetItem *list_widget_item : std::as_const(context_menu_items_)) {
    AlbumItem *album_item = static_cast<AlbumItem*>(list_widget_item);
    switch (album_cover_choice_controller_->get_save_album_cover_type()) {
      case CoverOptions::CoverType::Cache:
      case CoverOptions::CoverType::Album:{
        Song current_song = AlbumItemAsSong(album_item);
        album_cover_choice_controller_->SaveArtManualToSong(&current_song, cover_url);
        UpdateCoverInList(album_item, cover_url);
        break;
      }
      case CoverOptions::CoverType::Embedded:{
        for (const QUrl &url : std::as_const(album_item->urls)) {
          const bool art_embedded = !result.image_data.isEmpty();
          TagReaderReplyPtr reply = tagreader_client_->SaveCoverAsync(url.toLocalFile(), SaveTagCoverData(result.image_data, result.mime_type));
          SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
          *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, album_item, url, art_embedded, connection]() {
            SaveEmbeddedCoverFinished(reply, album_item, url, art_embedded);
            QObject::disconnect(*connection);
          });
          cover_save_tasks_.insert(album_item, url);
        }
        urls << album_item->urls;
        album_items << album_item;
        break;
      }
    }
  }

}

void AlbumCoverManager::UnsetCover() {

  if (context_menu_items_.isEmpty()) return;

  // Force the 'none' cover on all of the selected items
  for (QListWidgetItem *list_widget_item : std::as_const(context_menu_items_)) {
    AlbumItem *album_item = static_cast<AlbumItem*>(list_widget_item);
    album_item->setIcon(icon_nocover_item_);
    album_item->setData(Role_ArtEmbedded, false);
    album_item->setData(Role_ArtManual, QUrl());
    album_item->setData(Role_ArtAutomatic, QUrl());
    album_item->setData(Role_ArtUnset, true);

    Song current_song = AlbumItemAsSong(album_item);
    album_cover_choice_controller_->UnsetAlbumCoverForSong(&current_song);
  }

}

void AlbumCoverManager::ClearCover() {

  if (context_menu_items_.isEmpty()) return;

  // Force the 'none' cover on all of the selected items
  for (QListWidgetItem *list_widget_item : std::as_const(context_menu_items_)) {
    AlbumItem *album_item = static_cast<AlbumItem*>(list_widget_item);
    album_item->setIcon(icon_nocover_item_);
    album_item->setData(Role_ArtEmbedded, false);
    album_item->setData(Role_ArtAutomatic, QUrl());
    album_item->setData(Role_ArtManual, QUrl());
    album_item->setData(Role_ArtUnset, false);

    Song current_song = AlbumItemAsSong(album_item);
    album_cover_choice_controller_->ClearAlbumCoverForSong(&current_song);
  }

}

void AlbumCoverManager::DeleteCover() {

  for (QListWidgetItem *list_widget_item : std::as_const(context_menu_items_)) {
    AlbumItem *album_item = static_cast<AlbumItem*>(list_widget_item);
    Song song = AlbumItemAsSong(album_item);
    album_cover_choice_controller_->DeleteCover(&song);
    album_item->setIcon(icon_nocover_item_);
    album_item->setData(Role_ArtEmbedded, false);
    album_item->setData(Role_ArtManual, QUrl());
    album_item->setData(Role_ArtAutomatic, QUrl());
  }

}

SongList AlbumCoverManager::GetSongsInAlbum(const QModelIndex &idx) const {

  SongList ret;

  QMutexLocker l(collection_backend_->db()->Mutex());
  QSqlDatabase db(collection_backend_->db()->Connect());

  CollectionQuery q(db, collection_backend_->songs_table());
  q.SetColumnSpec(Song::kRowIdColumnSpec);
  q.AddWhere(u"album"_s, idx.data(Role_Album).toString());
  q.SetOrderBy(u"disc, track, title"_s);

  QString albumartist = idx.data(Role_AlbumArtist).toString();
  if (!albumartist.isEmpty()) {
    q.AddWhere(u"effective_albumartist"_s, albumartist);
  }

  q.AddCompilationRequirement(albumartist.isEmpty());

  if (!q.Exec()) return ret;

  while (q.Next()) {
    Song song;
    song.InitFromQuery(q, true);
    ret << song;
  }
  return ret;

}

SongList AlbumCoverManager::GetSongsInAlbums(const QModelIndexList &indexes) const {

  SongList ret;
  for (const QModelIndex &idx : indexes) {
    ret << GetSongsInAlbum(idx);
  }
  return ret;

}

SongMimeData *AlbumCoverManager::GetMimeDataForAlbums(const QModelIndexList &indexes) const {

  SongList songs = GetSongsInAlbums(indexes);
  if (songs.isEmpty()) return nullptr;

  SongMimeData *mimedata = new SongMimeData;
  mimedata->backend = collection_backend_;
  mimedata->songs = songs;
  return mimedata;

}

void AlbumCoverManager::AlbumDoubleClicked(const QModelIndex &idx) {

  AlbumItem *album_item = static_cast<AlbumItem*>(idx.internalPointer());
  if (!album_item) return;
  album_cover_choice_controller_->ShowCover(AlbumItemAsSong(album_item));

}

void AlbumCoverManager::AddSelectedToPlaylist() {
  Q_EMIT AddToPlaylist(GetMimeDataForAlbums(ui_->albums->selectionModel()->selectedIndexes()));
}

void AlbumCoverManager::LoadSelectedToPlaylist() {

  SongMimeData *mimedata = GetMimeDataForAlbums(ui_->albums->selectionModel()->selectedIndexes());
  if (mimedata) {
    mimedata->clear_first_ = true;
    Q_EMIT AddToPlaylist(mimedata);
  }

}

void AlbumCoverManager::SaveAndSetCover(AlbumItem *album_item, const AlbumCoverImageResult &result) {

  const QList<QUrl> &urls = album_item->urls;
  const Song::FileType filetype = static_cast<Song::FileType>(album_item->data(Role_Filetype).toInt());
  const bool has_cue = !album_item->data(Role_CuePath).toString().isEmpty();

  if (album_cover_choice_controller_->get_save_album_cover_type() == CoverOptions::CoverType::Embedded && Song::save_embedded_cover_supported(filetype) && !has_cue) {
    for (const QUrl &url : urls) {
      const bool art_embedded = !result.image_data.isEmpty();
      TagReaderReplyPtr reply = tagreader_client_->SaveCoverAsync(url.toLocalFile(), SaveTagCoverData(result.cover_url.isValid() ? result.cover_url.toLocalFile() : QString(), result.image_data, result.mime_type));
      SharedPtr<QMetaObject::Connection> connection = std::make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, album_item, url, art_embedded, connection]() {
        SaveEmbeddedCoverFinished(reply, album_item, url, art_embedded);
        QObject::disconnect(*connection);
      });
      cover_save_tasks_.insert(album_item, url);
    }
  }
  else {
    const QString albumartist = album_item->data(Role_AlbumArtist).toString();
    const QString album = album_item->data(Role_Album).toString();
    QUrl cover_url;
    if (!result.cover_url.isEmpty() && result.cover_url.isValid() && result.cover_url.isLocalFile()) {
      cover_url = result.cover_url;
    }
    else if (!result.image_data.isEmpty() || !result.image.isNull()) {
      cover_url = album_cover_choice_controller_->SaveCoverToFileAutomatic(Song::Source::Collection, albumartist, album, QString(), QFileInfo(urls.first().toLocalFile()).path(), result, false);
    }

    if (cover_url.isEmpty()) return;

    // Save the image in the database
    collection_backend_->UpdateManualAlbumArtAsync(albumartist, album, cover_url);

    // Update the icon in our list
    UpdateCoverInList(album_item, cover_url);
  }

}

void AlbumCoverManager::ExportCovers() {

  AlbumCoverExport::DialogResult result = cover_export_->Exec();

  if (result.cancelled_) {
    return;
  }

  DisableCoversButtons();

  cover_exporter_->SetDialogResult(result);
  cover_exporter_->SetCoverTypes(cover_types_);

  for (int i = 0; i < ui_->albums->count(); ++i) {
    AlbumItem *album_item = static_cast<AlbumItem*>(ui_->albums->item(i));

    // skip hidden and coverless albums
    if (album_item->isHidden() || !ItemHasCover(*album_item)) {
      continue;
    }

    cover_exporter_->AddExportRequest(AlbumItemAsSong(album_item));
  }

  if (cover_exporter_->request_count() > 0) {
    progress_bar_->setMaximum(cover_exporter_->request_count());
    progress_bar_->show();
    abort_progress_->show();

    cover_exporter_->StartExporting();
  }
  else {
    QMessageBox msg;
    msg.setWindowTitle(tr("Export finished"));
    msg.setText(tr("No covers to export."));
    msg.exec();
  }

}

void AlbumCoverManager::UpdateExportStatus(const int exported, const int skipped, const int max) {

  progress_bar_->setValue(exported);

  QString message = tr("Exported %1 covers out of %2 (%3 skipped)")
                        .arg(exported)
                        .arg(max)
                        .arg(skipped);
  statusBar()->showMessage(message);

  // End of the current process
  if (exported + skipped >= max) {
    QTimer::singleShot(2000, statusBar(), &QStatusBar::clearMessage);

    progress_bar_->hide();
    abort_progress_->hide();
    EnableCoversButtons();

    QMessageBox msg;
    msg.setWindowTitle(tr("Export finished"));
    msg.setText(message);
    msg.exec();
  }

}

bool AlbumCoverManager::ItemHasCover(const AlbumItem &album_item) const {
  return album_item.icon().cacheKey() != icon_nocover_item_.cacheKey();
}

void AlbumCoverManager::SaveEmbeddedCoverFinished(TagReaderReplyPtr reply, AlbumItem *album_item, const QUrl &url, const bool art_embedded) {

  if (cover_save_tasks_.contains(album_item, url)) {
    cover_save_tasks_.remove(album_item, url);
  }

  if (!reply->success()) {
    Q_EMIT Error(tr("Could not save cover to file %1.").arg(url.toLocalFile()));
    return;
  }

  if (cover_save_tasks_.contains(album_item)) {
    return;
  }

  album_item->setData(Role_ArtEmbedded, true);
  album_item->setData(Role_ArtUnset, false);
  Song song = AlbumItemAsSong(album_item);
  album_cover_choice_controller_->SaveArtEmbeddedToSong(&song, art_embedded);
  LoadAlbumCoverAsync(album_item);

}

