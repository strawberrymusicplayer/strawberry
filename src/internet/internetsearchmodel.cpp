/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#include <QObject>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QPixmap>
#include <QMimeData>

#include "core/mimedata.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "internetsearch.h"
#include "internetsearchmodel.h"

InternetSearchModel::InternetSearchModel(InternetSearch *engine, QObject *parent)
    : QStandardItemModel(parent),
      engine_(engine),
      proxy_(nullptr),
      use_pretty_covers_(true),
      artist_icon_(IconLoader::Load("folder-sound")) {

  group_by_[0] = CollectionModel::GroupBy_Artist;
  group_by_[1] = CollectionModel::GroupBy_Album;
  group_by_[2] = CollectionModel::GroupBy_None;

  QIcon nocover = IconLoader::Load("cdcase");
  no_cover_icon_ = nocover.pixmap(nocover.availableSizes().last()).scaled(CollectionModel::kPrettyCoverSize, CollectionModel::kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  //no_cover_icon_ = QPixmap(":/pictures/noalbumart.png").scaled(CollectionModel::kPrettyCoverSize, CollectionModel::kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  album_icon_ = no_cover_icon_;

}

void InternetSearchModel::AddResults(const InternetSearch::ResultList &results) {

  int sort_index = 0;

  for (const InternetSearch::Result &result : results) {
    QStandardItem *parent = invisibleRootItem();

    // Find (or create) the container nodes for this result if we can.
    ContainerKey key;
    key.provider_index_ = sort_index;
    parent = BuildContainers(result.metadata_, parent, &key);

    // Create the item
    QStandardItem *item = new QStandardItem;
    item->setText(result.metadata_.TitleWithCompilationArtist());
    item->setData(QVariant::fromValue(result), Role_Result);
    item->setData(sort_index, Role_ProviderIndex);

    parent->appendRow(item);

  }

}

QStandardItem *InternetSearchModel::BuildContainers(const Song &s, QStandardItem *parent, ContainerKey *key, int level) {

  if (level >= 3) {
    return parent;
  }

  bool has_artist_icon = false;
  bool has_album_icon = false;
  QString display_text;
  QString sort_text;
  int unique_tag = -1;
  int year = 0;

  switch (group_by_[level]) {
    case CollectionModel::GroupBy_Artist:
      if (s.is_compilation()) {
        display_text = tr("Various artists");
        sort_text = "aaaaaa";
      }
      else {
        display_text = CollectionModel::TextOrUnknown(s.artist());
        sort_text = CollectionModel::SortTextForArtist(s.artist());
      }
      has_artist_icon = true;
      break;

    case CollectionModel::GroupBy_YearAlbum:
      year = qMax(0, s.year());
      display_text = CollectionModel::PrettyYearAlbum(year, s.album());
      sort_text = CollectionModel::SortTextForNumber(year) + s.album();
      unique_tag = s.album_id();
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy_OriginalYearAlbum:
      year = qMax(0, s.effective_originalyear());
      display_text = CollectionModel::PrettyYearAlbum(year, s.album());
      sort_text = CollectionModel::SortTextForNumber(year) + s.album();
      unique_tag = s.album_id();
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy_Year:
      year = qMax(0, s.year());
      display_text = QString::number(year);
      sort_text = CollectionModel::SortTextForNumber(year) + " ";
      break;

    case CollectionModel::GroupBy_OriginalYear:
      year = qMax(0, s.effective_originalyear());
      display_text = QString::number(year);
      sort_text = CollectionModel::SortTextForNumber(year) + " ";
      break;

    case CollectionModel::GroupBy_Composer:
      display_text = s.composer();
    case CollectionModel::GroupBy_Performer:
      display_text = s.performer();
    case CollectionModel::GroupBy_Disc:
      display_text = s.disc();
    case CollectionModel::GroupBy_Grouping:
      display_text = s.grouping();
    case CollectionModel::GroupBy_Genre:
      if (display_text.isNull()) display_text = s.genre();
    case CollectionModel::GroupBy_Album:
      unique_tag = s.album_id();
      if (display_text.isNull()) {
        display_text = s.album();
      }
    // fallthrough
    case CollectionModel::GroupBy_AlbumArtist:
      if (display_text.isNull()) display_text = s.effective_albumartist();
      display_text = CollectionModel::TextOrUnknown(display_text);
      sort_text = CollectionModel::SortTextForArtist(display_text);
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy_FileType:
      display_text = s.TextForFiletype();
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy_Bitrate:
      display_text = QString(s.bitrate(), 1);
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy_Samplerate:
      display_text = QString(s.samplerate(), 1);
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy_Bitdepth:
      display_text = QString(s.bitdepth(), 1);
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy_None:
      return parent;
  }

  // Find a container for this level
  key->group_[level] = display_text + QString::number(unique_tag);
  QStandardItem *container = containers_[*key];
  if (!container) {
    container = new QStandardItem(display_text);
    container->setData(key->provider_index_, Role_ProviderIndex);
    container->setData(sort_text, CollectionModel::Role_SortText);
    container->setData(group_by_[level], CollectionModel::Role_ContainerType);

    if (has_artist_icon) {
      container->setIcon(artist_icon_);
    }
    else if (has_album_icon) {
      if (use_pretty_covers_) {
        container->setData(no_cover_icon_, Qt::DecorationRole);
      }
      else {
        container->setIcon(album_icon_);
      }
    }

    parent->appendRow(container);
    containers_[*key] = container;
  }

  // Create the container for the next level.
  return BuildContainers(s, container, key, level + 1);

}

void InternetSearchModel::Clear() {
  containers_.clear();
  clear();
}

InternetSearch::ResultList InternetSearchModel::GetChildResults(const QModelIndexList &indexes) const {

  QList<QStandardItem*> items;
  for (const QModelIndex &index : indexes) {
    items << itemFromIndex(index);
  }
  return GetChildResults(items);

}

InternetSearch::ResultList InternetSearchModel::GetChildResults(const QList<QStandardItem*> &items) const {

  InternetSearch::ResultList results;
  QSet<const QStandardItem*> visited;

  for (QStandardItem *item : items) {
    GetChildResults(item, &results, &visited);
  }

  return results;

}

void InternetSearchModel::GetChildResults(const QStandardItem *item, InternetSearch::ResultList *results, QSet<const QStandardItem*> *visited) const {

  if (visited->contains(item)) {
    return;
  }
  visited->insert(item);

  // Does this item have children?
  if (item->rowCount()) {
    const QModelIndex parent_proxy_index = proxy_->mapFromSource(item->index());

    // Yes - visit all the children, but do so through the proxy so we get them
    // in the right order.
    for (int i = 0; i < item->rowCount(); ++i) {
      const QModelIndex proxy_index = parent_proxy_index.child(i, 0);
      const QModelIndex index = proxy_->mapToSource(proxy_index);
      GetChildResults(itemFromIndex(index), results, visited);
    }
  }
  else {
    // No - maybe it's a song, add its result if valid
    QVariant result = item->data(Role_Result);
    if (result.isValid()) {
      results->append(result.value<InternetSearch::Result>());
    }
    else {
      // Maybe it's a provider then?
      bool is_provider;
      const int sort_index = item->data(Role_ProviderIndex).toInt(&is_provider);
      if (is_provider) {
        // Go through all the items (through the proxy to keep them ordered) and add the ones belonging to this provider to our list
        for (int i = 0; i < proxy_->rowCount(invisibleRootItem()->index()); ++i) {
          QModelIndex child_index = proxy_->index(i, 0, invisibleRootItem()->index());
          const QStandardItem *child_item = itemFromIndex(proxy_->mapToSource(child_index));
          if (child_item->data(Role_ProviderIndex).toInt() == sort_index) {
            GetChildResults(child_item, results, visited);
          }
        }
      }
    }
  }

}

QMimeData *InternetSearchModel::mimeData(const QModelIndexList &indexes) const {
  return engine_->LoadTracks(GetChildResults(indexes));
}

namespace {
void GatherResults(const QStandardItem *parent, InternetSearch::ResultList *results) {

  QVariant result_variant = parent->data(InternetSearchModel::Role_Result);
  if (result_variant.isValid()) {
    InternetSearch::Result result = result_variant.value<InternetSearch::Result>();
    (*results).append(result);
  }

  for (int i = 0; i < parent->rowCount(); ++i) {
    GatherResults(parent->child(i), results);
  }
}
}

void InternetSearchModel::SetGroupBy(const CollectionModel::Grouping &grouping, bool regroup_now) {

  const CollectionModel::Grouping old_group_by = group_by_;
  group_by_ = grouping;

  if (regroup_now && group_by_ != old_group_by) {
    // Walk the tree gathering the results we have already
    InternetSearch::ResultList results;
    GatherResults(invisibleRootItem(), &results);

    // Reset the model and re-add all the results using the new grouping.
    Clear();
    AddResults(results);
  }

}
