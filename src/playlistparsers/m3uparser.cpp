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
#include <QDir>
#include <QBuffer>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/settings.h"
#include "constants/timeconstants.h"
#include "constants/playlistsettings.h"
#include "parserbase.h"
#include "m3uparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

M3UParser::M3UParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : ParserBase(tagreader_client, collection_backend, parent) {}

ParserBase::LoadResult M3UParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  M3UType type = M3UType::STANDARD;
  Metadata current_metadata;

  QString data = QString::fromUtf8(device->readAll());
  data.replace(u'\r', u'\n');
  data.replace("\n\n"_L1, "\n"_L1);
  QByteArray bytes = data.toUtf8();
  QBuffer buffer(&bytes);
  if (!buffer.open(QIODevice::ReadOnly)) return SongList();

  QString line = QString::fromUtf8(buffer.readLine()).trimmed();
  if (line.startsWith("#EXTM3U"_L1)) {
    // This is in extended M3U format.
    type = M3UType::EXTENDED;
    line = QString::fromUtf8(buffer.readLine()).trimmed();
  }

  SongList ret;
  Q_FOREVER {
    if (line.startsWith(u'#')) {
      // Extended info or comment.
      if (type == M3UType::EXTENDED && line.startsWith("#EXT"_L1)) {
        if (!ParseMetadata(line, &current_metadata)) {
          qLog(Warning) << "Failed to parse metadata: " << line;
        }
      }
    }
    else if (!line.isEmpty()) {
      Song song = LoadSong(line, 0, 0, dir, collection_lookup);
      if (!current_metadata.title.isEmpty()) {
        song.set_title(current_metadata.title);
      }
      if (!current_metadata.artist.isEmpty()) {
        song.set_artist(current_metadata.artist);
      }
      if (current_metadata.length > 0) {
        song.set_length_nanosec(current_metadata.length);
      }
      ret << song;

      current_metadata = Metadata();
    }
    if (buffer.atEnd()) {
      break;
    }
    line = QString::fromUtf8(buffer.readLine()).trimmed();
  }

  buffer.close();

  return ret;

}

bool M3UParser::ParseMetadata(const QString &line, M3UParser::Metadata *metadata) {

  // Extended info, eg.
  // #EXTINF:123,Sample Artist - Sample title
  QString info = line.section(u':', 1);
  QString l = info.section(u',', 0, 0);
  bool ok = false;
  int length = l.toInt(&ok);
  if (!ok) {
    return false;
  }
  metadata->length = length * kNsecPerSec;

  QString track_info = info.section(u',', 1);
  QStringList list = track_info.split(u" - "_s);
  if (list.size() <= 1) {
    metadata->title = track_info;
    return true;
  }
  metadata->artist = list[0].trimmed();
  metadata->title = list[1].trimmed();
  return true;

}

void M3UParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name)

  device->write("#EXTM3U\n");

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  bool write_metadata = s.value(PlaylistSettings::kWriteMetadata, true).toBool();
  s.endGroup();

  for (const Song &song : songs) {
    if (song.url().isEmpty()) {
      continue;
    }
    if (write_metadata || (song.is_stream() && !song.is_radio())) {
      QString meta = QStringLiteral("#EXTINF:%1,%2 - %3\n").arg(song.length_nanosec() / kNsecPerSec).arg(song.artist(), song.title());
      device->write(meta.toUtf8());
    }
    device->write(URLOrFilename(song.url(), dir, path_type).toUtf8());
    device->write("\n");
  }

}

bool M3UParser::TryMagic(const QByteArray &data) const {
  return data.contains("#EXTM3U") || data.contains("#EXTINF");
}
