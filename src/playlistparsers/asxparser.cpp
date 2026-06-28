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
#include <QBuffer>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "utilities/xmlutils.h"
#include "constants/playlistsettings.h"
#include "xmlparser.h"
#include "asxparser.h"

using namespace Qt::Literals::StringLiterals;

namespace {
// A real ASX playlist is tiny; we load the whole file into memory and run regex rewrites over it, so cap the size to avoid a memory-exhaustion DoS from a hostile file.
constexpr qint64 kMaxFileSize = 50 * 1024 * 1024;
}  // namespace

class CollectionBackendInterface;

ASXParser::ASXParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : XMLParser(tagreader_client, collection_backend, parent) {}

ParserBase::LoadResult ASXParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  // QIODevice::size() is unreliable for sequential devices, so bound the actual read instead of trusting it: request one byte past the limit and treat any overflow as too large.
  // This caps memory use even when size() under-reports, and the failure is reported via Error() so it is not mistaken for a valid empty playlist.
  // We have to load everything first anyway, so we can munge the "XML".
  QByteArray data = device->read(kMaxFileSize + 1);
  if (data.size() > kMaxFileSize) {
    qLog(Error) << "ASX playlist is too large; refusing to load more than" << kMaxFileSize << "bytes";
    Q_EMIT Error(tr("ASX playlist is too large"));
    return SongList();
  }

  // Some playlists have unescaped & characters in URLs :(
  static const QRegularExpression ex(u"(href\\s*=\\s*\")([^\"]+)\""_s, QRegularExpression::CaseInsensitiveOption);
  qint64 index = 0;
  for (QRegularExpressionMatch re_match = ex.match(QString::fromUtf8(data), index); re_match.hasMatch(); re_match = ex.match(QString::fromUtf8(data), index)) {
    index = re_match.capturedStart();
    QString url = re_match.captured(2);
    static const QRegularExpression regex_html_entities(u"&(?!amp;|quot;|apos;|lt;|gt;)"_s);
    url.replace(regex_html_entities, u"&amp;"_s);

    QByteArray replacement = QStringLiteral("%1%2\"").arg(re_match.captured(1), url).toLocal8Bit();
    data.replace(re_match.captured(0).toLocal8Bit(), replacement);
    index += replacement.length();
  }

  QBuffer buffer(&data);
  if (!buffer.open(QIODevice::ReadOnly)) return SongList();

  QXmlStreamReader reader(&buffer);
  if (!Utilities::ParseUntilElementCI(&reader, u"asx"_s)) {
    buffer.close();
    return SongList();
  }

  SongList ret;
  while (!reader.atEnd() && Utilities::ParseUntilElementCI(&reader, u"entry"_s)) {
    Song song = ParseTrack(&reader, dir, collection_lookup);
    if (song.is_valid()) {
      ret << song;
    }
  }

  buffer.close();

  return ret;

}

Song ASXParser::ParseTrack(QXmlStreamReader *reader, const QDir &dir, const bool collection_lookup) const {

  QString title, artist, ref;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();

    switch (type) {
      case QXmlStreamReader::StartElement:{
        const QString name = reader->name().toString().toLower();
        if (name == "ref"_L1) {
          ref = reader->attributes().value("href"_L1).toString();
        }
        else if (name == "title"_L1) {
          title = reader->readElementText();
        }
        else if (name == "author"_L1) {
          artist = reader->readElementText();
        }
        break;
      }
      case QXmlStreamReader::EndElement:{
        const QString name = reader->name().toString().toLower();
        if (name == "entry"_L1) {
          goto return_song;
        }
        break;
      }
      default:
        break;
    }
  }

return_song:
  Song song = LoadSong(ref, 0, 0, dir, collection_lookup);

  // Override metadata with what was in the playlist
  if (song.source() != Song::Source::Collection) {
    if (!title.isEmpty()) song.set_title(title);
    if (!artist.isEmpty()) song.set_artist(artist);
  }

  return song;

}

void ASXParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name)
  Q_UNUSED(dir)
  Q_UNUSED(path_type)

  QXmlStreamWriter writer(device);
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(2);
  writer.writeStartDocument();
  {
    StreamElement asx(u"asx"_s, &writer);
    writer.writeAttribute("version"_L1, "3.0"_L1);
    for (const Song &song : songs) {
      StreamElement entry(u"entry"_s, &writer);
      writer.writeTextElement("title"_L1, song.title());
      {
        StreamElement ref(u"ref"_s, &writer);
        writer.writeAttribute("href"_L1, song.url().toString());
      }
      if (!song.artist().isEmpty()) {
        writer.writeTextElement("author"_L1, song.artist());
      }
    }
  }
  writer.writeEndDocument();

  if (writer.hasError()) {
    qLog(Error) << "Error writing ASX playlist to device";
    Q_EMIT Error(tr("Failed to write ASX playlist"));
  }

}

bool ASXParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("<asx");
}
