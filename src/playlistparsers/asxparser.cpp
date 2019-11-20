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

#include <QObject>
#include <QIODevice>
#include <QBuffer>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QStringBuilder>
#include <QRegExp>
#include <QUrl>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "asxparser.h"
#include "core/utilities.h"
#include "playlistparsers/xmlparser.h"

class CollectionBackendInterface;

ASXParser::ASXParser(CollectionBackendInterface *collection, QObject *parent)
    : XMLParser(collection, parent) {}

SongList ASXParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir) const {

  Q_UNUSED(playlist_path);

  // We have to load everything first so we can munge the "XML".
  QByteArray data = device->readAll();

  // Some playlists have unescaped & characters in URLs :(
  QRegExp ex("(href\\s*=\\s*\")([^\"]+)\"");
  int index = 0;
  while ((index = ex.indexIn(data, index)) != -1) {
    QString url = ex.cap(2);
    url.replace(QRegExp("&(?!amp;|quot;|apos;|lt;|gt;)"), "&amp;");

    QByteArray replacement = QString(ex.cap(1) + url + "\"").toLocal8Bit();
    data.replace(ex.cap(0).toLocal8Bit(), replacement);
    index += replacement.length();
  }

  QBuffer buffer(&data);
  buffer.open(QIODevice::ReadOnly);

  SongList ret;

  QXmlStreamReader reader(&buffer);
  if (!Utilities::ParseUntilElementCI(&reader, "asx")) {
    return ret;
  }

  while (!reader.atEnd() && Utilities::ParseUntilElementCI(&reader, "entry")) {
    Song song = ParseTrack(&reader, dir);
    if (song.is_valid()) {
      ret << song;
    }
  }

  return ret;

}

Song ASXParser::ParseTrack(QXmlStreamReader *reader, const QDir &dir) const {

  QString title, artist, album, ref;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();

    switch (type) {
      case QXmlStreamReader::StartElement: {
        QString name = reader->name().toString().toLower();
        if (name == "ref") {
          ref = reader->attributes().value("href").toString();
        }
        else if (name == "title") {
          title = reader->readElementText();
        }
        else if (name == "author") {
          artist = reader->readElementText();
        }
        break;
      }
      case QXmlStreamReader::EndElement: {
        QString name = reader->name().toString().toLower();
        if (name == "entry") {
          goto return_song;
        }
        break;
      }
      default:
        break;
    }
  }

return_song:
  Song song = LoadSong(ref, 0, dir);

  // Override metadata with what was in the playlist
  if (song.source() != Song::Source_Collection) {
    if (!title.isEmpty()) song.set_title(title);
    if (!artist.isEmpty()) song.set_artist(artist);
    if (!album.isEmpty()) song.set_album(album);
  }

  return song;

}

void ASXParser::Save(const SongList &songs, QIODevice *device, const QDir&, Playlist::Path) const {

  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeStartDocument();
  {
    StreamElement asx("asx", &writer);
    writer.writeAttribute("version", "3.0");
    for (const Song &song : songs) {
      StreamElement entry("entry", &writer);
      writer.writeTextElement("title", song.title());
      {
        StreamElement ref("ref", &writer);
        writer.writeAttribute("href", song.url().toString());
      }
      if (!song.artist().isEmpty()) {
        writer.writeTextElement("author", song.artist());
      }
    }
  }
  writer.writeEndDocument();

}

bool ASXParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("<asx");
}
