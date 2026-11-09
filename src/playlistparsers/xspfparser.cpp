/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QRegularExpression>
#include <QUrl>
#include <QSettings>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/settings.h"
#include "utilities/xmlutils.h"
#include "constants/timeconstants.h"
#include "constants/playlistsettings.h"
#include "xmlparser.h"
#include "xspfparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

namespace {

bool HasUrlScheme(const QString &location) {

  static const QRegularExpression regex_url_schema(u"^[a-z]{2,}:"_s, QRegularExpression::CaseInsensitiveOption);
  return location.contains(regex_url_schema);

}

}  // namespace

XSPFParser::XSPFParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : XMLParser(tagreader_client, collection_backend, parent) {}

ParserBase::LoadResult XSPFParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  QString playlist_name;
  {
    QXmlStreamReader reader(device);
    if (Utilities::ParseUntilElement(&reader, u"playlist"_s)) {
      while (reader.readNextStartElement()) {
        if (reader.name() == "title"_L1) {
          playlist_name = reader.readElementText();
          break;
        }
        reader.skipCurrentElement();
      }
    }
  }

  device->seek(0);
  QXmlStreamReader reader(device);
  if (!Utilities::ParseUntilElement(&reader, u"playlist"_s)) {
    return LoadResult();
  }
  if (!Utilities::ParseUntilElement(&reader, u"trackList"_s)) {
    return LoadResult();
  }
  SongList songs;
  while (!reader.atEnd() && Utilities::ParseUntilElement(&reader, u"track"_s)) {
    const Song song = ParseTrack(&reader, dir, collection_lookup);
    if (song.is_valid()) {
      songs << song;
    }
  }

  return LoadResult(songs, playlist_name);

}

QString XSPFParser::EncodeLocation(const QUrl &url, const QDir &dir, const PlaylistSettings::PathType path_type) {

  if (!url.isLocalFile()) return QString::fromLatin1(url.toEncoded());

  const QString location = URLOrFilename(url, dir, path_type);
  if (QDir::isAbsolutePath(location)) return QString::fromLatin1(url.toEncoded());

  // A colon must also be encoded here, or the first part of the path would be read as a URL scheme.
  const QString encoded_location = QString::fromLatin1(QUrl::toPercentEncoding(location, "/"));

  // Locations saved by older versions can be encoded URLs, so a path looking like a URL also needs a path in front of it, to be loaded as a file when the file is missing.
  return HasUrlScheme(location) ? "./"_L1 + encoded_location : encoded_location;

}

QString XSPFParser::DecodeLocation(const QString &location, const QDir &dir) {

  // A URL is kept encoded when it's loaded, decoding it here would change what an encoded character like %2F or %23 means.
  if (HasUrlScheme(location)) return location;

  const QString decoded_location = QUrl::fromPercentEncoding(location.toUtf8());

  // Playlists saved by older versions have the whole URL encoded, so a decoded location can be a URL, but it can also be a file name containing a colon.
  if (HasUrlScheme(decoded_location) && QFileInfo::exists(dir.absoluteFilePath(decoded_location))) {
    return "./"_L1 + decoded_location;
  }

  return decoded_location;

}

Song XSPFParser::ParseTrack(QXmlStreamReader *reader, const QDir &dir, const bool collection_lookup) const {

  QString platform, location, title, artist, album, art;
  qint64 nanosec = -1;
  int track_num = -1;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();
    switch (type) {
      case QXmlStreamReader::StartElement:{
        if (name == "platform"_L1) {
          platform = reader->readElementText().toLower();
        }
        else if (name == "location"_L1 || name == "url"_L1) {
          location = DecodeLocation(reader->readElementText(), dir);
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
          art = DecodeLocation(reader->readElementText(), dir);
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
        break;
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
    if (!art.isEmpty()) {
      // Local images are saved as filenames, which can be relative to the playlist, and a URL without a scheme is never loaded.
      song.set_art_manual(HasUrlScheme(art) ? QUrl(art) : QUrl::fromLocalFile(QDir::cleanPath(dir.absoluteFilePath(art))));
    }
    if (nanosec > 0) song.set_length_nanosec(nanosec);
    if (track_num > 0) song.set_track(track_num);
  }

  return song;

}

void XSPFParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeStartDocument();
  StreamElement playlist(u"playlist"_s, &writer);
  writer.writeAttribute("version"_L1, "1"_L1);
  writer.writeDefaultNamespace("http://xspf.org/ns/0/"_L1);

  writer.writeTextElement("title"_L1, playlist_name);

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  bool write_metadata = s.value(PlaylistSettings::kWriteMetadata, PlaylistSettings::kDefaultWriteMetadata).toBool();
  s.endGroup();

  StreamElement tracklist(u"trackList"_s, &writer);
  for (const Song &song : songs) {
    const QString filename_or_url = EncodeLocation(song.url(), dir, path_type);

    StreamElement track(u"track"_s, &writer);
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
        const QString cover_filename = EncodeLocation(cover_url, dir, path_type);
        writer.writeTextElement("image"_L1, cover_filename);
      }
    }
  }

  writer.writeEndDocument();

  if (writer.hasError()) {
    qLog(Error) << "Error writing XSPF playlist to device";
    Q_EMIT Error(tr("Failed to write XSPF playlist"));
  }

}

bool XSPFParser::TryMagic(const QByteArray &data) const {
  return data.contains("<playlist") && data.contains("<trackList");
}
