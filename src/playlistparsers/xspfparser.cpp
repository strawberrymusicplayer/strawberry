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

class CollectionBackendInterface;

XSPFParser::XSPFParser(SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : XMLParser(collection_backend, parent) {}

SongList XSPFParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_search) const {

  Q_UNUSED(playlist_path);

  SongList ret;

  QXmlStreamReader reader(device);
  if (!Utilities::ParseUntilElement(&reader, QStringLiteral("playlist")) || !Utilities::ParseUntilElement(&reader, QStringLiteral("trackList"))) {
    return ret;
  }

  while (!reader.atEnd() && Utilities::ParseUntilElement(&reader, QStringLiteral("track"))) {
    Song song = ParseTrack(&reader, dir, collection_search);
    if (song.is_valid()) {
      ret << song;
    }
  }
  return ret;

}

Song XSPFParser::ParseTrack(QXmlStreamReader *reader, const QDir &dir, const bool collection_search) const {

  QString title, artist, album, location, art;
  qint64 nanosec = -1;
  int track_num = -1;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();
    switch (type) {
      case QXmlStreamReader::StartElement:{
        if (name == QStringLiteral("location")) {
          location = QUrl::fromPercentEncoding(reader->readElementText().toUtf8());
        }
        else if (name == QStringLiteral("title")) {
          title = reader->readElementText();
        }
        else if (name == QStringLiteral("creator")) {
          artist = reader->readElementText();
        }
        else if (name == QStringLiteral("album")) {
          album = reader->readElementText();
        }
        else if (name == QStringLiteral("image")) {
          art = QUrl::fromPercentEncoding(reader->readElementText().toUtf8());
        }
        else if (name == QStringLiteral("duration")) {  // in milliseconds.
          const QString duration = reader->readElementText();
          bool ok = false;
          nanosec = duration.toInt(&ok) * kNsecPerMsec;
          if (!ok) {
            nanosec = -1;
          }
        }
        else if (name == QStringLiteral("trackNum")) {
          const QString track_num_str = reader->readElementText();
          bool ok = false;
          track_num = track_num_str.toInt(&ok);
          if (!ok || track_num < 1) {
            track_num = -1;
          }
        }
        else if (name == QStringLiteral("info")) {
          // TODO: Do something with extra info?
        }
        break;
      }
      case QXmlStreamReader::EndElement:{
        if (name == QStringLiteral("track")) {
          goto return_song;
        }
      }
      default:
        break;
    }
  }

return_song:
  Song song = LoadSong(location, 0, track_num, dir, collection_search);

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
  writer.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));
  writer.writeDefaultNamespace(QStringLiteral("http://xspf.org/ns/0/"));

  Settings s;
  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  bool write_metadata = s.value("write_metadata", true).toBool();
  s.endGroup();

  StreamElement tracklist(QStringLiteral("trackList"), &writer);
  for (const Song &song : songs) {
    QString filename_or_url = QString::fromLatin1(QUrl::toPercentEncoding(URLOrFilename(song.url(), dir, path_type), "/ "));

    StreamElement track(QStringLiteral("track"), &writer);
    writer.writeTextElement(QStringLiteral("location"), filename_or_url);

    if (write_metadata || (song.is_stream() && !song.is_radio())) {
      writer.writeTextElement(QStringLiteral("title"), song.title());
      if (!song.artist().isEmpty()) {
        writer.writeTextElement(QStringLiteral("creator"), song.artist());
      }
      if (!song.album().isEmpty()) {
        writer.writeTextElement(QStringLiteral("album"), song.album());
      }
      if (song.length_nanosec() != -1) {
        writer.writeTextElement(QStringLiteral("duration"), QString::number(song.length_nanosec() / kNsecPerMsec));
      }
    }

    if ((write_metadata || song.has_cue() || (song.is_stream() && !song.is_radio())) && song.track() > 0) {
      writer.writeTextElement(QStringLiteral("trackNum"), QString::number(song.track()));
    }

    if (write_metadata || (song.is_stream() && !song.is_radio())) {
      const QUrl cover_url = song.art_manual().isEmpty() || !song.art_manual().isValid() ? song.art_automatic() : song.art_manual();
      // Ignore images that are in our resource bundle.
      if (!cover_url.isEmpty() && cover_url.isValid()) {
        const QString cover_filename = QString::fromLatin1(QUrl::toPercentEncoding(URLOrFilename(cover_url, dir, path_type), "/ "));
        writer.writeTextElement(QStringLiteral("image"), cover_filename);
      }
    }
  }

  writer.writeEndDocument();

}

bool XSPFParser::TryMagic(const QByteArray &data) const {
  return data.contains("<playlist") && data.contains("<trackList");
}
