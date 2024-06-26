/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, David Sansome <me@davidsansome.com>
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

#include "version.h"

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "core/shared_ptr.h"
#include "utilities/xmlutils.h"
#include "settings/playlistsettingspage.h"
#include "xmlparser.h"
#include "wplparser.h"

class CollectionBackendInterface;

WplParser::WplParser(SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : XMLParser(collection_backend, parent) {}

bool WplParser::TryMagic(const QByteArray &data) const {
  return data.contains("<?wpl") || data.contains("<smil>");
}

SongList WplParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  SongList ret;

  QXmlStreamReader reader(device);
  if (!Utilities::ParseUntilElement(&reader, QStringLiteral("smil")) || !Utilities::ParseUntilElement(&reader, QStringLiteral("body"))) {
    return ret;
  }

  while (!reader.atEnd() && Utilities::ParseUntilElement(&reader, QStringLiteral("seq"))) {
    ParseSeq(dir, &reader, &ret, collection_lookup);
  }
  return ret;

}

void WplParser::ParseSeq(const QDir &dir, QXmlStreamReader *reader, SongList *songs, const bool collection_lookup) const {

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();
    switch (type) {
      case QXmlStreamReader::StartElement:{
        if (name == QLatin1String("media")) {
          QString src = reader->attributes().value(QLatin1String("src")).toString();
          if (!src.isEmpty()) {
            Song song = LoadSong(src, 0, 0, dir, collection_lookup);
            if (song.is_valid()) {
              songs->append(song);
            }
          }
        }
        else {
          Utilities::ConsumeCurrentElement(reader);
        }
        break;
      }
      case QXmlStreamReader::EndElement:{
        if (name == QLatin1String("seq")) {
          return;
        }
        break;
      }
      default:
        break;
    }
  }

}

void WplParser::Save(const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettingsPage::PathType path_type) const {

  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeProcessingInstruction(QLatin1String("wpl"), QLatin1String("version=\"1.0\""));

  StreamElement smil(QStringLiteral("smil"), &writer);

  {
    StreamElement head(QStringLiteral("head"), &writer);
    WriteMeta(QLatin1String("Generator"), QLatin1String("Strawberry -- ") + QLatin1String(STRAWBERRY_VERSION_DISPLAY), &writer);
    WriteMeta(QLatin1String("ItemCount"), QString::number(songs.count()), &writer);
  }

  {
    StreamElement body(QStringLiteral("body"), &writer);
    {
      StreamElement seq(QStringLiteral("seq"), &writer);
      for (const Song &song : songs) {
        writer.writeStartElement(QLatin1String("media"));
        writer.writeAttribute(QLatin1String("src"), URLOrFilename(song.url(), dir, path_type));
        writer.writeEndElement();
      }
    }
  }
}

void WplParser::WriteMeta(const QString &name, const QString &content, QXmlStreamWriter *writer) {

  writer->writeStartElement(QLatin1String("meta"));
  writer->writeAttribute(QLatin1String("name"), name);
  writer->writeAttribute(QLatin1String("content"), content);
  writer->writeEndElement();

}
