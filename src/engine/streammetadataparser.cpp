/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include "streammetadataparser.h"

#include <utility>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

#include "core/enginemetadata.h"

using namespace Qt::Literals::StringLiterals;

namespace StreamMetadataParser {

void Parse(EngineMetadata &metadata) {

  const QString trimmed_title = metadata.title.trimmed();
  if (trimmed_title.startsWith(u'{') || trimmed_title.startsWith(u'[')) {
    QJsonParseError parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(trimmed_title.toUtf8(), &parse_error);
    if (parse_error.error == QJsonParseError::NoError && (json_document.isObject() || json_document.isArray())) {
      if (!json_document.isObject()) return;

      const QJsonObject json_object = json_document.object();
      QString title;
      const QJsonValue title_value = json_object.value("title"_L1);
      if (title_value.isString()) title = title_value.toString().trimmed();
      if (title.isEmpty()) {
        const QJsonValue name_value = json_object.value("name"_L1);
        if (name_value.isString()) title = name_value.toString().trimmed();
      }
      if (title.isEmpty()) return;

      metadata.title = title;
      if (metadata.artist.isEmpty()) {
        const QJsonValue artist_value = json_object.value("artist"_L1);
        if (artist_value.isString()) metadata.artist = artist_value.toString().trimmed();
      }
      if (metadata.album.isEmpty()) {
        const QJsonValue album_value = json_object.value("album"_L1);
        if (album_value.isString()) metadata.album = album_value.toString().trimmed();
      }
      return;
    }
  }

  if (!metadata.title.isEmpty() && metadata.artist.isEmpty() && metadata.album.isEmpty()) {
    QStringList title_splitted;
    if (metadata.title.contains(" - "_L1)) {
      title_splitted = metadata.title.split(u" - "_s);
    }
    else if (metadata.title.contains(u'~')) {
      title_splitted = metadata.title.split(u'~');
    }
    if (!title_splitted.isEmpty() && title_splitted.count() >= 2) {
      int i = 0;
      for (const QString &title_part : std::as_const(title_splitted)) {
        ++i;
        switch (i) {
          case 1:
            metadata.artist = title_part.trimmed();
            break;
          case 2:
            metadata.title = title_part.trimmed();
            break;
          case 3:
            metadata.album = title_part.trimmed();
            break;
          default:
            break;
        }
      }
    }
  }

}

}  // namespace StreamMetadataParser
