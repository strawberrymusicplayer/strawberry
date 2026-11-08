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

#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QMap>
#include <QByteArray>
#include <QString>
#include <QRegularExpression>
#include <QTextStream>

#include "includes/shared_ptr.h"
#include "constants/timeconstants.h"
#include "constants/playlistsettings.h"
#include "utilities/textencodingutils.h"
#include "parserbase.h"
#include "plsparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

PLSParser::PLSParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : ParserBase(tagreader_client, collection_backend, parent) {}

ParserBase::LoadResult PLSParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  // The keys of an entry can be in any order, so the songs are loaded after all of them are read.
  struct Entry {
    QString file;
    QString title;
    qint64 length_nanosec = -1;
    int track = 0;
  };
  QMap<int, Entry> entries;
  static const QRegularExpression n_re(u"\\d+$"_s);

  QString data = Utilities::TextFromData(device->readAll());
  QTextStream text_stream(&data, QIODevice::ReadOnly);
  while (!text_stream.atEnd()) {
    QString line = text_stream.readLine().trimmed();
    qint64 equals = line.indexOf(u'=');
    if (equals < 0) continue;
    QString key = line.left(equals).toLower();
    QString value = line.mid(equals + 1);

    QRegularExpressionMatch re_match = n_re.match(key);
    int n = re_match.captured(0).toInt();

    if (key.startsWith("file"_L1)) {
      entries[n].file = value;
    }
    else if (key.startsWith("track"_L1)) {
      entries[n].track = value.toInt();
    }
    else if (key.startsWith("title"_L1)) {
      entries[n].title = value;
    }
    else if (key.startsWith("length"_L1)) {
      qint64 seconds = value.toLongLong();
      if (seconds > 0) {
        entries[n].length_nanosec = seconds * kNsecPerSec;
      }
    }
  }

  SongList ret;
  for (const Entry &entry : std::as_const(entries)) {
    // Title and length lines also add an entry, only the entries with a file are songs.
    if (entry.file.isEmpty()) continue;
    Song song = LoadSong(entry.file, 0, entry.track, dir, collection_lookup);
    if (!entry.title.isEmpty()) song.set_title(entry.title);
    if (entry.length_nanosec != -1) song.set_length_nanosec(entry.length_nanosec);
    ret << song;
  }

  return ret;

}

void PLSParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name)

  QTextStream s(device);
  s << "[playlist]" << Qt::endl;
  s << "Version=2" << Qt::endl;
  s << "NumberOfEntries=" << songs.count() << Qt::endl;

  int n = 1;
  for (const Song &song : songs) {
    if (CueTrack(song) > 0) s << "Track" << n << "=" << CueTrack(song) << Qt::endl;
    s << "File" << n << "=" << URLOrFilename(song.url(), dir, path_type) << Qt::endl;
    s << "Title" << n << "=" << song.title() << Qt::endl;
    s << "Length" << n << "=" << song.length_nanosec() / kNsecPerSec << Qt::endl;
    ++n;
  }

}

bool PLSParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("[playlist]");
}
