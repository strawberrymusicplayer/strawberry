/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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
#include <QTimer>
#include <QList>
#include <QString>
#include <QPixmap>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QStandardItem>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QtEvents>

#include "core/application.h"
#include "core/logging.h"
#include "core/mimedata.h"
#include "core/iconloader.h"
#include "internet/internetsongmimedata.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionmodel.h"
#include "collection/groupbydialog.h"
#include "playlist/songmimedata.h"
#include "internetsearch.h"
#include "internetsearchitemdelegate.h"
#include "internetsearchmodel.h"
#include "internetsearchsortmodel.h"
#include "internetsearchview.h"
#include "ui_internetsearchview.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::swap;

const int InternetSearchView::kSwapModelsTimeoutMsec = 250;

InternetSearchView::InternetSearchView(QWidget *parent)
    : QWidget(parent),
      app_(nullptr),
      engine_(nullptr),
      ui_(new Ui_InternetSearchView),
      context_menu_(nullptr),
      last_search_id_(0),
      front_model_(nullptr),
      back_model_(nullptr),
      current_model_(nullptr),
      front_proxy_(new InternetSearchSortModel(this)),
      back_proxy_(new InternetSearchSortModel(this)),
      current_proxy_(front_proxy_),
      swap_models_timer_(new QTimer(this)),
      error_(false)
      {

  ui_->setupUi(this);

  ui_->progressbar->hide();
  ui_->progressbar->reset();

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

}

InternetSearchView::~InternetSearchView() { delete ui_; }

void InternetSearchView::Init(Application *app, InternetSearch *engine, QString settings_group, SettingsDialog::Page settings_page, const bool artists, const bool albums, const bool songs) {

  app_ = app;
  engine_ = engine;
  settings_group_ = settings_group;
  settings_page_ = settings_page;
  artists_ = artists;
  albums_ = albums;
  songs_ = songs;

  front_model_ = new InternetSearchModel(engine_, this);
  back_model_ = new InternetSearchModel(engine_, this);

  front_proxy_ = new InternetSearchSortModel(this);
  back_proxy_ = new InternetSearchSortModel(this);

  front_model_->set_proxy(front_proxy_);
  back_model_->set_proxy(back_proxy_);

  current_model_ = front_model_;
  current_proxy_ = front_proxy_;

  // Must be a queued connection to ensure the InternetSearch handles it first.
  connect(app_, SIGNAL(SettingsChanged()), SLOT(ReloadSettings()), Qt::QueuedConnection);
  
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
  settings_menu->addAction(IconLoader::Load("configure"), tr("Configure %1...").arg(Song::TextForSource(engine->source())), this, SLOT(OpenSettingsDialog()));
  ui_->settings->setMenu(settings_menu);

  swap_models_timer_->setSingleShot(true);
  swap_models_timer_->setInterval(kSwapModelsTimeoutMsec);
  connect(swap_models_timer_, SIGNAL(timeout()), SLOT(SwapModels()));

  connect(ui_->radiobutton_search_artists, SIGNAL(clicked(bool)), SLOT(SearchArtistsClicked(bool)));
  connect(ui_->radiobutton_search_albums, SIGNAL(clicked(bool)), SLOT(SearchAlbumsClicked(bool)));
  connect(ui_->radiobutton_search_songs, SIGNAL(clicked(bool)), SLOT(SearchSongsClicked(bool)));
  connect(group_by_actions_, SIGNAL(triggered(QAction*)), SLOT(GroupByClicked(QAction*)));

  connect(ui_->search, SIGNAL(textChanged(QString)), SLOT(TextEdited(QString)));
  connect(ui_->results, SIGNAL(AddToPlaylistSignal(QMimeData*)), SIGNAL(AddToPlaylist(QMimeData*)));
  connect(ui_->results, SIGNAL(FocusOnFilterSignal(QKeyEvent*)), SLOT(FocusOnFilter(QKeyEvent*)));

  // These have to be queued connections because they may get emitted before our call to Search() (or whatever) returns and we add the ID to the map.

  connect(engine_, SIGNAL(UpdateStatus(QString)), SLOT(UpdateStatus(QString)));
  connect(engine_, SIGNAL(ProgressSetMaximum(int)), SLOT(ProgressSetMaximum(int)), Qt::QueuedConnection);
  connect(engine_, SIGNAL(UpdateProgress(int)), SLOT(UpdateProgress(int)), Qt::QueuedConnection);

  connect(engine_, SIGNAL(AddResults(int, InternetSearch::ResultList)), SLOT(AddResults(int, InternetSearch::ResultList)), Qt::QueuedConnection);
  connect(engine_, SIGNAL(SearchError(int, QString)), SLOT(SearchError(int, QString)), Qt::QueuedConnection);
  connect(engine_, SIGNAL(ArtLoaded(int, QPixmap)), SLOT(ArtLoaded(int, QPixmap)), Qt::QueuedConnection);

  ReloadSettings();

}

void InternetSearchView::ReloadSettings() {

  QSettings s;

  // Collection settings

  s.beginGroup(settings_group_);
  const bool pretty = s.value("pretty_covers", true).toBool();
  front_model_->set_use_pretty_covers(pretty);
  back_model_->set_use_pretty_covers(pretty);

  // Internet search settings

  search_type_ = InternetSearch::SearchType(s.value("type", int(InternetSearch::SearchType_Artists)).toInt());
  switch (search_type_) {
    case InternetSearch::SearchType_Artists:
      ui_->radiobutton_search_artists->setChecked(true);
      break;
    case InternetSearch::SearchType_Albums:
      ui_->radiobutton_search_albums->setChecked(true);
      break;
    case InternetSearch::SearchType_Songs:
      ui_->radiobutton_search_songs->setChecked(true);
      break;
  }

  SetGroupBy(CollectionModel::Grouping(
      CollectionModel::GroupBy(s.value("search_group_by1", int(CollectionModel::GroupBy_AlbumArtist)).toInt()),
      CollectionModel::GroupBy(s.value("search_group_by2", int(CollectionModel::GroupBy_Album)).toInt()),
      CollectionModel::GroupBy(s.value("search_group_by3", int(CollectionModel::GroupBy_None)).toInt())));
  s.endGroup();

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

  error_ = false;

  // Add results to the back model, switch models after some delay.
  back_model_->Clear();
  current_model_ = back_model_;
  current_proxy_ = back_proxy_;
  swap_models_timer_->start();

  // Cancel the last search (if any) and start the new one.
  engine_->CancelSearch(last_search_id_);
  // If text query is empty, don't start a new search
  if (trimmed.isEmpty()) {
    last_search_id_ = -1;
    ui_->label_helptext->setText("Enter search terms above to find music");
    ui_->label_status->clear();
    ui_->progressbar->hide();
    ui_->progressbar->reset();
  }
  else {
    ui_->progressbar->reset();
    last_search_id_ = engine_->SearchAsync(trimmed, search_type_);
  }

}

void InternetSearchView::AddResults(int id, const InternetSearch::ResultList &results) {

  if (id != last_search_id_) return;
  if (results.isEmpty()) return;
  ui_->label_status->clear();
  ui_->progressbar->reset();
  ui_->progressbar->hide();
  current_model_->AddResults(results);

}

void InternetSearchView::SearchError(const int id, const QString error) {

  error_ = true;
  ui_->label_helptext->setText(error);
  ui_->label_status->clear();
  ui_->progressbar->reset();
  ui_->progressbar->hide();
  ui_->results_stack->setCurrentWidget(ui_->help_page);

}

void InternetSearchView::SwapModels() {

  art_requests_.clear();

  std::swap(front_model_, back_model_);
  std::swap(front_proxy_, back_proxy_);

  ui_->results->setModel(front_proxy_);

  if (ui_->search->text().trimmed().isEmpty() || error_) {
    ui_->results_stack->setCurrentWidget(ui_->help_page);
  }
  else {
    ui_->results_stack->setCurrentWidget(ui_->results_page);
  }

}

void InternetSearchView::LazyLoadArt(const QModelIndex &proxy_index) {

  if (!proxy_index.isValid() || proxy_index.model() != front_proxy_) {
    return;
  }

  // Already loading art for this item?
  if (proxy_index.data(InternetSearchModel::Role_LazyLoadingArt).isValid()) {
    return;
  }

  // Should we even load art at all?
  if (!app_->collection_model()->use_pretty_covers()) {
    return;
  }

  // Is this an album?
  const CollectionModel::GroupBy container_type = CollectionModel::GroupBy(proxy_index.data(CollectionModel::Role_ContainerType).toInt());
  if (container_type != CollectionModel::GroupBy_Album &&
      container_type != CollectionModel::GroupBy_YearAlbum &&
      container_type != CollectionModel::GroupBy_OriginalYearAlbum) {
    return;
  }

  // Mark the item as loading art
  const QModelIndex source_index = front_proxy_->mapToSource(proxy_index);
  QStandardItem *item = front_model_->itemFromIndex(source_index);
  item->setData(true, InternetSearchModel::Role_LazyLoadingArt);

  // Walk down the item's children until we find a track
  while (item->rowCount()) {
    item = item->child(0);
  }

  // Get the track's Result
  const InternetSearch::Result result = item->data(InternetSearchModel::Role_Result).value<InternetSearch::Result>();

  // Load the art.
  int id = engine_->LoadArtAsync(result);
  art_requests_[id] = source_index;

}

void InternetSearchView::ArtLoaded(int id, const QPixmap &pixmap) {

  if (!art_requests_.contains(id)) return;
  QModelIndex index = art_requests_.take(id);

  if (!pixmap.isNull()) {
    front_model_->itemFromIndex(index)->setData(pixmap, Qt::DecorationRole);
  }

}

MimeData *InternetSearchView::SelectedMimeData() {

  if (!ui_->results->selectionModel()) return nullptr;

  // Get all selected model indexes
  QModelIndexList indexes = ui_->results->selectionModel()->selectedRows();
  if (indexes.isEmpty()) {
    // There's nothing selected - take the first thing in the model that isn't a divider.
    for (int i = 0; i < front_proxy_->rowCount(); ++i) {
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
  return engine_->LoadTracks(front_model_->GetChildResults(items));

}

bool InternetSearchView::eventFilter(QObject *object, QEvent *event) {

  if (object == ui_->search && event->type() == QEvent::KeyRelease) {
    if (SearchKeyEvent(static_cast<QKeyEvent*>(event))) {
      return true;
    }
  }
  else if (object == ui_->results_stack && event->type() == QEvent::ContextMenu) {
    if (ResultsContextMenuEvent(static_cast<QContextMenuEvent*>(event))) {
      return true;
    }
  }

  return QWidget::eventFilter(object, event);

}

bool InternetSearchView::SearchKeyEvent(QKeyEvent *event) {

  switch (event->key()) {
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

  event->accept();
  return true;

}

bool InternetSearchView::ResultsContextMenuEvent(QContextMenuEvent *event) {

  context_menu_ = new QMenu(this);
  context_actions_ << context_menu_->addAction( IconLoader::Load("media-play"), tr("Append to current playlist"), this, SLOT(AddSelectedToPlaylist()));
  context_actions_ << context_menu_->addAction( IconLoader::Load("media-play"), tr("Replace current playlist"), this, SLOT(LoadSelected()));
  context_actions_ << context_menu_->addAction( IconLoader::Load("document-new"), tr("Open in new playlist"), this, SLOT(OpenSelectedInNewPlaylist()));

  context_menu_->addSeparator();
  context_actions_ << context_menu_->addAction(IconLoader::Load("go-next"), tr("Queue track"), this, SLOT(AddSelectedToPlaylistEnqueue()));

  context_menu_->addSeparator();

  if (artists_ || albums_ || songs_) {
    if (artists_)
      context_actions_ << context_menu_->addAction(IconLoader::Load("folder-new"), tr("Add to artists"), this, SLOT(AddArtists()));
    if (albums_)
      context_actions_ << context_menu_->addAction(IconLoader::Load("folder-new"), tr("Add to albums"), this, SLOT(AddAlbums()));
    if (songs_)
      context_actions_ << context_menu_->addAction(IconLoader::Load("folder-new"), tr("Add to songs"), this, SLOT(AddSongs()));
    context_menu_->addSeparator();
  }

  if (ui_->results->selectionModel() && ui_->results->selectionModel()->selectedRows().length() == 1) {
    context_actions_ << context_menu_->addAction(IconLoader::Load("search"), tr("Search for this"), this, SLOT(SearchForThis()));
  }

  context_menu_->addSeparator();
  context_menu_->addMenu(tr("Group by"))->addActions(group_by_actions_->actions());

  context_menu_->addAction(IconLoader::Load("configure"), tr("Configure %1...").arg(Song::TextForSource(engine_->source())), this, SLOT(OpenSettingsDialog()));

  const bool enable_context_actions = ui_->results->selectionModel() && ui_->results->selectionModel()->hasSelection();

  for (QAction *action : context_actions_) {
    action->setEnabled(enable_context_actions);
  }

  context_menu_->popup(event->globalPos());

  return true;

}

void InternetSearchView::AddSelectedToPlaylist() {
  emit AddToPlaylist(SelectedMimeData());
}

void InternetSearchView::LoadSelected() {
  MimeData *data = SelectedMimeData();
  if (!data) return;

  data->clear_first_ = true;
  emit AddToPlaylist(data);
}

void InternetSearchView::AddSelectedToPlaylistEnqueue() {
  MimeData *data = SelectedMimeData();
  if (!data) return;

  data->enqueue_now_ = true;
  emit AddToPlaylist(data);
}

void InternetSearchView::OpenSelectedInNewPlaylist() {
  MimeData *data = SelectedMimeData();
  if (!data) return;

  data->open_in_new_playlist_ = true;
  emit AddToPlaylist(data);
}

void InternetSearchView::SearchForThis() {
  StartSearch(ui_->results->selectionModel()->selectedRows().first().data().toString());
}

void InternetSearchView::showEvent(QShowEvent *e) {
  QWidget::showEvent(e);
  FocusSearchField();
}

void InternetSearchView::FocusSearchField() {
  ui_->search->setFocus();
  ui_->search->selectAll();
}

void InternetSearchView::hideEvent(QHideEvent *e) {
  QWidget::hideEvent(e);
}

void InternetSearchView::FocusOnFilter(QKeyEvent *event) {
  ui_->search->setFocus();
  QApplication::sendEvent(ui_->search, event);
}

void InternetSearchView::OpenSettingsDialog() {
  app_->OpenSettingsDialogAtPage(settings_page_);
}

void InternetSearchView::GroupByClicked(QAction *action) {

  if (action->property("group_by").isNull()) {
    if (!group_by_dialog_) {
      group_by_dialog_.reset(new GroupByDialog);
      connect(group_by_dialog_.data(), SIGNAL(Accepted(CollectionModel::Grouping)), SLOT(SetGroupBy(CollectionModel::Grouping)));
    }

    group_by_dialog_->show();
    return;
  }

  SetGroupBy(action->property("group_by").value<CollectionModel::Grouping>());

}

void InternetSearchView::SetGroupBy(const CollectionModel::Grouping &g) {

  // Clear requests: changing "group by" on the models will cause all the items to be removed/added again,
  // so all the QModelIndex here will become invalid. New requests will be created for those
  // songs when they will be displayed again anyway (when InternetSearchItemDelegate::paint will call LazyLoadArt)
  art_requests_.clear();
  // Update the models
  front_model_->SetGroupBy(g, true);
  back_model_->SetGroupBy(g, false);

  // Save the setting
  QSettings s;
  s.beginGroup(settings_group_);
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

void InternetSearchView::SearchArtistsClicked(bool checked) {
  SetSearchType(InternetSearch::SearchType_Artists);
}

void InternetSearchView::SearchAlbumsClicked(bool checked) {
  SetSearchType(InternetSearch::SearchType_Albums);
}

void InternetSearchView::SearchSongsClicked(bool checked) {
  SetSearchType(InternetSearch::SearchType_Songs);
}

void InternetSearchView::SetSearchType(InternetSearch::SearchType type) {
  search_type_ = type;
  QSettings s;
  s.beginGroup(settings_group_);
  s.setValue("type", int(search_type_));
  s.endGroup();
  TextEdited(ui_->search->text());
}

void InternetSearchView::UpdateStatus(QString text) {
  ui_->progressbar->show();
  ui_->label_status->setText(text);
}

void InternetSearchView::ProgressSetMaximum(int max) {
  ui_->progressbar->setMaximum(max);
}

void InternetSearchView::UpdateProgress(int progress) {
  ui_->progressbar->setValue(progress);
}

void InternetSearchView::AddArtists() {

  MimeData *data = SelectedMimeData();
  if (!data) return;
  if (const InternetSongMimeData *internet_song_data = qobject_cast<const InternetSongMimeData*>(data)) {
    emit AddArtistsSignal(internet_song_data->songs);
  }

}

void InternetSearchView::AddAlbums() {

  MimeData *data = SelectedMimeData();
  if (!data) return;
  if (const InternetSongMimeData *internet_song_data = qobject_cast<const InternetSongMimeData*>(data)) {
    emit AddAlbumsSignal(internet_song_data->songs);
  }

}

void InternetSearchView::AddSongs() {

  MimeData *data = SelectedMimeData();
  if (!data) return;
  if (const InternetSongMimeData *internet_song_data = qobject_cast<const InternetSongMimeData*>(data)) {
    emit AddSongsSignal(internet_song_data->songs);
  }

}
