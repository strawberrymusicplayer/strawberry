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

#include "includes/shared_ptr.h"
#include "utilities/xmlutils.h"
#include "constants/playlistsettings.h"
#include "xmlparser.h"
#include "wplparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

WplParser::WplParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : XMLParser(tagreader_client, collection_backend, parent) {}

bool WplParser::TryMagic(const QByteArray &data) const {
  return data.contains("<?wpl") || data.contains("<smil>");
}

ParserBase::LoadResult WplParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  QXmlStreamReader reader(device);
  if (!Utilities::ParseUntilElement(&reader, u"smil"_s) || !Utilities::ParseUntilElement(&reader, u"body"_s)) {
    return LoadResult();
  }

  SongList songs;
  while (!reader.atEnd() && Utilities::ParseUntilElement(&reader, u"seq"_s)) {
    ParseSeq(dir, &reader, &songs, collection_lookup);
  }

  return songs;

}

void WplParser::ParseSeq(const QDir &dir, QXmlStreamReader *reader, SongList *songs, const bool collection_lookup) const {

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();
    switch (type) {
      case QXmlStreamReader::StartElement:{
        if (name == "media"_L1) {
          QString src = reader->attributes().value("src"_L1).toString();
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
        if (name == "seq"_L1) {
          return;
        }
        break;
      }
      default:
        break;
    }
  }

}

void WplParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name)

  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeProcessingInstruction("wpl"_L1, "version=\"1.0\""_L1);

  StreamElement smil(u"smil"_s, &writer);

  {
    StreamElement head(u"head"_s, &writer);
    WriteMeta("Generator"_L1, "Strawberry -- "_L1 + QLatin1String(STRAWBERRY_VERSION_DISPLAY), &writer);
    WriteMeta("ItemCount"_L1, QString::number(songs.count()), &writer);
  }

  {
    StreamElement body(u"body"_s, &writer);
    {
      StreamElement seq(u"seq"_s, &writer);
      for (const Song &song : songs) {
        writer.writeStartElement("media"_L1);
        writer.writeAttribute("src"_L1, URLOrFilename(song.url(), dir, path_type));
        writer.writeEndElement();
      }
    }
  }
}

void WplParser::WriteMeta(const QString &name, const QString &content, QXmlStreamWriter *writer) {

  writer->writeStartElement("meta"_L1);
  writer->writeAttribute("name"_L1, name);
  writer->writeAttribute("content"_L1, content);
  writer->writeEndElement();

}
