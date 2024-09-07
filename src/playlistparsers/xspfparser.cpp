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
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QSettings>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "core/shared_ptr.h"
#include "core/settings.h"
#include "utilities/xmlutils.h"
#include "utilities/timeconstants.h"
#include "settings/playlistsettingspage.h"
#include "xmlparser.h"
#include "xspfparser.h"

using namespace Qt::StringLiterals;

class CollectionBackendInterface;

XSPFParser::XSPFParser(SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : XMLParser(collection_backend, parent) {}

SongList XSPFParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  SongList songs;

  QXmlStreamReader reader(device);
  if (!Utilities::ParseUntilElement(&reader, QStringLiteral("playlist")) || !Utilities::ParseUntilElement(&reader, QStringLiteral("trackList"))) {
    return songs;
  }

  while (!reader.atEnd() && Utilities::ParseUntilElement(&reader, QStringLiteral("track"))) {
    const Song song = ParseTrack(&reader, dir, collection_lookup);
    if (song.is_valid()) {
      songs << song;
    }
  }

  return songs;

}

Song XSPFParser::ParseTrack(QXmlStreamReader *reader, const QDir &dir, const bool collection_lookup) const {

  QString title, artist, album, location, art;
  qint64 nanosec = -1;
  int track_num = -1;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();
    switch (type) {
      case QXmlStreamReader::StartElement:{
        if (name == "location"_L1) {
          location = QUrl::fromPercentEncoding(reader->readElementText().toUtf8());
        }
        else if (name == "title"_L1) {
          title = reader->readElementText();
        }
        else if (name == "creator"_L1) {
          artist = reader->readElementText();
        }
        else if (name == "album"_L1) {
          album = reader->readElementText();
        }
        else if (name == "image"_L1) {
          art = QUrl::fromPercentEncoding(reader->readElementText().toUtf8());
        }
        else if (name == "duration"_L1) {  // in milliseconds.
          const QString duration = reader->readElementText();
          bool ok = false;
          nanosec = duration.toInt(&ok) * kNsecPerMsec;
          if (!ok) {
            nanosec = -1;
          }
        }
        else if (name == "trackNum"_L1) {
          const QString track_num_str = reader->readElementText();
          bool ok = false;
          track_num = track_num_str.toInt(&ok);
          if (!ok || track_num < 1) {
            track_num = -1;
          }
        }
        else if (name == "info"_L1) {
          // TODO: Do something with extra info?
        }
        break;
      }
      case QXmlStreamReader::EndElement:{
        if (name == "track"_L1) {
          goto return_song;
        }
      }
      default:
        break;
    }
  }

return_song:
  Song song = LoadSong(location, 0, track_num, dir, collection_lookup);

  // Override metadata with what was in the playlist
  if (song.source() != Song::Source::Collection) {
    if (!title.isEmpty()) song.set_title(title);
    if (!artist.isEmpty()) song.set_artist(artist);
    if (!album.isEmpty()) song.set_album(album);
    if (!art.isEmpty()) song.set_art_manual(QUrl(art));
    if (nanosec > 0) song.set_length_nanosec(nanosec);
    if (track_num > 0) song.set_track(track_num);
  }

  return song;

}

void XSPFParser::Save(const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettingsPage::PathType path_type) const {

  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeStartDocument();
  StreamElement playlist(QStringLiteral("playlist"), &writer);
  writer.writeAttribute("version"_L1, "1"_L1);
  writer.writeDefaultNamespace("http://xspf.org/ns/0/"_L1);

  Settings s;
  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  bool write_metadata = s.value("write_metadata", true).toBool();
  s.endGroup();

  StreamElement tracklist(QStringLiteral("trackList"), &writer);
  for (const Song &song : songs) {
    QString filename_or_url = QString::fromLatin1(QUrl::toPercentEncoding(URLOrFilename(song.url(), dir, path_type), "/ "));

    StreamElement track(QStringLiteral("track"), &writer);
    writer.writeTextElement("location"_L1, filename_or_url);

    if (write_metadata || (song.is_stream() && !song.is_radio())) {
      writer.writeTextElement("title"_L1, song.title());
      if (!song.artist().isEmpty()) {
        writer.writeTextElement("creator"_L1, song.artist());
      }
      if (!song.album().isEmpty()) {
        writer.writeTextElement("album"_L1, song.album());
      }
      if (song.length_nanosec() != -1) {
        writer.writeTextElement("duration"_L1, QString::number(song.length_nanosec() / kNsecPerMsec));
      }
    }

    if ((write_metadata || song.has_cue() || (song.is_stream() && !song.is_radio())) && song.track() > 0) {
      writer.writeTextElement("trackNum"_L1, QString::number(song.track()));
    }

    if (write_metadata || (song.is_stream() && !song.is_radio())) {
      const QUrl cover_url = song.art_manual().isEmpty() || !song.art_manual().isValid() ? song.art_automatic() : song.art_manual();
      // Ignore images that are in our resource bundle.
      if (!cover_url.isEmpty() && cover_url.isValid()) {
        const QString cover_filename = QString::fromLatin1(QUrl::toPercentEncoding(URLOrFilename(cover_url, dir, path_type), "/ "));
        writer.writeTextElement("image"_L1, cover_filename);
      }
    }
  }

  writer.writeEndDocument();

}

bool XSPFParser::TryMagic(const QByteArray &data) const {
  return data.contains("<playlist") && data.contains("<trackList");
}
