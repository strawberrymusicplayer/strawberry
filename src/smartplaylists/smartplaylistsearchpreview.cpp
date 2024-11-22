/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QWidget>
#include <QAbstractItemView>
#include <QString>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

#include "includes/shared_ptr.h"

#include "smartplaylistsearchpreview.h"
#include "ui_smartplaylistsearchpreview.h"

#include "playlist/playlist.h"
#include "playlistquerygenerator.h"

using std::make_shared;

SmartPlaylistSearchPreview::SmartPlaylistSearchPreview(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_SmartPlaylistSearchPreview),
      collection_backend_(nullptr),
      model_(nullptr) {

  ui_->setupUi(this);

  // Prevent editing songs and saving settings (like header columns and geometry)
  ui_->tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui_->tree->SetReadOnlySettings(true);

  QFont bold_font;
  bold_font.setBold(true);
  ui_->preview_label->setFont(bold_font);
  ui_->busy_container->hide();

}

SmartPlaylistSearchPreview::~SmartPlaylistSearchPreview() {
  delete ui_;
}

void SmartPlaylistSearchPreview::Init(const SharedPtr<Player> player,
                                      const SharedPtr<PlaylistManager> playlist_manager,
                                      const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
                                      const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
                                      const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader) {

  collection_backend_ = collection_backend;

  model_ = new Playlist(nullptr, nullptr, nullptr, collection_backend_, nullptr, -1, QString(), false, this);
  ui_->tree->setModel(model_);
  ui_->tree->SetPlaylist(model_);

  ui_->tree->Init(player,
                  playlist_manager,
                  collection_backend,
#ifdef HAVE_MOODBAR
                  moodbar_loader,
#endif
                  current_albumcover_loader);

}

void SmartPlaylistSearchPreview::Update(const SmartPlaylistSearch &search) {

  if (search == last_search_) {
    // This search was the same as the last one we did
    return;
  }

  if (generator_ || isHidden()) {
    // It's busy generating something already, or the widget isn't visible
    pending_search_ = search;
    return;
  }

  RunSearch(search);

}

void SmartPlaylistSearchPreview::showEvent(QShowEvent *e) {

  if (pending_search_.is_valid() && !generator_) {
    // There was a search waiting while we were hidden, so run it now
    RunSearch(pending_search_);
    pending_search_ = SmartPlaylistSearch();
  }

  QWidget::showEvent(e);

}

namespace {
PlaylistItemPtrList DoRunSearch(PlaylistGeneratorPtr gen) { return gen->Generate(); }
}  // namespace

void SmartPlaylistSearchPreview::RunSearch(const SmartPlaylistSearch &search) {

  generator_ = make_shared<PlaylistQueryGenerator>();
  generator_->set_collection_backend(collection_backend_);
  std::dynamic_pointer_cast<PlaylistQueryGenerator>(generator_)->Load(search);

  ui_->busy_container->show();
  ui_->count_label->hide();
  QFuture<PlaylistItemPtrList> future = QtConcurrent::run(DoRunSearch, generator_);
  QFutureWatcher<PlaylistItemPtrList> *watcher = new QFutureWatcher<PlaylistItemPtrList>();
  QObject::connect(watcher, &QFutureWatcher<PlaylistItemPtrList>::finished, this, &SmartPlaylistSearchPreview::SearchFinished);
  watcher->setFuture(future);

}

void SmartPlaylistSearchPreview::SearchFinished() {

  QFutureWatcher<PlaylistItemPtrList> *watcher = static_cast<QFutureWatcher<PlaylistItemPtrList>*>(sender());
  PlaylistItemPtrList all_items = watcher->result();
  watcher->deleteLater();

  last_search_ = std::dynamic_pointer_cast<PlaylistQueryGenerator>(generator_)->search();
  generator_.reset();

  if (pending_search_.is_valid() && pending_search_ != last_search_) {
    // There was another search done while we were running
    // throw away these results and do that one now instead
    RunSearch(pending_search_);
    pending_search_ = SmartPlaylistSearch();
    return;
  }

  PlaylistItemPtrList displayed_items = all_items.mid(0, PlaylistGenerator::kDefaultLimit);

  model_->Clear();
  model_->InsertItems(displayed_items);

  if (displayed_items.count() < all_items.count()) {
    ui_->count_label->setText(tr("%1 songs found (showing %2)").arg(all_items.count()).arg(displayed_items.count()));
  }
  else {
    ui_->count_label->setText(tr("%1 songs found").arg(all_items.count()));
  }

  ui_->busy_container->hide();
  ui_->count_label->show();

}
