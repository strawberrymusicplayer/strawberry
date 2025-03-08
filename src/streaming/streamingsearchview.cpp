/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QPixmap>
#include <QPixmapCache>
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

#include "core/song.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "core/mimedata.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionmodel.h"
#include "collection/groupbydialog.h"
#include "collection/savedgroupingmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"
#include "streamsongmimedata.h"
#include "streamingservice.h"
#include "streamingsearchitemdelegate.h"
#include "streamingsearchmodel.h"
#include "streamingsearchsortmodel.h"
#include "streamingsearchview.h"
#include "ui_streamingsearchview.h"
#include "constants/appearancesettings.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kSwapModelsTimeoutMsec = 250;
constexpr int kDelayedSearchTimeoutMs = 200;
constexpr int kArtHeight = 32;
}  // namespace

StreamingSearchView::StreamingSearchView(QWidget *parent)
    : QWidget(parent),
      service_(nullptr),
      ui_(new Ui_StreamingSearchView),
      context_menu_(nullptr),
      group_by_actions_(nullptr),
      front_model_(nullptr),
      back_model_(nullptr),
      current_model_(nullptr),
      front_proxy_(new StreamingSearchSortModel(this)),
      back_proxy_(new StreamingSearchSortModel(this)),
      current_proxy_(front_proxy_),
      swap_models_timer_(new QTimer(this)),
      use_pretty_covers_(true),
      search_type_(StreamingService::SearchType::Artists),
      search_error_(false),
      last_search_id_(0),
      searches_next_id_(1) {

  ui_->setupUi(this);

  ui_->search->installEventFilter(this);
  ui_->results_stack->installEventFilter(this);

  ui_->settings->setIcon(IconLoader::Load(u"configure"_s));

  // Set the appearance of the results list
  ui_->results->setItemDelegate(new StreamingSearchItemDelegate(this));
  ui_->results->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui_->results->setStyleSheet(u"QTreeView::item{padding-top:1px;}"_s);

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

}

StreamingSearchView::~StreamingSearchView() { delete ui_; }

void StreamingSearchView::Init(const StreamingServicePtr service, const SharedPtr<AlbumCoverLoader> albumcover_loader) {

  service_ = service;
  albumcover_loader_ = albumcover_loader;

  front_model_ = new StreamingSearchModel(service, this);
  back_model_ = new StreamingSearchModel(service, this);

  front_proxy_ = new StreamingSearchSortModel(this);
  back_proxy_ = new StreamingSearchSortModel(this);

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
  group_by_actions_ = CollectionFilterWidget::CreateGroupByActions(SavedGroupingManager::GetSavedGroupingsSettingsGroup(service_->settings_group()), this);
  QMenu *settings_menu = new QMenu(this);
  settings_menu->addActions(group_by_actions_->actions());
  settings_menu->addSeparator();
  settings_menu->addAction(IconLoader::Load(u"configure"_s), tr("Configure %1...").arg(Song::DescriptionForSource(service_->source())), this, &StreamingSearchView::Configure);
  ui_->settings->setMenu(settings_menu);

  swap_models_timer_->setSingleShot(true);
  swap_models_timer_->setInterval(kSwapModelsTimeoutMsec);
  QObject::connect(swap_models_timer_, &QTimer::timeout, this, &StreamingSearchView::SwapModels);

  QObject::connect(ui_->radiobutton_search_artists, &QRadioButton::clicked, this, &StreamingSearchView::SearchArtistsClicked);
  QObject::connect(ui_->radiobutton_search_albums, &QRadioButton::clicked, this, &StreamingSearchView::SearchAlbumsClicked);
  QObject::connect(ui_->radiobutton_search_songs, &QRadioButton::clicked, this, &StreamingSearchView::SearchSongsClicked);
  QObject::connect(group_by_actions_, &QActionGroup::triggered, this, &StreamingSearchView::GroupByClicked);
  QObject::connect(group_by_actions_, &QActionGroup::triggered, this, &StreamingSearchView::GroupByClicked);

  QObject::connect(ui_->search, &SearchField::textChanged, this, &StreamingSearchView::TextEdited);
  QObject::connect(ui_->results, &AutoExpandingTreeView::AddToPlaylistSignal, this, &StreamingSearchView::AddToPlaylist);
  QObject::connect(ui_->results, &AutoExpandingTreeView::FocusOnFilterSignal, this, &StreamingSearchView::FocusOnFilter);

  QObject::connect(&*service_, &StreamingService::SearchUpdateStatus, this, &StreamingSearchView::UpdateStatus);
  QObject::connect(&*service_, &StreamingService::SearchProgressSetMaximum, this, &StreamingSearchView::ProgressSetMaximum);
  QObject::connect(&*service_, &StreamingService::SearchUpdateProgress, this, &StreamingSearchView::UpdateProgress);
  QObject::connect(&*service_, &StreamingService::SearchResults, this, &StreamingSearchView::SearchDone);

  QObject::connect(&*albumcover_loader_, &AlbumCoverLoader::AlbumCoverLoaded, this, &StreamingSearchView::AlbumCoverLoaded);

  QObject::connect(ui_->settings, &QToolButton::clicked, ui_->settings, &QToolButton::showMenu);

  ReloadSettings();

}

void StreamingSearchView::ReloadSettings() {

  Settings s;

  // Collection settings

  s.beginGroup(service_->settings_group());
  use_pretty_covers_ = s.value("pretty_covers", true).toBool();
  front_model_->set_use_pretty_covers(use_pretty_covers_);
  back_model_->set_use_pretty_covers(use_pretty_covers_);

  // Streaming search settings

  search_type_ = static_cast<StreamingService::SearchType>(s.value("type", static_cast<int>(StreamingService::SearchType::Artists)).toInt());
  switch (search_type_) {
    case StreamingService::SearchType::Artists:
      ui_->radiobutton_search_artists->setChecked(true);
      break;
    case StreamingService::SearchType::Albums:
      ui_->radiobutton_search_albums->setChecked(true);
      break;
    case StreamingService::SearchType::Songs:
      ui_->radiobutton_search_songs->setChecked(true);
      break;
  }

  int group_by_version = s.value("search_group_by_version", 0).toInt();
  if (group_by_version == 1 && s.contains("search_group_by1") && s.contains("search_group_by2") && s.contains("search_group_by3")) {
    SetGroupBy(CollectionModel::Grouping(
        static_cast<CollectionModel::GroupBy>(s.value("search_group_by1", static_cast<int>(CollectionModel::GroupBy::AlbumArtist)).toInt()),
        static_cast<CollectionModel::GroupBy>(s.value("search_group_by2", static_cast<int>(CollectionModel::GroupBy::AlbumDisc)).toInt()),
        static_cast<CollectionModel::GroupBy>(s.value("search_group_by3", static_cast<int>(CollectionModel::GroupBy::None)).toInt())));
  }
  else {
    SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::AlbumDisc, CollectionModel::GroupBy::None));
  }
  s.endGroup();

  s.beginGroup(AppearanceSettings::kSettingsGroup);
  int iconsize = s.value(AppearanceSettings::kIconSizeConfigureButtons, 20).toInt();
  s.endGroup();

  ui_->settings->setIconSize(QSize(iconsize, iconsize));
  ui_->search->setIconSize(iconsize);

}

void StreamingSearchView::showEvent(QShowEvent *e) {

  QWidget::showEvent(e);

#ifndef Q_OS_MACOS
  FocusSearchField();
#endif

}

bool StreamingSearchView::eventFilter(QObject *object, QEvent *e) {

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

bool StreamingSearchView::SearchKeyEvent(QKeyEvent *e) {

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

bool StreamingSearchView::ResultsContextMenuEvent(QContextMenuEvent *e) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);
    context_actions_ << context_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this, &StreamingSearchView::AddSelectedToPlaylist);
    context_actions_ << context_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Replace current playlist"), this, &StreamingSearchView::LoadSelected);
    context_actions_ << context_menu_->addAction(IconLoader::Load(u"document-new"_s), tr("Open in new playlist"), this, &StreamingSearchView::OpenSelectedInNewPlaylist);

    context_menu_->addSeparator();
    context_actions_ << context_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Queue track"), this, &StreamingSearchView::AddSelectedToPlaylistEnqueue);

    context_menu_->addSeparator();

    if (service_->artists_collection_model() || service_->albums_collection_model() || service_->songs_collection_model()) {
      if (service_->artists_collection_model()) {
        context_actions_ << context_menu_->addAction(IconLoader::Load(u"folder-new"_s), tr("Add to artists"), this, &StreamingSearchView::AddArtists);
      }
      if (service_->albums_collection_model()) {
        context_actions_ << context_menu_->addAction(IconLoader::Load(u"folder-new"_s), tr("Add to albums"), this, &StreamingSearchView::AddAlbums);
      }
      if (service_->songs_collection_model()) {
        context_actions_ << context_menu_->addAction(IconLoader::Load(u"folder-new"_s), tr("Add to songs"), this, &StreamingSearchView::AddSongs);
      }
      context_menu_->addSeparator();
    }

    if (ui_->results->selectionModel() && ui_->results->selectionModel()->selectedRows().length() == 1) {
      context_actions_ << context_menu_->addAction(IconLoader::Load(u"search"_s), tr("Search for this"), this, &StreamingSearchView::SearchForThis);
    }

    context_menu_->addSeparator();
    context_menu_->addMenu(tr("Group by"))->addActions(group_by_actions_->actions());

    context_menu_->addAction(IconLoader::Load(u"configure"_s), tr("Configure %1...").arg(Song::TextForSource(service_->source())), this, &StreamingSearchView::Configure);

  }

  const bool enable_context_actions = ui_->results->selectionModel() && ui_->results->selectionModel()->hasSelection();

  for (QAction *action : std::as_const(context_actions_)) {
    action->setEnabled(enable_context_actions);
  }

  context_menu_->popup(e->globalPos());

  return true;

}

void StreamingSearchView::timerEvent(QTimerEvent *e) {

  QMap<int, DelayedSearch>::const_iterator it = delayed_searches_.constFind(e->timerId());
  if (it != delayed_searches_.constEnd()) {
    SearchAsync(it.value().id_, it.value().query_, it.value().type_);
    delayed_searches_.erase(it);
    return;
  }

  QObject::timerEvent(e);

}

void StreamingSearchView::StartSearch(const QString &query) {

  ui_->search->setText(query);
  TextEdited(query);

  // Swap models immediately
  swap_models_timer_->stop();
  SwapModels();

}

void StreamingSearchView::TextEdited(const QString &text) {

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

void StreamingSearchView::SwapModels() {

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

QStringList StreamingSearchView::TokenizeQuery(const QString &query) {

  static const QRegularExpression regex_whitespaces(u"\\s+"_s);
  QStringList tokens = query.split(regex_whitespaces);

  for (QStringList::iterator it = tokens.begin(); it != tokens.end(); ++it) {
    (*it).remove(u'(');
    (*it).remove(u')');
    (*it).remove(u'"');

    const qint64 colon = (*it).indexOf(u':');
    if (colon != -1) {
      (*it).remove(0, colon + 1);
    }
  }

  return tokens;

}

bool StreamingSearchView::Matches(const QStringList &tokens, const QString &string) {

  for (const QString &token : tokens) {
    if (!string.contains(token, Qt::CaseInsensitive)) {
      return false;
    }
  }

  return true;

}

int StreamingSearchView::SearchAsync(const QString &query, const StreamingService::SearchType type) {

  const int id = searches_next_id_++;

  int timer_id = startTimer(kDelayedSearchTimeoutMs);
  delayed_searches_[timer_id].id_ = id;
  delayed_searches_[timer_id].query_ = query;
  delayed_searches_[timer_id].type_ = type;

  return id;

}

void StreamingSearchView::SearchAsync(const int id, const QString &query, const StreamingService::SearchType type) {

  const int service_id = service_->Search(query, type);
  pending_searches_[service_id] = PendingState(id, TokenizeQuery(query));

}

void StreamingSearchView::SearchDone(const int service_id, const SongMap &songs, const QString &error) {

  if (!pending_searches_.contains(service_id)) return;

  // Map back to the original id.
  const PendingState state = pending_searches_.take(service_id);
  const int search_id = state.orig_id_;

  if (songs.isEmpty()) {
    SearchError(search_id, error);
    return;
  }

  ResultList results;
  results.reserve(songs.count());
  for (const Song &song : songs) {
    Result result;
    result.metadata_ = song;
    results << result;
  }

  // Load cached pixmaps into the results
  for (StreamingSearchView::ResultList::iterator it = results.begin(); it != results.end(); ++it) {
    it->pixmap_cache_key_ = PixmapCacheKey(*it);
  }

  AddResults(search_id, results);

}

void StreamingSearchView::CancelSearch(const int id) {

  for (QMap<int, DelayedSearch>::const_iterator it = delayed_searches_.constBegin(); it != delayed_searches_.constEnd(); ++it) {
    if (it.value().id_ == id) {
      killTimer(it.key());
      delayed_searches_.erase(it);
      return;
    }
  }
  service_->CancelSearch();

}

void StreamingSearchView::AddResults(const int id, const StreamingSearchView::ResultList &results) {

  if (id != last_search_id_ || results.isEmpty()) return;

  ui_->label_status->clear();
  ui_->progressbar->reset();
  ui_->progressbar->hide();
  current_model_->AddResults(results);

}

void StreamingSearchView::SearchError(const int id, const QString &error) {

  if (id != last_search_id_) return;

  search_error_ = true;
  ui_->label_helptext->setText(error);
  ui_->label_status->clear();
  ui_->progressbar->reset();
  ui_->progressbar->hide();
  ui_->results_stack->setCurrentWidget(ui_->help_page);

}

void StreamingSearchView::UpdateStatus(const int service_id, const QString &text) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_.value(service_id);
  const int search_id = state.orig_id_;
  if (search_id != last_search_id_) return;
  ui_->progressbar->show();
  ui_->label_status->setText(text);

}

void StreamingSearchView::ProgressSetMaximum(const int service_id, const int max) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_.value(service_id);
  const int search_id = state.orig_id_;
  if (search_id != last_search_id_) return;
  ui_->progressbar->setMaximum(max);

}

void StreamingSearchView::UpdateProgress(const int service_id, const int progress) {

  if (!pending_searches_.contains(service_id)) return;
  const PendingState state = pending_searches_.value(service_id);
  const int search_id = state.orig_id_;
  if (search_id != last_search_id_) return;
  ui_->progressbar->setValue(progress);

}

MimeData *StreamingSearchView::SelectedMimeData() {

  if (!ui_->results->selectionModel()) return nullptr;

  // Get all selected model indexes
  QModelIndexList indexes = ui_->results->selectionModel()->selectedRows();
  if (indexes.isEmpty()) {
    // There's nothing selected - take the first thing in the model that isn't a divider.
    for (int i = 0; i < front_proxy_->rowCount(); ++i) {
      QModelIndex idx = front_proxy_->index(i, 0);
      if (!idx.data(CollectionModel::Role_IsDivider).toBool()) {
        indexes << idx;  // clazy:exclude=reserve-candidates
        ui_->results->setCurrentIndex(idx);
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
  for (const QModelIndex &idx : std::as_const(indexes)) {
    items << (front_model_->itemFromIndex(front_proxy_->mapToSource(idx)));  // clazy:exclude=reserve-candidates
  }

  // Get a MimeData for these items
  return front_model_->LoadTracks(front_model_->GetChildResults(items));

}

void StreamingSearchView::AddSelectedToPlaylist() {
  Q_EMIT AddToPlaylist(SelectedMimeData());
}

void StreamingSearchView::LoadSelected() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;

  mimedata->clear_first_ = true;
  Q_EMIT AddToPlaylist(mimedata);

}

void StreamingSearchView::AddSelectedToPlaylistEnqueue() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;

  mimedata->enqueue_now_ = true;
  Q_EMIT AddToPlaylist(mimedata);

}

void StreamingSearchView::OpenSelectedInNewPlaylist() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;

  mimedata->open_in_new_playlist_ = true;
  Q_EMIT AddToPlaylist(mimedata);

}

void StreamingSearchView::SearchForThis() {
  StartSearch(ui_->results->selectionModel()->selectedRows().first().data().toString());
}

bool StreamingSearchView::SearchFieldHasFocus() const {

  return ui_->search->hasFocus();

}

void StreamingSearchView::FocusSearchField() {

  ui_->search->setFocus();
  ui_->search->selectAll();

}

void StreamingSearchView::FocusOnFilter(QKeyEvent *e) {

  ui_->search->setFocus();
  QApplication::sendEvent(ui_->search, e);

}

void StreamingSearchView::Configure() {
  Q_EMIT OpenSettingsDialog(service_->source());
}

void StreamingSearchView::GroupByClicked(QAction *action) {

  if (action->property("group_by").isNull()) {
    if (!group_by_dialog_) {
      group_by_dialog_ = make_unique<GroupByDialog>();
      QObject::connect(&*group_by_dialog_, &GroupByDialog::Accepted, this, &StreamingSearchView::SetGroupBy);
    }

    group_by_dialog_->show();
    return;
  }

  SetGroupBy(action->property("group_by").value<CollectionModel::Grouping>());

}

void StreamingSearchView::SetGroupBy(const CollectionModel::Grouping g) {

  // Clear requests: changing "group by" on the models will cause all the items to be removed/added again,
  // so all the QModelIndex here will become invalid. New requests will be created for those
  // songs when they will be displayed again anyway (when StreamingSearchItemDelegate::paint will call LazyLoadAlbumCover)
  cover_loader_tasks_.clear();

  // Update the models
  front_model_->SetGroupBy(g, true);
  back_model_->SetGroupBy(g, false);

  // Save the setting
  Settings s;
  s.beginGroup(service_->settings_group());
  s.setValue("search_group_by_version", 1);
  s.setValue("search_group_by1", static_cast<int>(g.first));
  s.setValue("search_group_by2", static_cast<int>(g.second));
  s.setValue("search_group_by3", static_cast<int>(g.third));
  s.endGroup();

  // Make sure the correct action is checked.
  const QList<QAction*> actions = group_by_actions_->actions();
  for (QAction *action : actions) {
    if (action->property("group_by").isNull()) continue;

    if (g == action->property("group_by").value<CollectionModel::Grouping>()) {
      action->setChecked(true);
      return;
    }
  }

  // Check the advanced action
  actions.last()->setChecked(true);

}

void StreamingSearchView::SearchArtistsClicked(const bool checked) {
  Q_UNUSED(checked)
  SetSearchType(StreamingService::SearchType::Artists);
}

void StreamingSearchView::SearchAlbumsClicked(const bool checked) {
  Q_UNUSED(checked)
  SetSearchType(StreamingService::SearchType::Albums);
}

void StreamingSearchView::SearchSongsClicked(const bool checked) {
  Q_UNUSED(checked)
  SetSearchType(StreamingService::SearchType::Songs);
}

void StreamingSearchView::SetSearchType(const StreamingService::SearchType type) {

  search_type_ = type;

  Settings s;
  s.beginGroup(service_->settings_group());
  s.setValue("type", static_cast<int>(search_type_));
  s.endGroup();

  TextEdited(ui_->search->text());

}

void StreamingSearchView::AddArtists() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;
  if (const StreamSongMimeData *streaming_song_data = qobject_cast<const StreamSongMimeData*>(mimedata)) {
    Q_EMIT AddArtistsSignal(streaming_song_data->songs);
  }

}

void StreamingSearchView::AddAlbums() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;
  if (const StreamSongMimeData *streaming_song_data = qobject_cast<const StreamSongMimeData*>(mimedata)) {
    Q_EMIT AddAlbumsSignal(streaming_song_data->songs);
  }

}

void StreamingSearchView::AddSongs() {

  MimeData *mimedata = SelectedMimeData();
  if (!mimedata) return;
  if (const StreamSongMimeData *streaming_song_data = qobject_cast<const StreamSongMimeData*>(mimedata)) {
    Q_EMIT AddSongsSignal(streaming_song_data->songs);
  }

}

QString StreamingSearchView::PixmapCacheKey(const StreamingSearchView::Result &result) const {

  if (result.metadata_.art_automatic_is_valid()) {
    return Song::TextForSource(service_->source()) + QLatin1Char('/') + result.metadata_.art_automatic().toString();
  }
  if (!result.metadata_.effective_albumartist().isEmpty() && !result.metadata_.album().isEmpty()) {
    return Song::TextForSource(service_->source()) + QLatin1Char('/') + result.metadata_.effective_albumartist() + QLatin1Char('/') + result.metadata_.album();
  }

  return Song::TextForSource(service_->source()) + QLatin1Char('/') + result.metadata_.url().toString();

}

bool StreamingSearchView::FindCachedPixmap(const StreamingSearchView::Result &result, QPixmap *pixmap) const {
  return QPixmapCache::find(result.pixmap_cache_key_, pixmap);
}

void StreamingSearchView::LazyLoadAlbumCover(const QModelIndex &proxy_index) {

  if (!proxy_index.isValid() || proxy_index.model() != front_proxy_) {
    return;
  }

  const QModelIndex source_index = front_proxy_->mapToSource(proxy_index);
  if (!source_index.isValid()) {
    return;
  }

  // Already loading art for this item?
  if (source_index.data(StreamingSearchModel::Role_LazyLoadingArt).isValid()) {
    return;
  }

  // Should we even load art at all?
  if (!use_pretty_covers_) {
    return;
  }

  // Is this an album?
  const CollectionModel::GroupBy container_type = static_cast<CollectionModel::GroupBy>(proxy_index.data(CollectionModel::Role_ContainerType).toInt());
  if (!CollectionModel::IsAlbumGroupBy(container_type)) return;

  // Mark the item as loading art

  QStandardItem *item_album = front_model_->itemFromIndex(source_index);
  if (!item_album) {
    return;
  }
  item_album->setData(true, StreamingSearchModel::Role_LazyLoadingArt);

  // Walk down the item's children until we find a track
  QStandardItem *item_song = item_album;
  while (item_song->rowCount() > 0) {
    item_song = item_song->child(0);
  }

  // Get the track's Result
  const StreamingSearchView::Result result = item_song->data(StreamingSearchModel::Role_Result).value<StreamingSearchView::Result>();

  QPixmap cached_pixmap;
  if (QPixmapCache::find(result.pixmap_cache_key_, &cached_pixmap)) {
    item_album->setData(cached_pixmap, Qt::DecorationRole);
  }
  else {
    AlbumCoverLoaderOptions cover_loader_options(AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
    cover_loader_options.desired_scaled_size = QSize(kArtHeight, kArtHeight);
    quint64 loader_id = albumcover_loader_->LoadImageAsync(cover_loader_options, result.metadata_);
    cover_loader_tasks_[loader_id] = qMakePair(source_index, result.pixmap_cache_key_);
  }

}

void StreamingSearchView::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &albumcover_result) {

  if (!cover_loader_tasks_.contains(id)) return;

  QPair<QModelIndex, QString> cover_loader_task = cover_loader_tasks_.take(id);
  QModelIndex idx = cover_loader_task.first;
  QString key = cover_loader_task.second;

  if (albumcover_result.success && !albumcover_result.image_scaled.isNull()) {
    QPixmap pixmap = QPixmap::fromImage(albumcover_result.image_scaled);
    if (!pixmap.isNull()) {
      QPixmapCache::insert(key, pixmap);
    }
    if (idx.isValid()) {
      QStandardItem *item = front_model_->itemFromIndex(idx);
      if (item) {
        item->setData(albumcover_result.image_scaled, Qt::DecorationRole);
      }
    }
  }

}
