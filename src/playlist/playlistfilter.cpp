/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

#include "playlist/playlist.h"
#include "playlistfilter.h"
#include "playlistfilterparser.h"

PlaylistFilter::PlaylistFilter(QObject *parent)
    : QSortFilterProxyModel(parent),
      filter_tree_(new PlaylistNopFilter),
      query_hash_(0) {

  setDynamicSortFilter(true);

  column_names_[QStringLiteral("title")] = static_cast<int>(Playlist::Column::Title);
  column_names_[QStringLiteral("name")] = static_cast<int>(Playlist::Column::Title);
  column_names_[QStringLiteral("artist")] = static_cast<int>(Playlist::Column::Artist);
  column_names_[QStringLiteral("album")] = static_cast<int>(Playlist::Column::Album);
  column_names_[QStringLiteral("albumartist")] = static_cast<int>(Playlist::Column::AlbumArtist);
  column_names_[QStringLiteral("performer")] = static_cast<int>(Playlist::Column::Performer);
  column_names_[QStringLiteral("composer")] = static_cast<int>(Playlist::Column::Composer);
  column_names_[QStringLiteral("year")] = static_cast<int>(Playlist::Column::Year);
  column_names_[QStringLiteral("originalyear")] = static_cast<int>(Playlist::Column::OriginalYear);
  column_names_[QStringLiteral("track")] = static_cast<int>(Playlist::Column::Track);
  column_names_[QStringLiteral("disc")] = static_cast<int>(Playlist::Column::Disc);
  column_names_[QStringLiteral("length")] = static_cast<int>(Playlist::Column::Length);
  column_names_[QStringLiteral("genre")] = static_cast<int>(Playlist::Column::Genre);
  column_names_[QStringLiteral("samplerate")] = static_cast<int>(Playlist::Column::Samplerate);
  column_names_[QStringLiteral("bitdepth")] = static_cast<int>(Playlist::Column::Bitdepth);
  column_names_[QStringLiteral("bitrate")] = static_cast<int>(Playlist::Column::Bitrate);
  column_names_[QStringLiteral("filename")] = static_cast<int>(Playlist::Column::Filename);
  column_names_[QStringLiteral("grouping")] = static_cast<int>(Playlist::Column::Grouping);
  column_names_[QStringLiteral("comment")] = static_cast<int>(Playlist::Column::Comment);
  column_names_[QStringLiteral("rating")] = static_cast<int>(Playlist::Column::Rating);
  column_names_[QStringLiteral("playcount")] = static_cast<int>(Playlist::Column::PlayCount);
  column_names_[QStringLiteral("skipcount")] = static_cast<int>(Playlist::Column::SkipCount);

  numerical_columns_ << static_cast<int>(Playlist::Column::Year)
                     << static_cast<int>(Playlist::Column::OriginalYear)
                     << static_cast<int>(Playlist::Column::Track)
                     << static_cast<int>(Playlist::Column::Disc)
                     << static_cast<int>(Playlist::Column::Length)
                     << static_cast<int>(Playlist::Column::Samplerate)
                     << static_cast<int>(Playlist::Column::Bitdepth)
                     << static_cast<int>(Playlist::Column::Bitrate)
                     << static_cast<int>(Playlist::Column::PlayCount)
                     << static_cast<int>(Playlist::Column::SkipCount);

}

PlaylistFilter::~PlaylistFilter() = default;

void PlaylistFilter::sort(int column, Qt::SortOrder order) {
  // Pass this through to the Playlist, it does sorting itself
  sourceModel()->sort(column, order);
}

bool PlaylistFilter::filterAcceptsRow(const int row, const QModelIndex &parent) const {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  size_t hash = qHash(filter_text_);
#else
  uint hash = qHash(filter_text_);
#endif
  if (hash != query_hash_) {
    // Parse the query
    PlaylistFilterParser p(filter_text_, column_names_, numerical_columns_);
    filter_tree_.reset(p.parse());

    query_hash_ = hash;
  }

  // Test the row
  return filter_tree_->accept(row, parent, sourceModel());

}

void PlaylistFilter::SetFilterText(const QString &filter_text) {

  filter_text_ = filter_text;
  setFilterFixedString(filter_text);

}
