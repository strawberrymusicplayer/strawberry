/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <memory>
#include <algorithm>

#include <QtGlobal>
#include <QObject>
#include <QMainWindow>
#include <QWidget>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QScreen>
#include <QWindow>
#include <QItemSelectionModel>
#include <QListWidgetItem>
#include <QFile>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
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

#include "core/application.h"
#include "core/iconloader.h"
#include "core/utilities.h"
#include "core/imageutils.h"
#include "core/tagreaderclient.h"
#include "core/database.h"
#include "widgets/forcescrollperpixel.h"
#include "widgets/qsearchfield.h"
#include "collection/sqlrow.h"
#include "collection/collectionbackend.h"
#include "collection/collectionquery.h"
#include "playlist/songmimedata.h"
#include "settings/collectionsettingspage.h"
#include "coverproviders.h"
#include "albumcovermanager.h"
#include "albumcoversearcher.h"
#include "albumcoverchoicecontroller.h"
#include "albumcoverexport.h"
#include "albumcoverexporter.h"
#include "albumcoverfetcher.h"
#include "albumcoverloader.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"
#include "albumcovermanagerlist.h"
#include "coversearchstatistics.h"
#include "coversearchstatisticsdialog.h"
#include "albumcoverimageresult.h"

#include "ui_albumcovermanager.h"

const char *AlbumCoverManager::kSettingsGroup = "CoverManager";

AlbumCoverManager::AlbumCoverManager(Application *app, CollectionBackend *collection_backend, QMainWindow *mainwindow, QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui_CoverManager),
      mainwindow_(mainwindow),
      app_(app),
      collection_backend_(collection_backend),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
      filter_all_(nullptr),
      filter_with_covers_(nullptr),
      filter_without_covers_(nullptr),
      cover_fetcher_(new AlbumCoverFetcher(app_->cover_providers(), this)),
      cover_searcher_(nullptr),
      cover_export_(nullptr),
      cover_exporter_(new AlbumCoverExporter(this)),
      artist_icon_(IconLoader::Load("folder-sound")),
      all_artists_icon_(IconLoader::Load("library-music")),
      image_nocover_thumbnail_(ImageUtils::GenerateNoCoverImage(QSize(120, 120))),
      icon_nocover_item_(QPixmap::fromImage(image_nocover_thumbnail_)),
      context_menu_(new QMenu(this)),
      progress_bar_(new QProgressBar(this)),
      abort_progress_(new QPushButton(this)),
      jobs_(0),
      all_artists_(nullptr) {

  ui_->setupUi(this);
  ui_->albums->set_cover_manager(this);

  // Icons
  ui_->action_fetch->setIcon(IconLoader::Load("download"));
  ui_->export_covers->setIcon(IconLoader::Load("document-save"));
  ui_->view->setIcon(IconLoader::Load("view-choose"));
  ui_->button_fetch->setIcon(IconLoader::Load("download"));
  ui_->action_add_to_playlist->setIcon(IconLoader::Load("media-playback-start"));
  ui_->action_load->setIcon(IconLoader::Load("media-playback-start"));

  album_cover_choice_controller_->Init(app_);

  cover_searcher_ = new AlbumCoverSearcher(icon_nocover_item_, app_, this);
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

  cover_loader_options_.scale_output_image_ = true;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.desired_height_ = 120;
  cover_loader_options_.create_thumbnail_ = false;

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
  QObject::connect(ui_->filter, &QSearchField::textChanged, this, &AlbumCoverManager::UpdateFilter);
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
  QSettings s;
  s.beginGroup(kSettingsGroup);

  if (s.contains("geometry")) {
    restoreGeometry(s.value("geometry").toByteArray());
  }

  if (!s.contains("splitter_state") || !ui_->splitter->restoreState(s.value("splitter_state").toByteArray())) {
    // Sensible default size for the artists view
    ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);
  }

  s.endGroup();

  QObject::connect(app_->album_cover_loader(), &AlbumCoverLoader::AlbumCoverLoaded, this, &AlbumCoverManager::AlbumCoverLoaded);
  QObject::connect(app_->album_cover_loader(), &AlbumCoverLoader::SaveEmbeddedCoverAsyncFinished, this, &AlbumCoverManager::SaveEmbeddedCoverAsyncFinished);

  cover_searcher_->Init(cover_fetcher_);

  new ForceScrollPerPixel(ui_->albums, this);

}

void AlbumCoverManager::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {
    LoadGeometry();
    album_cover_choice_controller_->ReloadSettings();
    Reset();
  }

  QMainWindow::showEvent(e);

}

void AlbumCoverManager::closeEvent(QCloseEvent *e) {

  if (!cover_fetching_tasks_.isEmpty()) {
    std::unique_ptr<QMessageBox> message_box(new QMessageBox(QMessageBox::Question, tr("Really cancel?"), tr("Closing this window will stop searching for album covers."), QMessageBox::Abort, this));
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

  QSettings s;
  s.beginGroup(kSettingsGroup);
  if (s.contains("geometry")) {
    restoreGeometry(s.value("geometry").toByteArray());
  }
  if (s.contains("splitter_state")) {
    ui_->splitter->restoreState(s.value("splitter_state").toByteArray());
  }
  else {
    // Sensible default size for the artists view
    ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);
  }
  s.endGroup();

  // Center the window on the same screen as the mainwindow.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  QScreen *screen = mainwindow_->screen();
#else
  QScreen *screen = (mainwindow_->window() && mainwindow_->window()->windowHandle() ? mainwindow_->window()->windowHandle()->screen() : nullptr);
#endif
  if (screen) {
    const QRect sr = screen->availableGeometry();
    const QRect wr({}, size().boundedTo(sr.size()));
    resize(wr.size());
    move(sr.center() - wr.center());
  }

}

void AlbumCoverManager::SaveSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("geometry", saveGeometry());
  s.setValue("splitter_state", ui_->splitter->saveState());
  s.setValue("save_cover_type", album_cover_choice_controller_->get_save_album_cover_type());
  s.endGroup();

}

void AlbumCoverManager::CancelRequests() {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  app_->album_cover_loader()->CancelTasks(QSet<quint64>(cover_loading_tasks_.keyBegin(), cover_loading_tasks_.keyEnd()));
#else
  app_->album_cover_loader()->CancelTasks(QSet<quint64>::fromList(cover_loading_tasks_.keys()));
#endif
  cover_loading_tasks_.clear();
  cover_save_tasks_.clear();
  cover_save_tasks2_.clear();

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

  QStringList artists(collection_backend_->GetAllArtistsWithAlbums());
  std::stable_sort(artists.begin(), artists.end(), CompareNocase);

  for (const QString &artist : artists) {
    if (artist.isEmpty()) continue;
    new QListWidgetItem(artist_icon_, artist, ui_->artists, Specific_Artist);
  }

}

void AlbumCoverManager::EnableCoversButtons() {
  ui_->button_fetch->setEnabled(app_->cover_providers()->HasAnyProviders());
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

  for (const CollectionBackend::Album &info : albums) {

    // Don't show songs without an album, obviously
    if (info.album.isEmpty()) continue;

    QString display_text;

    if (current->type() == Specific_Artist) {
      display_text = info.album;
    }
    else {
      display_text = info.album_artist + " - " + info.album;
    }

    AlbumItem *item = new AlbumItem(icon_nocover_item_, display_text, ui_->albums);
    item->setData(Role_AlbumArtist, info.album_artist);
    item->setData(Role_Album, info.album);
    item->setData(Role_Filetype, info.filetype);
    item->setData(Role_CuePath, info.cue_path);
    item->setData(Qt::TextAlignmentRole, QVariant(Qt::AlignTop | Qt::AlignHCenter));
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
    item->urls = info.urls;

    if (info.album_artist.isEmpty()) {
      item->setToolTip(info.album);
    }
    else {
      item->setToolTip(info.album_artist + " - " + info.album);
    }

    if (!info.art_automatic.isEmpty() || !info.art_manual.isEmpty()) {
      item->setData(Role_PathAutomatic, info.art_automatic);
      item->setData(Role_PathManual, info.art_manual);
      quint64 id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, info.art_automatic, info.art_manual, info.urls.first());
      cover_loading_tasks_[id] = item;
    }

  }

  UpdateFilter();

}

void AlbumCoverManager::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!cover_loading_tasks_.contains(id)) return;

  AlbumItem *item = cover_loading_tasks_.take(id);

  if (!result.success || result.image_scaled.isNull() || result.type == AlbumCoverLoaderResult::Type_ManuallyUnset) {
    item->setIcon(icon_nocover_item_);
  }
  else {
    item->setIcon(QPixmap::fromImage(result.image_scaled));
  }

  //item->setData(Role_Image, result.image_original);
  //item->setData(Role_ImageData, result.image_data);

  UpdateFilter();

}

void AlbumCoverManager::UpdateFilter() {

  const QString filter = ui_->filter->text().toLower();
  const bool hide_with_covers = filter_without_covers_->isChecked();
  const bool hide_without_covers = filter_with_covers_->isChecked();

  HideCovers hide = Hide_None;
  if (hide_with_covers) {
    hide = Hide_WithCovers;
  }
  else if (hide_without_covers) {
    hide = Hide_WithoutCovers;
  }

  qint32 total_count = 0;
  qint32 without_cover = 0;

  for (int i = 0; i < ui_->albums->count(); ++i) {
    AlbumItem *item = static_cast<AlbumItem*>(ui_->albums->item(i));
    bool should_hide = ShouldHide(*item, filter, hide);
    item->setHidden(should_hide);

    if (!should_hide) {
      ++total_count;
      if (!ItemHasCover(*item)) {
        ++without_cover;
      }
    }
  }

  ui_->total_albums->setText(QString::number(total_count));
  ui_->without_cover->setText(QString::number(without_cover));

}

bool AlbumCoverManager::ShouldHide(const AlbumItem &item, const QString &filter, HideCovers hide) const {

  bool has_cover = ItemHasCover(item);
  if (hide == Hide_WithCovers && has_cover) {
    return true;
  }
  else if (hide == Hide_WithoutCovers && !has_cover) {
    return true;
  }

  if (filter.isEmpty()) {
    return false;
  }

  QStringList query = filter.split(' ');
  for (const QString &s : query) {
    bool in_text = item.text().contains(s, Qt::CaseInsensitive);
    bool in_albumartist = item.data(Role_AlbumArtist).toString().contains(s, Qt::CaseInsensitive);
    if (!in_text && !in_albumartist) {
      return true;
    }
  }

  return false;

}

void AlbumCoverManager::FetchAlbumCovers() {

  for (int i = 0; i < ui_->albums->count(); ++i) {
    AlbumItem *item = static_cast<AlbumItem*>(ui_->albums->item(i));
    if (item->isHidden()) continue;
    if (ItemHasCover(*item)) continue;

    quint64 id = cover_fetcher_->FetchAlbumCover(item->data(Role_AlbumArtist).toString(), item->data(Role_Album).toString(), QString(), true);
    cover_fetching_tasks_[id] = item;
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

  AlbumItem *item = cover_fetching_tasks_.take(id);
  if (!result.image.isNull()) {
    SaveAndSetCover(item, result);
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
    message += ", " + tr("%1 transferred").arg(Utilities::PrettySize(fetch_statistics_.bytes_transferred_));
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

    for (QListWidgetItem *item : context_menu_items_) {
      AlbumItem *album_item = static_cast<AlbumItem*>(item);
      if (ItemHasCover(*album_item)) some_with_covers = true;
      if (album_item->data(Role_PathManual).toUrl().path() == Song::kManuallyUnsetCover) {
        some_unset = true;
      }
      else if (album_item->data(Role_PathAutomatic).toUrl().isEmpty() && album_item->data(Role_PathManual).toUrl().isEmpty()) {
        some_clear = true;
      }
    }

    album_cover_choice_controller_->show_cover_action()->setEnabled(some_with_covers && context_menu_items_.size() == 1);
    album_cover_choice_controller_->cover_to_file_action()->setEnabled(some_with_covers);
    album_cover_choice_controller_->cover_from_file_action()->setEnabled(context_menu_items_.size() == 1);
    album_cover_choice_controller_->cover_from_url_action()->setEnabled(context_menu_items_.size() == 1);
    album_cover_choice_controller_->search_for_cover_action()->setEnabled(app_->cover_providers()->HasAnyProviders());
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
  return context_menu_items_.size() != 1 ? Song() : ItemAsSong(context_menu_items_[0]);
}

Song AlbumCoverManager::GetFirstSelectedAsSong() {
  return context_menu_items_.isEmpty() ? Song() : ItemAsSong(context_menu_items_[0]);
}

Song AlbumCoverManager::ItemAsSong(AlbumItem *item) {

  Song result(Song::Source_Collection);

  QString title = item->data(Role_Album).toString();
  QString artist_name = item->data(Role_AlbumArtist).toString();
  if (!artist_name.isEmpty()) {
    result.set_title(artist_name + " - " + title);
  }
  else {
    result.set_title(title);
  }

  result.set_artist(item->data(Role_AlbumArtist).toString());
  result.set_albumartist(item->data(Role_AlbumArtist).toString());
  result.set_album(item->data(Role_Album).toString());

  result.set_filetype(Song::FileType(item->data(Role_Filetype).toInt()));
  result.set_url(item->urls.first());
  result.set_cue_path(item->data(Role_CuePath).toString());

  result.set_art_automatic(item->data(Role_PathAutomatic).toUrl());
  result.set_art_manual(item->data(Role_PathManual).toUrl());

  // force validity
  result.set_valid(true);
  result.set_id(0);

  return result;
}

void AlbumCoverManager::ShowCover() {

  Song song = GetSingleSelectionAsSong();
  if (!song.is_valid()) return;

  album_cover_choice_controller_->ShowCover(song);

}

void AlbumCoverManager::FetchSingleCover() {

  for (QListWidgetItem *item : context_menu_items_) {
    AlbumItem *album_item = static_cast<AlbumItem*>(item);
    quint64 id = cover_fetcher_->FetchAlbumCover(album_item->data(Role_AlbumArtist).toString(), album_item->data(Role_Album).toString(), QString(), false);
    cover_fetching_tasks_[id] = album_item;
    jobs_++;
  }

  progress_bar_->setMaximum(jobs_);
  progress_bar_->show();
  abort_progress_->show();
  UpdateStatusText();

}

void AlbumCoverManager::UpdateCoverInList(AlbumItem *item, const QUrl &cover_url) {

  quint64 id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, QUrl(), cover_url);
  item->setData(Role_PathManual, cover_url);
  cover_loading_tasks_[id] = item;

}

void AlbumCoverManager::LoadCoverFromFile() {

  Song song = GetSingleSelectionAsSong();
  if (!song.is_valid()) return;

  AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromFile(&song);
  if (!result.image.isNull()) {
    SaveImageToAlbums(&song, result);
  }

}

void AlbumCoverManager::SaveCoverToFile() {

  Song song = GetSingleSelectionAsSong();
  if (!song.is_valid() || song.has_manually_unset_cover()) return;

  AlbumCoverImageResult result;

  // Load the image from disk

  if (!song.art_manual().isEmpty() && !song.has_manually_unset_cover() && song.art_manual().isLocalFile() && QFile::exists(song.art_manual().toLocalFile())) {
    result.image_data = Utilities::ReadDataFromFile(song.art_manual().toLocalFile());
  }
  else if (!song.art_manual().isEmpty() && !song.art_manual().path().isEmpty() && song.art_manual().scheme().isEmpty() && QFile::exists(song.art_manual().path())) {
    result.image_data = Utilities::ReadDataFromFile(song.art_manual().path());
  }
  else if (song.has_embedded_cover()) {
    result.image_data = TagReaderClient::Instance()->LoadEmbeddedArtBlocking(song.url().toLocalFile());
  }
  else if (!song.art_automatic().isEmpty() && song.art_automatic().isLocalFile() && QFile::exists(song.art_automatic().toLocalFile())) {
    result.image_data = Utilities::ReadDataFromFile(song.art_automatic().toLocalFile());
  }
  else if (!song.art_automatic().isEmpty() && !song.art_automatic().path().isEmpty() && song.art_automatic().scheme().isEmpty() && QFile::exists(song.art_automatic().path())) {
    result.image_data = Utilities::ReadDataFromFile(song.art_automatic().path());
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

  AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromURL();
  if (result.is_valid()) {
    SaveImageToAlbums(&song, result);
  }

}

void AlbumCoverManager::SearchForCover() {

  Song song = GetFirstSelectedAsSong();
  if (!song.is_valid()) return;

  AlbumCoverImageResult result = album_cover_choice_controller_->SearchForImage(&song);
  if (result.is_valid()) {
    SaveImageToAlbums(&song, result);
  }

}

void AlbumCoverManager::SaveImageToAlbums(Song *song, const AlbumCoverImageResult &result) {

  QUrl cover_url = result.cover_url;
  switch (album_cover_choice_controller_->get_save_album_cover_type()) {
    case CollectionSettingsPage::SaveCoverType_Cache:
    case CollectionSettingsPage::SaveCoverType_Album:
      if (cover_url.isEmpty() || !cover_url.isValid() || !cover_url.isLocalFile()) {
        cover_url = album_cover_choice_controller_->SaveCoverToFileAutomatic(song, result);
      }
      break;
    case CollectionSettingsPage::SaveCoverType_Embedded:
      cover_url = QUrl::fromLocalFile(Song::kEmbeddedCover);
      break;
  }

  // Force the found cover on all of the selected items
  QList<QUrl> urls;
  QList<AlbumItem*> album_items;
  for (QListWidgetItem *item : context_menu_items_) {
    AlbumItem *album_item = static_cast<AlbumItem*>(item);
    switch (album_cover_choice_controller_->get_save_album_cover_type()) {
      case CollectionSettingsPage::SaveCoverType_Cache:
      case CollectionSettingsPage::SaveCoverType_Album:{
        Song current_song = ItemAsSong(album_item);
        album_cover_choice_controller_->SaveArtManualToSong(&current_song, cover_url);
        UpdateCoverInList(album_item, cover_url);
        break;
      }
      case CollectionSettingsPage::SaveCoverType_Embedded:{
        urls << album_item->urls;
        album_items << album_item;
        break;
      }
    }
  }

  if (album_cover_choice_controller_->get_save_album_cover_type() == CollectionSettingsPage::SaveCoverType_Embedded && !urls.isEmpty()) {
    quint64 id = -1;
    if (result.is_jpeg()) {
      id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.image_data);
    }
    else {
      id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.image);
    }
    for (AlbumItem *album_item : album_items) {
      cover_save_tasks_.insert(id, album_item);
    }
  }

}

void AlbumCoverManager::UnsetCover() {

  Song song = GetFirstSelectedAsSong();
  if (!song.is_valid()) return;

  AlbumItem *first_album_item = static_cast<AlbumItem*>(context_menu_items_[0]);

  QUrl cover_url = album_cover_choice_controller_->UnsetCover(&song);

  // Force the 'none' cover on all of the selected items
  for (QListWidgetItem *item : context_menu_items_) {
    AlbumItem *album_item = static_cast<AlbumItem*>(item);
    album_item->setIcon(icon_nocover_item_);
    album_item->setData(Role_PathManual, cover_url);

    // Don't save the first one twice
    if (album_item != first_album_item) {
      Song current_song = ItemAsSong(album_item);
      album_cover_choice_controller_->SaveArtManualToSong(&current_song, cover_url);
    }
  }

}

void AlbumCoverManager::ClearCover() {

  Song song = GetFirstSelectedAsSong();
  if (!song.is_valid()) return;

  AlbumItem *first_album_item = static_cast<AlbumItem*>(context_menu_items_[0]);

  album_cover_choice_controller_->ClearCover(&song);

  // Force the 'none' cover on all of the selected items
  for (QListWidgetItem *item : context_menu_items_) {
    AlbumItem *album_item = static_cast<AlbumItem*>(item);
    album_item->setIcon(icon_nocover_item_);
    album_item->setData(Role_PathManual, QUrl());

    // Don't save the first one twice
    if (album_item != first_album_item) {
      Song current_song = ItemAsSong(album_item);
      album_cover_choice_controller_->SaveArtManualToSong(&current_song, QUrl(), false);
    }
  }

}

void AlbumCoverManager::DeleteCover() {

  for (QListWidgetItem *item : context_menu_items_) {
    AlbumItem *album_item = static_cast<AlbumItem*>(item);
    Song song = ItemAsSong(album_item);
    album_cover_choice_controller_->DeleteCover(&song);
    album_item->setIcon(icon_nocover_item_);
    album_item->setData(Role_PathManual, QUrl());
    album_item->setData(Role_PathAutomatic, QUrl());
  }

}

SongList AlbumCoverManager::GetSongsInAlbum(const QModelIndex &idx) const {

  SongList ret;

  QMutexLocker l(collection_backend_->db()->Mutex());
  QSqlDatabase db(collection_backend_->db()->Connect());

  CollectionQuery q(db, collection_backend_->songs_table());
  q.SetColumnSpec("ROWID," + Song::kColumnSpec);
  q.AddWhere("album", idx.data(Role_Album).toString());
  q.SetOrderBy("disc, track, title");

  QString albumartist = idx.data(Role_AlbumArtist).toString();
  if (!albumartist.isEmpty()) {
    q.AddWhere("effective_albumartist", albumartist);
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

  AlbumItem *item = static_cast<AlbumItem*>(idx.internalPointer());
  if (!item) return;
  album_cover_choice_controller_->ShowCover(ItemAsSong(item));

}

void AlbumCoverManager::AddSelectedToPlaylist() {
  emit AddToPlaylist(GetMimeDataForAlbums(ui_->albums->selectionModel()->selectedIndexes()));
}

void AlbumCoverManager::LoadSelectedToPlaylist() {

  SongMimeData *mimedata = GetMimeDataForAlbums(ui_->albums->selectionModel()->selectedIndexes());
  if (mimedata) {
    mimedata->clear_first_ = true;
    emit AddToPlaylist(mimedata);
  }

}

void AlbumCoverManager::SaveAndSetCover(AlbumItem *item, const AlbumCoverImageResult &result) {

  const QString albumartist = item->data(Role_AlbumArtist).toString();
  const QString album = item->data(Role_Album).toString();
  const QList<QUrl> &urls = item->urls;
  const Song::FileType filetype = Song::FileType(item->data(Role_Filetype).toInt());
  const bool has_cue = !item->data(Role_CuePath).toString().isEmpty();

  if (album_cover_choice_controller_->get_save_album_cover_type() == CollectionSettingsPage::SaveCoverType_Embedded && Song::save_embedded_cover_supported(filetype) && !has_cue) {
    if (result.is_jpeg()) {
      quint64 id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.image_data);
      cover_save_tasks_.insert(id, item);
    }
    else if (!result.image.isNull()) {
      quint64 id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.image);
      cover_save_tasks_.insert(id, item);
    }
    else if (!result.cover_url.isEmpty() && result.cover_url.isLocalFile()) {
      quint64 id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.cover_url.toLocalFile());
      cover_save_tasks_.insert(id, item);
    }
  }
  else {
    QUrl cover_url;
    if (!result.cover_url.isEmpty() && result.cover_url.isValid() && result.cover_url.isLocalFile()) {
      cover_url = result.cover_url;
    }
    else if (!result.image_data.isEmpty() || !result.image.isNull()) {
      cover_url = album_cover_choice_controller_->SaveCoverToFileAutomatic(Song::Source_Collection, albumartist, album, QString(), urls.first().adjusted(QUrl::RemoveFilename).path(), result, false);
    }

    if (cover_url.isEmpty()) return;

    // Save the image in the database
    collection_backend_->UpdateManualAlbumArtAsync(albumartist, album, cover_url);

    // Update the icon in our list
    UpdateCoverInList(item, cover_url);
  }

}

void AlbumCoverManager::ExportCovers() {

  AlbumCoverExport::DialogResult result = cover_export_->Exec();

  if (result.cancelled_) {
    return;
  }

  DisableCoversButtons();

  cover_exporter_->SetDialogResult(result);

  for (int i = 0; i < ui_->albums->count(); ++i) {
    AlbumItem *item = static_cast<AlbumItem*>(ui_->albums->item(i));

    // skip hidden and coverless albums
    if (item->isHidden() || !ItemHasCover(*item)) {
      continue;
    }

    cover_exporter_->AddExportRequest(ItemAsSong(item));
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

bool AlbumCoverManager::ItemHasCover(const AlbumItem &item) const {
  return item.icon().cacheKey() != icon_nocover_item_.cacheKey();
}

void AlbumCoverManager::SaveEmbeddedCoverAsyncFinished(quint64 id, const bool success) {

  while (cover_save_tasks_.contains(id)) {
    AlbumItem *album_item = cover_save_tasks_.take(id);
    if (!success) continue;
    album_item->setData(Role_PathAutomatic, QUrl::fromLocalFile(Song::kEmbeddedCover));
    Song song = ItemAsSong(album_item);
    album_cover_choice_controller_->SaveArtAutomaticToSong(&song, QUrl::fromLocalFile(Song::kEmbeddedCover));
    quint64 cover_load_id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, album_item->data(Role_PathAutomatic).toUrl(), album_item->data(Role_PathManual).toUrl(), album_item->urls.first());
    cover_loading_tasks_[cover_load_id] = album_item;
  }

}
