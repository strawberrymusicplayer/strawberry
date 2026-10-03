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

#include <QStandardItemModel>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QSize>
#include <QUrl>

#include "core/iconloader.h"
#include "core/mimedata.h"
#include "streamsongmimedata.h"
#include "streamingservice.h"
#include "streamingsearchmodel.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Container types shown with the album icon, or the album cover with pretty covers.
bool HasAlbumIcon(const CollectionModel::GroupBy group_by) {

  switch (group_by) {
    case CollectionModel::GroupBy::Disc:
    case CollectionModel::GroupBy::Genre:
    case CollectionModel::GroupBy::Composer:
    case CollectionModel::GroupBy::Performer:
    case CollectionModel::GroupBy::Grouping:
      return true;
    default:
      return CollectionModel::IsAlbumGroupBy(group_by);
  }

}

void GatherResults(const QStandardItem *parent, StreamingSearchModel::ResultList *results) {

  const QVariant result = parent->data(StreamingSearchModel::Role_Result);
  if (result.isValid()) {
    results->append(result.value<StreamingSearchModel::Result>());
  }

  for (int i = 0; i < parent->rowCount(); ++i) {
    GatherResults(parent->child(i), results);
  }

}

}  // namespace

StreamingSearchModel::StreamingSearchModel(StreamingServicePtr service, QObject *parent)
    : QStandardItemModel(parent),
      service_(service),
      use_pretty_covers_(true),
      artist_icon_(IconLoader::Load(u"folder-sound"_s)),
      album_icon_(IconLoader::Load(u"cdcase"_s)),
      group_by_(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::AlbumDisc, CollectionModel::GroupBy::None) {

  const QList<QSize> nocover_sizes = album_icon_.availableSizes();
  const QPixmap nocover_pixmap = nocover_sizes.isEmpty() ? album_icon_.pixmap(CollectionModel::kPrettyCoverSize, CollectionModel::kPrettyCoverSize) : album_icon_.pixmap(nocover_sizes.last());
  no_cover_icon_ = nocover_pixmap.scaled(CollectionModel::kPrettyCoverSize, CollectionModel::kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

}

void StreamingSearchModel::AddResults(const ResultList &results) {

  for (const Result &result : results) {
    // Find (or create) the container nodes for this result.
    QStringList key;
    QStandardItem *parent = BuildContainers(result.metadata_, invisibleRootItem(), &key);

    QStandardItem *item = new QStandardItem;
    item->setText(result.metadata_.TitleWithCompilationArtist());
    item->setData(QVariant::fromValue(result), Role_Result);
    parent->appendRow(item);
  }

}

QStandardItem *StreamingSearchModel::BuildContainers(const Song &song, QStandardItem *parent, QStringList *key, const int level) {

  if (level >= 3) {
    return parent;
  }

  const CollectionModel::GroupBy group_by = group_by_[level];
  if (group_by == CollectionModel::GroupBy::None || group_by == CollectionModel::GroupBy::GroupByCount) {
    return parent;
  }

  const bool is_artist_container = CollectionModel::IsArtistGroupBy(group_by);
  const bool is_album_container = CollectionModel::IsAlbumGroupBy(group_by);

  QString display_text;
  QString sort_text;
  if (is_artist_container && song.is_compilation()) {
    display_text = tr("Various artists");
    sort_text = "aaaaaa"_L1;
  }
  else {
    display_text = CollectionModel::TextOrUnknown(CollectionModel::DisplayText(group_by, song));
    sort_text = CollectionModel::SortText(group_by, song, true, false, true);
  }
  if (sort_text.isEmpty()) {
    sort_text = display_text;
  }

  // The key is based on the plain display text so that per-song differences in the (optional) qualifier suffix below can't split one album into multiple containers.
  key->append(is_album_container ? display_text + song.album_id() : display_text);

  QStandardItem *container = containers_.value(*key);
  if (!container) {
    if (is_album_container) {
      display_text += AlbumQualifierSuffix(song);
    }
    container = new QStandardItem(display_text);
    container->setData(sort_text, CollectionModel::Role_SortText);
    container->setData(static_cast<int>(group_by), CollectionModel::Role_ContainerType);

    if (is_artist_container) {
      container->setIcon(artist_icon_);
    }
    else if (HasAlbumIcon(group_by)) {
      if (use_pretty_covers_) {
        container->setData(no_cover_icon_, Qt::DecorationRole);
      }
      else {
        container->setIcon(album_icon_);
      }
    }

    parent->appendRow(container);
    containers_.insert(*key, container);
  }

  // Create the container for the next level.
  return BuildContainers(song, container, key, level + 1);

}

QString StreamingSearchModel::AlbumQualifierSuffix(const Song &song) const {

  QString suffix;

  // Skip if the title already contains the edition text.
  if (service_->show_search_album_edition() && !song.edition().isEmpty() && !song.album().contains(song.edition(), Qt::CaseInsensitive)) {
    suffix += QStringLiteral(" (%1)").arg(song.edition());
  }

  if (service_->show_search_album_quality() && !song.album_quality().isEmpty()) {
    suffix += QStringLiteral(" [%1]").arg(song.album_quality());
  }

  return suffix;

}

void StreamingSearchModel::Clear() {

  containers_.clear();
  clear();

}

StreamingSearchModel::ResultList StreamingSearchModel::AllResults() const {

  ResultList results;
  GatherResults(invisibleRootItem(), &results);
  return results;

}

void StreamingSearchModel::SetGroupBy(const CollectionModel::Grouping grouping, const bool regroup_now) {

  const CollectionModel::Grouping old_group_by = group_by_;
  group_by_ = grouping;

  if (regroup_now && group_by_ != old_group_by) {
    ReloadResults();
  }

}

void StreamingSearchModel::ReloadResults() {

  // Reset the model and add the results again, so the containers are built with the current settings.
  const ResultList results = AllResults();
  Clear();
  AddResults(results);

}

MimeData *StreamingSearchModel::LoadTracks(const ResultList &results) const {

  if (results.isEmpty()) {
    return nullptr;
  }

  SongList songs;
  QList<QUrl> urls;
  songs.reserve(results.count());
  urls.reserve(results.count());
  for (const Result &result : results) {
    songs << result.metadata_;
    urls << result.metadata_.url();
  }

  StreamSongMimeData *mimedata = new StreamSongMimeData(service_);
  mimedata->songs = songs;
  mimedata->setUrls(urls);

  return mimedata;

}
