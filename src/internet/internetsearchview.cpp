/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <QPair>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QSize>
#include <QStandardItem>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QStackedWidget>
#include <QLabel>
#include <QProgressBar>
#include <QRadioButton>
#include <QScrollArea>
#include <QToolButton>
#include <QEvent>
#include <QTimerEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QtGlobal>

#include "core/application.h"
#include "core/mimedata.h"
#include "core/iconloader.h"
#include "core/song.h"
#include "core/logging.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionmodel.h"
#include "collection/groupbydialog.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "internetsongmimedata.h"
#include "internetservice.h"
#include "internetsearchitemdelegate.h"
#include "internetsearchmodel.h"
#include "internetsearchsortmodel.h"
#include "internetsearchview.h"
#include "ui_internetsearchview.h"

using std::placeholders::_1;
using std::placeholders::_2;

const int InternetSearchView::kSwapModelsTimeoutMsec = 250;
const int InternetSearchView::kDelayedSearchTimeoutMs = 200;
const int InternetSearchView::kArtHeight = 32;

InternetSearchView::InternetSearchView(QWidget *parent)
    : QWidget(parent),
      app_(nullptr),
      service_(nullptr),
      ui_(new Ui_InternetSearchView),
      context_menu_(nullptr),
      group_by_actions_(nullptr),
      front_model_(nullptr),
      back_model_(nullptr),
      current_model_(nullptr),
      front_proxy_(new InternetSearchSortModel(this)),
      back_proxy_(new InternetSearchSortModel(this)),
      current_proxy_(front_proxy_),
      swap_models_timer_(new QTimer(this)),
      use_pretty_covers_(true),
      search_type_(InternetSearchView::SearchType_Artists),
      search_error_(false),
      last_search_id_(0),
      searches_next_id_(1) {

  ui_->setupUi(this);

  ui_->search->installEventFilter(this);
  ui_->results_stack->installEventFilter(this);

  ui_->settings->setIcon(IconLoader::Load("configure"));

  // Set the appearance of the results list
  ui_->results->setItemDelegate(new InternetSearchItemDelegate(this));
  ui_->results->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui_->results->setStyleSheet("QTreeView::item{padding-top:1px;}");

  // Show the help page initially
  ui_->results_stack->setCurrentWidget(ui_->help_page);
  ui_->help_frame->setBackgroundRole(QPalette::Base);

  // Set the colour of the help text to the disabled window text colour
  QPalette help_palette = ui_->label_helptext->palette();
  const QColor help_color = help_palette.color(QPalette::Disabled, QPalette::WindowText);
  help_palette.setColor(QPalette::Normal, QPalette::WindowText, help_color);
  help_palette.setColor(QPalette::Inactive, QPalette::WindowText, help_color);
  ui_->label_helptext->setPalette(help_palette);

  // Make it bold
  QFont help_font = ui_->label_helptext->font();
  help_font.setBold(true);
  ui_->label_helptext->setFont(help_font);

  // Hide progressbar
  ui_->progressbar->hide();
  ui_->progressbar->reset();

  cover_loader_options_.desired_height_ = kArtHeight;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;

}

InternetSearchView::~InternetSearchView() { delete ui_; }

void InternetSearchView::Init(Application *app, InternetService *service) {

  app_ = app;
  service_ = service;

  front_model_ = new InternetSearchModel(service, this);
  back_model_ = new InternetSearchModel(service, this);

  front_proxy_ = new InternetSearchSortModel(this);
  back_proxy_ = new InternetSearchSortModel(this);

  front_model_->set_proxy(front_proxy_);
  back_model_->set_proxy(back_proxy_);

  current_model_ = front_model_;
  current_proxy_ = front_proxy_;

  // Set up the sorting proxy model
  front_proxy_->setSourceModel(front_model_);
  front_proxy_->setDynamicSortFilter(true);
  front_proxy_->sort(0);

  back_proxy_->setSourceModel(back_model_);
  back_proxy_->setDynamicSortFilter(true);
  back_proxy_->sort(0);

  // Add actions to the settings menu
  group_by_actions_ = CollectionFilterWidget::CreateGroupByActions(this);
  QMenu *settings_menu = new QMenu(this);
  settings_menu->addActions(group_by_actions_->actions());
  settings_menu->addSeparator();
  settings_menu->addAction(IconLoader::Load("configure"), tr("Configure %1...").arg(Song::TextForSource(service_->source())), this, SLOT(OpenSettingsDialog()));
  ui_->settings->setMenu(settings_menu);

  swap_models_timer_->setSingleShot(true);
  swap_models_timer_->setInterval(kSwapModelsTimeoutMsec);
  connect(swap_models_timer_, SIGNAL(timeout()), SLOT(SwapModels()));

  connect(ui_->radiobutton_search_artists, SIGNAL(clicked(bool)), SLOT(SearchArtistsClicked(bool)));
  connect(ui_->radiobutton_search_albums, SIGNAL(clicked(bool)), SLOT(SearchAlbumsClicked(bool)));
  connect(ui_->radiobutton_search_songs, SIGNAL(clicked(bool)), SLOT(SearchSongsClicked(bool)));
  connect(group_by_actions_, SIGNAL(triggered(QAction*)), SLOT(GroupByClicked(QAction*)));
  connect(group_by_actions_, SIGNAL(triggered(QAction*)), SLOT(GroupByClicked(QAction*)));

  connect(ui_->search, SIGNAL(textChanged(QString)), SLOT(TextEdited(QString)));
  connect(ui_->results, SIGNAL(AddToPlaylistSignal(QMimeData*)), SIGNAL(AddToPlaylist(QMimeData*)));
  connect(ui_->results, SIGNAL(FocusOnFilterSignal(QKeyEvent*)), SLOT(FocusOnFilter(QKeyEvent*)));

  connect(service_, SIGNAL(SearchUpdateStatus(int, QString)), SLOT(UpdateStatus(int, QString)));
  connect(service_, SIGNAL(SearchProgressSetMaximum(int, int)), SLOT(ProgressSetMaximum(int, int)));
  connect(service_, SIGNAL(SearchUpdateProgress(int, int)), SLOT(UpdateProgress(int, int)));
  connect(service_, SIGNAL(SearchResults(int, SongList, QString)), SLOT(SearchDone(int, SongList, QString)));

  connect(app_, SIGNAL(SettingsChanged()), SLOT(ReloadSettings()));
  connect(app_->album_cover_loader(), SIGNAL(AlbumCoverLoaded(quint64, AlbumCoverLoaderResult)), SLOT(AlbumCoverLoaded(quint64, AlbumCoverLoaderResult)));

  ReloadSettings();

}

void InternetSearchView::ReloadSettings() {

  QSettings s;

  // Collection settings

  s.beginGroup(service_->settings_group());
  use_pretty_covers_ = s.value("pretty_covers", true).toBool();
  front_model_->set_use_pretty_covers(use_pretty_covers_);
  back_model_->set_use_pretty_covers(use_pretty_covers_);

  // Internet search settings

  search_type_ = InternetSearchView::SearchType(s.value("type", int(InternetSearchView::SearchType_Artists)).toInt());
  switch (search_type_) {
    case InternetSearchView::SearchType_Artists:
      ui_->radiobutton_search_artists->setChecked(true);
      break;
    case InternetSearchView::SearchType_Albums:
      ui_->radiobutton_search_albums->setChecked(true);
      break;
    case InternetSearchView::SearchType_Songs:
      ui_->radiobutton_search_songs->setChecked(true);
      break;
  }

  SetGroupBy(CollectionModel::Grouping(
      CollectionModel::GroupBy(s.value("search_group_by1", int(CollectionModel::GroupBy_AlbumArtist)).toInt()),
      CollectionModel::GroupBy(s.value("search_group_by2", int(CollectionModel::GroupBy_Album)).toInt()),
      CollectionModel::GroupBy(s.value("search_group_by3", int(CollectionModel::GroupBy_None)).toInt())));
  s.endGroup();

}

void InternetSearchView::showEvent(QShowEvent *e) {

  QWidget::showEvent(e);
  FocusSearchField();

}

bool InternetSearchView::eventFilter(QObject *object, QEvent *e) {

  if (object == ui_->search && e->type() == QEvent::KeyRelease) {
    if (SearchKeyEvent(static_cast<QKeyEvent*>(e))) {
      return true;
    }
  }
  else if (object == ui_->results_stack && e->type() == QEvent::ContextMenu) {
    if (ResultsContextMenuEvent(static_cast<QContextMenuEvent*>(e))) {
      return true;
    }
  }

  return QWidget::eventFilter(object, e);

}

bool InternetSearchView::SearchKeyEvent(QKeyEvent *e) {

  switch (e->key()) {
    case Qt::Key_Up:
      ui_->results->UpAndFocus();
      break;

    case Qt::Key_Down:
      ui_->results->DownAndFocus();
      break;

    case Qt::Key_Escape:
      ui_->search->clear();
      break;

    case Qt::Key_Return:
      TextEdited(ui_->search->text());
      break;

    default:
      return false;
  }

  e->accept();
  return true;

}

bool InternetSearchView::ResultsContextMenuEvent(QContextMenuEvent *e) {

  context_menu_ = new QMenu(this);
  context_actions_ << context_menu_->addAction( IconLoader::Load("media-playback-start"), tr("Append to current playlist"), this, SLOT(AddSelectedToPlaylist()));
  context_actions_ << context_menu_->addAction( IconLoader::Load("media-playback-start"), tr("Replace current playlist"), this, SLOT(LoadSelected()));
  context_actions_ << context_menu_->addAction( IconLoader::Load("document-new"), tr("Open in new playlist"), this, SLOT(OpenSelectedInNewPlaylist()));

  context_menu_->addSeparator();
  context_actions_ << context_menu_->addAction(IconLoader::Load("go-next"), tr("Queue track"), this, SLOT(AddSelectedToPlaylistEnqueue()));

  context_menu_->addSeparator();

  if (service_->artists_collection_model() || service_->albums_collection_model() || service_->songs_collection_model()) {
    if (service_->artists_collection_model()) {
      context_actions_ << context_menu_->addAction(IconLoader::Load("folder-new"), tr("Add to artists"), this, SLOT(AddArtists()));
    }
    if (service_->albums_collection_model()) {
      context_actions_ << context_menu_->addAction(IconLoader::Load("folder-new"), tr("Add to albums"), this, SLOT(AddAlbums()));
    }
    if (service_->songs_collection_model()) {
      context_actions_ << context_menu_->addAction(IconLoader::Load("folder-new"), tr("Add to songs"), this, SLOT(AddSongs()));
    }
    context_menu_->addSeparator();
  }

  if (ui_->results->selectionModel() && ui_->results->selectionModel()->selectedRows().length() == 1) {
    context_actions_ << context_menu_->addAction(IconLoader::Load("search"), tr("Search for this"), this, SLOT(SearchForThis()));
  }

  context_menu_->addSeparator();
  context_menu_->addMenu(tr("Group by"))->addActions(group_by_actions_->actions());

  context_menu_->addAction(IconLoader::Load("configure"), tr("Configure %1...").arg(Song::TextForSource(service_->source())), this, SLOT(OpenSettingsDialog()));

  const bool enable_context_actions = ui_->results->selectionModel() && ui_->results->selectionModel()->hasSelection();

  for (QAction *action : context_actions_) {
    action->setEnabled(enable_context_actions);
  }

  context_menu_->popup(e->globalPos());

  return true;

}

void InternetSearchView::timerEvent(QTimerEvent *e) {

  QMap<int, DelayedSearch>::iterator it = delayed_searches_.find(e->timerId());
  if (it != delayed_searches_.end()) {
    SearchAsync(it.value().id_, it.value().query_, it.value().type_);
    delayed_searches_.erase(it);
    return;
  }

  QObject::timerEvent(e);

}

void InternetSearchView::StartSearch(const QString &query) {

  ui_->search->setText(query);
  TextEdited(query);

  // Swap models immediately
  swap_models_timer_->stop();
  SwapModels();

}

void InternetSearchView::TextEdited(const QString &text) {

  const QString trimmed(text.trimmed());

  search_error_ = false;
  cover_loader_tasks_.clear();

  // Add results to the back model, switch models after some delay.
  back_model_->Clear();
  current_model_ = back_model_;
  current_proxy_ = back_proxy_;
  swap_models_timer_->start();

  // Cancel the last search (if any) and start the new one.
  CancelSearch(last_search_id_);

  // If text query is empty, don't start a new search
  if (trimmed.isEmpty()) {
    last_search_id_ = -1;
    ui_->label_helptext->setText(tr("Enter search terms above to find music"));
    ui_->label_status->clear();
    ui_->progressbar->hide();
    ui_->progressbar->reset();
  }
  else {
    ui_->progressbar->reset();
    last_search_id_ = SearchAsync(trimmed, search_type_);
  }

}

void InternetSearchView::SwapModels() {

  cover_loader_tasks_.clear();

  std::swap(front_model_, back_model_);
  std::swap(front_proxy_, back_proxy_);

  ui_->results->setModel(front_proxy_);

  if (ui_->search->text().trimmed().isEmpty() || search_error_) {
    ui_->results_stack->setCurrentWidget(ui_->help_page);
  }
  else {
    ui_->results_stack->setCurrentWidget(ui_->results_page);
  }

}

QStringList InternetSearchView::TokenizeQuery(const QString &query) {

  QStringList tokens(query.split(QRegExp("\\s+")));

  for (QStringList::iterator it = tokens.begin(); it != tokens.end(); ++it) {
    (*it).remove('(');
    (*it).remove(')');
    (*it).remove('"');

    const int colon = (*it).indexOf(":");
    if (colon != -1) {
      (*it).remove(0, colon + 1);
    }
  }

  return tokens;

}

bool InternetSearchView::Matches(const QStringList &tokens, const QString &string) {

  for (const QString &token : tokens) {
    if (!string.contains(token, Qt::CaseInsensitive)) {
      return false;
    }
  }

  return true;

}

int InternetSearchView::SearchAsync(const QString &query, const SearchType type) {

  const int id = searches_next_id_++;

  int timer_id = startTimer(kDelayedSearchTimeoutMs);
  delayed_searches_[timer_id].id_ = id;
  delayed_searches_[timer_id].query_ = query;
  delayed_searches_[timer_id].type_ = type;

  return id;

}

void InternetSearchView::SearchAsync(const int id, const QString &query, const SearchType type) {

  const int service_id = service_->Search(query, type);
  pending_searches_[service_id] = PendingState(id, TokenizeQuery(query));

}

void InternetSearchView::SearchDone(const int service_id, const SongList &songs, const QString &error) {

  if (!pending_searches_.contains(service_id)) return;

  // Map back to the original id.
  const PendingState state = pending_searches_.take(service_id);
  const int search_id = state.orig_id_;

  if (songs.isEmpty()) {
    SearchError(search_id, error);
    return;
  }

  ResultList results;
  for (const Song &song : songs) {
    Result result;
    result.metadata_ = song;
    results << result;
  }

  // Load cached pixmaps into the results
  for (InternetSearchView::ResultList::iterator it = results.begin() ; it != results.end() ; ++it) {
    it->pixmap_cache_key_ = PixmapCacheKey(*it);
  }

  AddResults(search_id, results);

}

void InternetSearchView::CancelSearch(const int id) {

  QMap<int, DelayedSearch>::iterator it;
  for (it = delayed_searches_.begin(); it != delayed_searches_.end(); ++it) {
    if (it.value().id_ == id) {
      killTimer(it.key());
      delayed_searches_.erase(it);
      return;
    }
  }
  service_->CancelSearch();

}

void InternetSearchView::AddResults(const int id, const InternetSearchView::ResultList &results) {

  if (id != last_search_id_ || results.isEmpty()) return;

  ui_->label_status->clear();
  ui_->progressbar->reset();
  ui_->progressbar->hide();
  current_model_->AddResults(results);

}

void InternetSearchView::SearchError(const int id, const QString &error) {

  if (id != last_search_id_) return;

  search_error_ = true;
  ui_->label_helptext->setText(error);
  ui_->label_status->clear();
  ui_->progressbar->reset();
  ui_->progressbar->hide();
  ui_->results_stack->setCurrentWidget(ui_->help_page);

}

void InternetSearchView::UpdateStatus(const int service_id, const QString &text) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_[service_id];
  const int search_id = state.orig_id_;
  if (search_id != last_search_id_) return;
  ui_->progressbar->show();
  ui_->label_status->setText(text);

}

void InternetSearchView::ProgressSetMaximum(const int service_id, const int max) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_[service_id];
  const int search_id = state.orig_id_;
  if (search_id != last_search_id_) return;
  ui_->progressbar->setMaximum(max);

}

void InternetSearchView::UpdateProgress(const int service_id, const int progress) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_[service_id];
  const int search_id = state.orig_id_;
  if (search_id != last_search_id_) return;
  ui_->progressbar->setValue(progress);

}

MimeData *InternetSearchView::SelectedMimeData() {

  if (!ui_->results->selectionModel()) return nullptr;

  // Get all selected model indexes
  QModelIndexList indexes = ui_->results->selectionModel()->selectedRows();
  if (indexes.isEmpty()) {
    // There's nothing selected - take the first thing in the model that isn't a divider.
    for (int i = 0 ; i < front_proxy_->rowCount() ; ++i) {
      QModelIndex index = front_proxy_->index(i, 0);
      if (!index.data(CollectionModel::Role_IsDivider).toBool()) {
        indexes << index;
        ui_->results->setCurrentIndex(index);
        break;
      }
    }
  }

  // Still got nothing?  Give up.
  if (indexes.isEmpty()) {
    return nullptr;
  }

  // Get items for these indexes
  QList<QStandardItem*> items;
  for (const QModelIndex &index : indexes) {
    items << (front_model_->itemFromIndex(front_proxy_->mapToSource(index)));
  }

  // Get a MimeData for these items
  return front_model_->LoadTracks(front_model_->GetChildResults(items));

}

void InternetSearchView::AddSelectedToPlaylist() {
  emit AddToPlaylist(SelectedMimeData());
}

void InternetSearchView::LoadSelected() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;

  mimedata->clear_first_ = true;
  emit AddToPlaylist(mimedata);

}

void InternetSearchView::AddSelectedToPlaylistEnqueue() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;

  mimedata->enqueue_now_ = true;
  emit AddToPlaylist(mimedata);

}

void InternetSearchView::OpenSelectedInNewPlaylist() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;

  mimedata->open_in_new_playlist_ = true;
  emit AddToPlaylist(mimedata);

}

void InternetSearchView::SearchForThis() {
  StartSearch(ui_->results->selectionModel()->selectedRows().first().data().toString());
}

void InternetSearchView::FocusSearchField() {

  ui_->search->setFocus();
  ui_->search->selectAll();

}

void InternetSearchView::FocusOnFilter(QKeyEvent *e) {

  ui_->search->setFocus();
  QApplication::sendEvent(ui_->search, e);

}

void InternetSearchView::OpenSettingsDialog() {
  app_->OpenSettingsDialogAtPage(service_->settings_page());
}

void InternetSearchView::GroupByClicked(QAction *action) {

  if (action->property("group_by").isNull()) {
    if (!group_by_dialog_) {
      group_by_dialog_.reset(new GroupByDialog);
      connect(group_by_dialog_.get(), SIGNAL(Accepted(CollectionModel::Grouping)), SLOT(SetGroupBy(CollectionModel::Grouping)));
    }

    group_by_dialog_->show();
    return;
  }

  SetGroupBy(action->property("group_by").value<CollectionModel::Grouping>());

}

void InternetSearchView::SetGroupBy(const CollectionModel::Grouping &g) {

  // Clear requests: changing "group by" on the models will cause all the items to be removed/added again,
  // so all the QModelIndex here will become invalid. New requests will be created for those
  // songs when they will be displayed again anyway (when InternetSearchItemDelegate::paint will call LazyLoadAlbumCover)
  cover_loader_tasks_.clear();

  // Update the models
  front_model_->SetGroupBy(g, true);
  back_model_->SetGroupBy(g, false);

  // Save the setting
  QSettings s;
  s.beginGroup(service_->settings_group());
  s.setValue("search_group_by1", int(g.first));
  s.setValue("search_group_by2", int(g.second));
  s.setValue("search_group_by3", int(g.third));
  s.endGroup();

  // Make sure the correct action is checked.
  for (QAction *action : group_by_actions_->actions()) {
    if (action->property("group_by").isNull()) continue;

    if (g == action->property("group_by").value<CollectionModel::Grouping>()) {
      action->setChecked(true);
      return;
    }
  }

  // Check the advanced action
  group_by_actions_->actions().last()->setChecked(true);

}

void InternetSearchView::SearchArtistsClicked(const bool) {
  SetSearchType(InternetSearchView::SearchType_Artists);
}

void InternetSearchView::SearchAlbumsClicked(const bool) {
  SetSearchType(InternetSearchView::SearchType_Albums);
}

void InternetSearchView::SearchSongsClicked(const bool) {
  SetSearchType(InternetSearchView::SearchType_Songs);
}

void InternetSearchView::SetSearchType(const InternetSearchView::SearchType type) {

  search_type_ = type;

  QSettings s;
  s.beginGroup(service_->settings_group());
  s.setValue("type", int(search_type_));
  s.endGroup();

  TextEdited(ui_->search->text());

}

void InternetSearchView::AddArtists() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;
  if (const InternetSongMimeData *internet_song_data = qobject_cast<const InternetSongMimeData*>(mimedata)) {
    emit AddArtistsSignal(internet_song_data->songs);
  }

}

void InternetSearchView::AddAlbums() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;
  if (const InternetSongMimeData *internet_song_data = qobject_cast<const InternetSongMimeData*>(mimedata)) {
    emit AddAlbumsSignal(internet_song_data->songs);
  }

}

void InternetSearchView::AddSongs() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;
  if (const InternetSongMimeData *internet_song_data = qobject_cast<const InternetSongMimeData*>(mimedata)) {
    emit AddSongsSignal(internet_song_data->songs);
  }

}

QString InternetSearchView::PixmapCacheKey(const InternetSearchView::Result &result) const {

  if (result.metadata_.art_automatic_is_valid()) {
    return Song::TextForSource(service_->source()) + "/" + result.metadata_.art_automatic().toString();
  }
  else if (!result.metadata_.effective_albumartist().isEmpty() && !result.metadata_.album().isEmpty()) {
    return Song::TextForSource(service_->source()) + "/" + result.metadata_.effective_albumartist() + "/" + result.metadata_.album();
  }
  else {
    return Song::TextForSource(service_->source()) + "/" + result.metadata_.url().toString();
  }

}

bool InternetSearchView::FindCachedPixmap(const InternetSearchView::Result &result, QPixmap *pixmap) const {
  return pixmap_cache_.find(result.pixmap_cache_key_, pixmap);
}

void InternetSearchView::LazyLoadAlbumCover(const QModelIndex &proxy_index) {

  if (!proxy_index.isValid() || proxy_index.model() != front_proxy_) {
    return;
  }

  const QModelIndex source_index = front_proxy_->mapToSource(proxy_index);
  if (!source_index.isValid()) {
    return;
  }

  // Already loading art for this item?
  if (source_index.data(InternetSearchModel::Role_LazyLoadingArt).isValid()) {
    return;
  }

  // Should we even load art at all?
  if (!use_pretty_covers_) {
    return;
  }

  // Is this an album?
  const CollectionModel::GroupBy container_type = CollectionModel::GroupBy(proxy_index.data(CollectionModel::Role_ContainerType).toInt());
  if (!CollectionModel::IsAlbumGrouping(container_type)) return;

  // Mark the item as loading art

  QStandardItem *item_album = front_model_->itemFromIndex(source_index);
  if (!item_album) {
    return;
  }
  item_album->setData(true, InternetSearchModel::Role_LazyLoadingArt);

  // Walk down the item's children until we find a track
  QStandardItem *item_song = item_album;
  while (item_song->rowCount()) {
    item_song = item_song->child(0);
  }

  // Get the track's Result
  const InternetSearchView::Result result = item_song->data(InternetSearchModel::Role_Result).value<InternetSearchView::Result>();

  QPixmap cached_pixmap;
  if (pixmap_cache_.find(result.pixmap_cache_key_, &cached_pixmap)) {
    item_album->setData(cached_pixmap, Qt::DecorationRole);
  }
  else {
    quint64 loader_id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, result.metadata_);
    cover_loader_tasks_[loader_id] = qMakePair(source_index, result.pixmap_cache_key_);
  }

}

void InternetSearchView::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &albumcover_result) {

  if (!cover_loader_tasks_.contains(id)) {
    return;
  }

  QPair<QModelIndex, QString> cover_loader_task = cover_loader_tasks_.take(id);
  QModelIndex idx = cover_loader_task.first;
  QString key = cover_loader_task.second;

  QPixmap pixmap = QPixmap::fromImage(albumcover_result.image_scaled);
  if (!pixmap.isNull()) {
    pixmap_cache_.insert(key, pixmap);
  }

  if (idx.isValid()) {
    QStandardItem *item = front_model_->itemFromIndex(idx);
    if (item) {
      item->setData(albumcover_result.image_scaled, Qt::DecorationRole);
    }
  }

}
