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

#ifndef MOCK_PLAYLISTITEM_H
#define MOCK_PLAYLISTITEM_H

#include "gmock_include.h"

#include <QVariant>
#include <QUrl>

#include "core/song.h"
#include "core/sqlrow.h"
#include "playlist/playlistitem.h"

// clazy:excludeall=returning-void-expression,function-args-by-value

class MockPlaylistItem : public PlaylistItem {
 public:
  MockPlaylistItem();
  MOCK_CONST_METHOD0(options, Options());
  MOCK_CONST_METHOD0(OriginalMetadata, Song());
  MOCK_CONST_METHOD0(OriginalUrl, QUrl());
  MOCK_METHOD1(SetStreamMetadata, void(const Song &song));
  MOCK_METHOD0(ClearStreamMetadata, void());
  MOCK_METHOD1(SetArtManual, void(const QUrl &cover_url));
  MOCK_METHOD1(InitFromQuery, bool(const SqlRow &settings));
  MOCK_METHOD0(Reload, void());
  MOCK_CONST_METHOD1(DatabaseValue, QVariant(DatabaseColumn));
};

#endif  // MOCK_PLAYLISTITEM_H
