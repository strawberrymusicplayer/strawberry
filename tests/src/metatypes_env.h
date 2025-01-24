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

#ifndef METATYPES_ENV_H
#define METATYPES_ENV_H

#include "gtest_include.h"

#include <QMetaType>
#include <QModelIndex>

#include "core/song.h"
#include "core/songloader.h"
#include "collection/collectiondirectory.h"

class MetatypesEnvironment : public ::testing::Environment {
 public:
  MetatypesEnvironment() = default;
  void SetUp() override {
    qRegisterMetaType<CollectionDirectory>("CollectionDirectory");
    qRegisterMetaType<CollectionDirectoryList>("CollectionDirectoryList");
    qRegisterMetaType<CollectionSubdirectory>("CollectionSubdirectory");
    qRegisterMetaType<CollectionSubdirectoryList>("CollectionSubdirectoryList");
    qRegisterMetaType<SongList>("SongList");
    qRegisterMetaType<QModelIndex>("QModelIndex");
    qRegisterMetaType<SongLoader::Result>("SongLoader::Result");
  }
 private:
  Q_DISABLE_COPY(MetatypesEnvironment)
};

#endif  // RESOURCES_ENV_H
