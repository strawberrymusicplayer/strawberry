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

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSettings>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "core/timeconstants.h"
#include "core/utilities.h"
#include "playlist/playlist.h"
#include "playlistparsers/xmlparser.h"
#include "xspfparser.h"

class CollectionBackendInterface;

XSPFParser::XSPFParser(CollectionBackendInterface *collection, QObject *parent)
    : XMLParser(collection, parent) {}

SongList XSPFParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir) const {

  Q_UNUSED(playlist_path);

  SongList ret;

  QXmlStreamReader reader(device);
  if (!Utilities::ParseUntilElement(&reader, "playlist") || !Utilities::ParseUntilElement(&reader, "trackList")) {
    return ret;
  }

  while (!reader.atEnd() && Utilities::ParseUntilElement(&reader, "track")) {
    Song song = ParseTrack(&reader, dir);
    if (song.is_valid()) {
      ret << song;
    }
  }
  return ret;

}

Song XSPFParser::ParseTrack(QXmlStreamReader *reader, const QDir &dir) const {

  QString title, artist, album, location;
  qint64 nanosec = -1;
  int track_num = -1;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    switch (type) {
      case QXmlStreamReader::StartElement: {
        QStringRef name = reader->name();
        if (name == "location") {
          location = reader->readElementText();
        }
        else if (name == "title") {
          title = reader->readElementText();
        }
        else if (name == "creator") {
          artist = reader->readElementText();
        }
        else if (name == "album") {
          album = reader->readElementText();
        }
        else if (name == "duration") {  // in milliseconds.
          const QString duration = reader->readElementText();
          bool ok = false;
          nanosec = duration.toInt(&ok) * kNsecPerMsec;
          if (!ok) {
            nanosec = -1;
          }
        }
        else if (name == "trackNum") {
          const QString track_num_str = reader->readElementText();
          bool ok = false;
          track_num = track_num_str.toInt(&ok);
          if (!ok || track_num < 1) {
            track_num = -1;
          }
        }
        else if (name == "image") {
          // TODO: Fetch album covers.
        }
        else if (name == "info") {
          // TODO: Do something with extra info?
        }
        break;
      }
      case QXmlStreamReader::EndElement: {
        if (reader->name() == "track") {
          goto return_song;
        }
      }
      default:
        break;
    }
  }

return_song:
  Song song = LoadSong(location, 0, dir);

  // Override metadata with what was in the playlist
  if (song.source() != Song::Source_Collection) {
    if (!title.isEmpty()) song.set_title(title);
    if (!artist.isEmpty()) song.set_artist(artist);
    if (!album.isEmpty()) song.set_album(album);
    if (nanosec > 0) song.set_length_nanosec(nanosec);
    if (track_num > 0) song.set_track(track_num);
  }

  return song;

}

void XSPFParser::Save(const SongList &songs, QIODevice *device, const QDir &dir, Playlist::Path path_type) const {

  QFileInfo file;
  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeStartDocument();
  StreamElement playlist("playlist", &writer);
  writer.writeAttribute("version", "1");
  writer.writeDefaultNamespace("http://xspf.org/ns/0/");

  QSettings s;
  s.beginGroup(Playlist::kSettingsGroup);
  bool writeMetadata = s.value(Playlist::kWriteMetadata, true).toBool();
  s.endGroup();

  StreamElement tracklist("trackList", &writer);
  for (const Song &song : songs) {
    QString filename_or_url = URLOrFilename(song.url(), dir, path_type).toUtf8();

    StreamElement track("track", &writer);
    writer.writeTextElement("location", filename_or_url);

    if (writeMetadata) {
      writer.writeTextElement("title", song.title());
      if (!song.artist().isEmpty()) {
        writer.writeTextElement("creator", song.artist());
      }
      if (!song.album().isEmpty()) {
        writer.writeTextElement("album", song.album());
      }
      if (song.length_nanosec() != -1) {
        writer.writeTextElement("duration", QString::number(song.length_nanosec() / kNsecPerMsec));
      }
      if (song.track() > 0) {
        writer.writeTextElement("trackNum", QString::number(song.track()));
      }

      QUrl cover_url = song.art_manual().isEmpty() || song.art_manual().path().isEmpty() ? song.art_automatic() : song.art_manual();
      // Ignore images that are in our resource bundle.
      if (!cover_url.isEmpty() && !cover_url.path().isEmpty() && cover_url.path() != Song::kManuallyUnsetCover && cover_url.path() != Song::kEmbeddedCover) {
        if (cover_url.scheme().isEmpty()) {
          cover_url.setScheme("file");
        }
        QString cover_filename = URLOrFilename(cover_url, dir, path_type).toUtf8();
        writer.writeTextElement("image", cover_filename);
      }
    }
  }

  writer.writeEndDocument();

}

bool XSPFParser::TryMagic(const QByteArray &data) const {
  return data.contains("<playlist") && data.contains("<trackList");
}
