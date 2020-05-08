/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/application.h"
#include "coverprovider.h"

CoverProvider::CoverProvider(const QString &name, const bool enabled, const bool authentication_required, const float quality, const bool fetchall, const bool allow_missing_album, Application *app, QObject *parent) : QObject(parent), app_(app), name_(name), enabled_(enabled), order_(0), authentication_required_(authentication_required), quality_(quality), fetchall_(fetchall), allow_missing_album_(allow_missing_album) {}
