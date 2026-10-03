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

#include <QWidget>
#include <QString>
#include <QMenu>
#include <QAction>

#include "core/iconloader.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilterwidget.h"
#include "streamingcollectionview.h"

using namespace Qt::Literals::StringLiterals;

StreamingCollectionView::StreamingCollectionView(QWidget *parent)
    : CollectionTreeView(parent),
      favorite_(false),
      action_remove_songs_(nullptr) {

  SetAutoOpen(false);

}

void StreamingCollectionView::Init(CollectionModel *collection_model, const bool favorite) {

  SetCollectionModel(collection_model);
  favorite_ = favorite;

  ReloadSettings();

}

void StreamingCollectionView::ReloadSettings() {

  if (collection_model()) collection_model()->ReloadSettings();
  if (filter_widget()) filter_widget()->ReloadSettings();

}

QString StreamingCollectionView::EmptyTitleText() const {
  return tr("The streaming collection is empty!");
}

QString StreamingCollectionView::EmptyText() const {
  return tr("Click here to retrieve music");
}

void StreamingCollectionView::EmptyClicked() {
  Q_EMIT GetSongs();
}

void StreamingCollectionView::AddContextMenuActions(QMenu *menu) {

  if (favorite_) {
    action_remove_songs_ = menu->addAction(IconLoader::Load(u"edit-delete"_s), tr("Remove from favorites"), this, &StreamingCollectionView::RemoveSelectedSongs);
    menu->addSeparator();
  }

}

void StreamingCollectionView::UpdateContextMenuActions(const bool has_selection) {

  if (action_remove_songs_) action_remove_songs_->setEnabled(has_selection);

}

void StreamingCollectionView::RemoveSelectedSongs() {

  Q_EMIT RemoveSongs(GetSelectedSongs());

}
