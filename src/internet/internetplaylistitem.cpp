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

#include <QApplication>
#include <QVariant>

#include "internetplaylistitem.h"
#include "internetservices.h"
#include "internetservice.h"
#include "core/settingsprovider.h"
#include "core/sqlrow.h"
#include "playlist/playlistbackend.h"

InternetPlaylistItem::InternetPlaylistItem(const Song::Source source)
    : PlaylistItem(source),
      source_(source) {}

InternetPlaylistItem::InternetPlaylistItem(const Song &metadata)
    : PlaylistItem(metadata.source()),
      source_(metadata.source()),
      metadata_(metadata) {
  InitMetadata();
}

InternetPlaylistItem::InternetPlaylistItem(InternetService *service, const Song &metadata)
    : PlaylistItem(metadata.source()),
      source_(service->source()),
      metadata_(metadata) {
  InitMetadata();
}

bool InternetPlaylistItem::InitFromQuery(const SqlRow &query) {

  metadata_.InitFromQuery(query, false);
  InitMetadata();
  return true;

}

QVariant InternetPlaylistItem::DatabaseValue(DatabaseColumn column) const {
  return PlaylistItem::DatabaseValue(column);
}

void InternetPlaylistItem::InitMetadata() {

  if (metadata_.title().isEmpty()) metadata_.set_title(metadata_.url().toString());
  if (metadata_.source() == Song::Source::Unknown) metadata_.set_source(Song::Source::Stream);
  if (metadata_.filetype() == Song::FileType::Unknown) metadata_.set_filetype(Song::FileType::Stream);
  metadata_.set_valid(true);

}

Song InternetPlaylistItem::Metadata() const {

  if (HasTemporaryMetadata()) return temp_metadata_;
  return metadata_;

}

QUrl InternetPlaylistItem::Url() const { return metadata_.url(); }

void InternetPlaylistItem::SetArtManual(const QUrl &cover_url) {

  metadata_.set_art_manual(cover_url);
  temp_metadata_.set_art_manual(cover_url);

}
