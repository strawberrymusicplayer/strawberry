/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QSortFilterProxyModel>
#include <QVariant>
#include <QString>

#include "collectionfilter.h"
#include "collectionmodel.h"
#include "collectionitem.h"

CollectionFilter::CollectionFilter(QObject *parent) : QSortFilterProxyModel(parent) {
  setDynamicSortFilter(true);
  setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool CollectionFilter::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {

  CollectionModel *model = qobject_cast<CollectionModel*>(sourceModel());
  if (!model) return false;
  QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
  if (!idx.isValid()) return false;
  CollectionItem *item = model->IndexToItem(idx);
  if (!item) return false;

  if (item->type == CollectionItem::Type_LoadingIndicator) return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QString filter = filterRegularExpression().pattern().remove('\\');
#else
  QString filter = filterRegExp().pattern();
#endif

  if (filter.isEmpty()) return true;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QStringList tokens(filter.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts));
#else
  QStringList tokens(filter.split(QRegularExpression("\\s+"), QString::SkipEmptyParts));
#endif

  filter.clear();

  QMap<QString, QString> tags;
  for (QString token : tokens) {
    if (token.contains(':')) {
      if (Song::kColumns.contains(token.section(':', 0, 0), Qt::CaseInsensitive)) {
        QString tag = token.section(':', 0, 0).remove(':').trimmed();
        QString value = token.section(':', 1, -1).remove(':').trimmed();
        if (!tag.isEmpty() && !value.isEmpty()) {
          tags.insert(tag, value);
        }
      }
      else {
        token = token.remove(':').trimmed();
        if (!token.isEmpty()) {
          if (!filter.isEmpty()) filter.append(" ");
          filter += token;
        }
      }
    }
    else {
      if (!filter.isEmpty()) filter.append(" ");
      filter += token;
    }
  }

  if (ItemMatches(model, item, tags, filter)) return true;

  for (CollectionItem *parent = item->parent ; parent ; parent = parent->parent) {
    if (ItemMatches(model, parent, tags, filter)) return true;
  }

  return ChildrenMatches(model, item, tags, filter);

}

bool CollectionFilter::ItemMatches(CollectionModel *model, CollectionItem *item, const QMap<QString, QString> &tags, const QString &filter) const {

  if (
      (filter.isEmpty() || item->DisplayText().contains(filter, Qt::CaseInsensitive))
      &&
      (
      tags.isEmpty() // If no tags were specified, only the filter needs to match.
      ||
      (item->metadata.is_valid() && TagMatches(item, tags)) // Song node
      ||
      (item->container_level >= 0 && item->container_level <= 2 && TagMatches(item, model->GetGroupBy()[item->container_level], tags)) // Container node
      )
  ) { return true; }

  return false;

}

bool CollectionFilter::ChildrenMatches(CollectionModel *model, CollectionItem *item, const QMap<QString, QString> &tags, const QString &filter) const {

  if (ItemMatches(model, item, tags, filter)) return true;

  for (CollectionItem *child : item->children) {
    if (ChildrenMatches(model, child, tags, filter)) return true;
  }

  return false;

}

bool CollectionFilter::TagMatches(CollectionItem *item, const QMap<QString, QString> &tags) const {

  Song &metadata = item->metadata;

  for (QMap<QString, QString>::const_iterator it = tags.begin() ; it != tags.end() ; ++it) {
    QString tag = it.key().toLower();
    QString value = it.value();
    if (tag == "albumartist" && metadata.effective_albumartist().contains(value, Qt::CaseInsensitive)) return true;
    if (tag == "artist" && metadata.artist().contains(value, Qt::CaseInsensitive)) return true;
    if (tag == "album" && metadata.album().contains(value, Qt::CaseInsensitive)) return true;
    if (tag == "title" && metadata.title().contains(value, Qt::CaseInsensitive)) return true;
  }

  return false;

}

bool CollectionFilter::TagMatches(CollectionItem *item, const CollectionModel::GroupBy group_by, const QMap<QString, QString> &tags) const {

  QString tag;
  switch (group_by) {
    case CollectionModel::GroupBy_AlbumArtist:
      tag = "albumartist";
      break;
    case CollectionModel::GroupBy_Artist:
      tag = "artist";
      break;
    case CollectionModel::GroupBy_Album:
    case CollectionModel::GroupBy_AlbumDisc:
    case CollectionModel::GroupBy_YearAlbum:
    case CollectionModel::GroupBy_YearAlbumDisc:
    case CollectionModel::GroupBy_OriginalYearAlbum:
    case CollectionModel::GroupBy_OriginalYearAlbumDisc:
      tag = "album";
      break;
    case CollectionModel::GroupBy_Disc:
    case CollectionModel::GroupBy_Year:
    case CollectionModel::GroupBy_OriginalYear:
      break;
    case CollectionModel::GroupBy_Genre:
      tag = "genre";
      break;
    case CollectionModel::GroupBy_Composer:
      tag = "composer";
      break;
    case CollectionModel::GroupBy_Performer:
      tag = "performer";
      break;
    case CollectionModel::GroupBy_Grouping:
      tag = "grouping";
      break;
    case CollectionModel::GroupBy_FileType:
      tag = "filetype";
      break;
    case CollectionModel::GroupBy_Format:
    case CollectionModel::GroupBy_Bitdepth:
    case CollectionModel::GroupBy_Samplerate:
    case CollectionModel::GroupBy_Bitrate:
    case CollectionModel::GroupBy_None:
    case CollectionModel::GroupByCount:
      break;
  }

  QString value;
  if (!tag.isEmpty() && tags.contains(tag)) {
    value = tags[tag];
  }

  return !value.isEmpty() && item->DisplayText().contains(value, Qt::CaseInsensitive);

}
