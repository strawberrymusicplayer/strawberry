/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QStandardItem>
#include <QVariant>
#include <QString>

#include "core/logging.h"
#include "core/mimedata.h"
#include "internetmodel.h"
#include "internetservice.h"

InternetService::InternetService(Song::Source source, const QString &name, Application *app, InternetModel *model, QObject *parent)
    : QObject(parent), app_(app), model_(model), source_(source), name_(name) {
}
