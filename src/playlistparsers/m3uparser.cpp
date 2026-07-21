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
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
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

  // Seed the active-path set with the parent playlist's own canonical path so that a child referencing the parent is detected as a cycle.
  QSet<QString> ancestors;
  if (!playlist_path.isEmpty()) {
    const QString canonical = QFileInfo(playlist_path).canonicalFilePath();
    if (!canonical.isEmpty()) {
      ancestors.insert(canonical);
    }
  }

  // Cache each nested file's fully-expanded tracks by canonical path so a playlist referenced more than once is parsed only once.
  QHash<QString, SongList> expanded;

  SongList ret;
  ParsePlaylistData(device, dir, ancestors, expanded, 0, collection_lookup, ret);
  return ret;

}

bool M3UParser::IsNestedPlaylistReference(const QString &line) {

  // A URL scheme (http, https, file, and so on) marks a stream or remote entry that LoadSong handles as a track, so only local paths are expanded as nested playlists.
  // Reuse the same scheme detection LoadSong applies so the two stay in agreement.
  static const QRegularExpression regex_url_schema(QStringLiteral("^[a-z]{2,}:"), QRegularExpression::CaseInsensitiveOption);
  if (line.contains(regex_url_schema)) {
    return false;
  }

  const QString suffix = QFileInfo(line).suffix();
  return suffix.compare("m3u"_L1, Qt::CaseInsensitive) == 0 || suffix.compare("m3u8"_L1, Qt::CaseInsensitive) == 0;

}

void M3UParser::ParsePlaylistData(QIODevice *device, const QDir &dir, QSet<QString> &ancestors, QHash<QString, SongList> &expanded, const int depth, const bool collection_lookup, SongList &ret) const {

  M3UType type = M3UType::STANDARD;
  Metadata current_metadata;

  QString data = QString::fromUtf8(device->readAll());
  data.replace(u'\r', u'\n');
  data.replace("\n\n"_L1, "\n"_L1);
  QByteArray bytes = data.toUtf8();
  QBuffer buffer(&bytes);
  if (!buffer.open(QIODevice::ReadOnly)) return;

  QString line = QString::fromUtf8(buffer.readLine()).trimmed();
  if (line.startsWith("#EXTM3U"_L1)) {
    // This is in extended M3U format.
    type = M3UType::EXTENDED;
    line = QString::fromUtf8(buffer.readLine()).trimmed();
  }

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
      if (IsNestedPlaylistReference(line)) {
        // Nested playlist reference — expand recursively instead of treating as a track.
        // Discard any preceding #EXTINF because metadata describes tracks, not playlists.
        current_metadata = Metadata();
        LoadNested(line, dir, ancestors, expanded, depth, collection_lookup, ret);
      }
      else {
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
    }
    if (buffer.atEnd()) {
      break;
    }
    line = QString::fromUtf8(buffer.readLine()).trimmed();
  }

  buffer.close();

}

void M3UParser::LoadNested(const QString &filename, const QDir &dir, QSet<QString> &ancestors, QHash<QString, SongList> &expanded, const int depth, const bool collection_lookup, SongList &ret) const {

  if (depth >= kMaxNestingDepth) {
    qLog(Warning) << "Nested playlist depth cap reached, skipping:" << filename;
    return;
  }

  // Resolve the reference path against the current directory.
  QString abs_path = QDir::cleanPath(filename);
  if (!QDir::isAbsolutePath(abs_path)) {
    abs_path = dir.absoluteFilePath(abs_path);
  }

  // Identify the file by its canonical path when it resolves on disk, otherwise by the cleaned absolute path.
  const QString key = QFileInfo(abs_path).canonicalFilePath();
  const QString file_key = key.isEmpty() ? abs_path : key;

  // A reference back to a file already on the active recursion path is a genuine cycle, so skip it.
  if (ancestors.contains(file_key)) {
    qLog(Warning) << "Nested playlist cycle detected, skipping:" << abs_path;
    return;
  }

  // A file already expanded on another branch is a repeated or diamond reference, not a cycle, so reuse its tracks instead of parsing it again.
  // The remaining depth budget bounds how deep this expansion may go, so it is part of the cache key: a result truncated under a tighter budget must not be reused where more budget remains.
  const QString cache_key = file_key + u':' + QString::number(kMaxNestingDepth - depth);
  const QHash<QString, SongList>::const_iterator cached = expanded.constFind(cache_key);
  if (cached != expanded.constEnd()) {
    ret.append(cached.value());
    return;
  }

  // Missing or unreadable references are skipped with a warning so sibling entries still load.
  QFile nested_file(abs_path);
  if (!nested_file.open(QIODevice::ReadOnly)) {
    qLog(Warning) << "Could not open nested playlist, skipping:" << abs_path;
    return;
  }

  // Mark the file as active on the recursion path before descending so a cycle within its subtree is detected.
  ancestors.insert(file_key);

  // A nested file's relative entries resolve against its own directory, and references found inside it descend one level deeper against the depth cap.
  const QDir nested_dir = QFileInfo(abs_path).dir();
  SongList nested_songs;
  ParsePlaylistData(&nested_file, nested_dir, ancestors, expanded, depth + 1, collection_lookup, nested_songs);

  nested_file.close();

  // Leaving the active path and caching the result lets a later sibling or diamond reference reuse it without being mistaken for a cycle.
  ancestors.remove(file_key);
  expanded.insert(cache_key, nested_songs);
  ret.append(nested_songs);

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
  bool write_metadata = s.value(PlaylistSettings::kWriteMetadata, PlaylistSettings::kDefaultWriteMetadata).toBool();
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
