/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "scrobblerservice.h"

#include "core/song.h"

ScrobblerService::ScrobblerService(const QString &name, QObject *parent) : QObject(parent), name_(name) {}

bool ScrobblerService::ExtractJsonObj(const QByteArray &data, QJsonObject &json_obj, QString &error_description) {

  QJsonParseError json_parse_error;
  const QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_parse_error);

  if (json_parse_error.error != QJsonParseError::NoError) {
    error_description = json_parse_error.errorString();
    return false;
  }

  if (json_doc.isObject()) {
    json_obj = json_doc.object();
  }

  return true;

}

QString ScrobblerService::StripAlbum(QString album) const {

  return album.remove(Song::kAlbumRemoveDisc).remove(Song::kAlbumRemoveMisc);

}

QString ScrobblerService::StripTitle(QString title) const {

  return title.remove(Song::kTitleRemoveMisc);

}
