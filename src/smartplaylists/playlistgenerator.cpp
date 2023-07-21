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

#include <QObject>
#include <QString>

#include "playlistgenerator.h"
#include "playlistquerygenerator.h"

using std::make_shared;

const int PlaylistGenerator::kDefaultLimit = 20;
const int PlaylistGenerator::kDefaultDynamicHistory = 5;
const int PlaylistGenerator::kDefaultDynamicFuture = 15;

PlaylistGenerator::PlaylistGenerator(QObject *parent) : QObject(parent), collection_backend_(nullptr) {}

PlaylistGeneratorPtr PlaylistGenerator::Create(const Type type) {

  Q_UNUSED(type)

  return make_shared<PlaylistQueryGenerator>();

}
