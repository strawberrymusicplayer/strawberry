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
#include <QMap>
#include <QByteArray>
#include <QString>
#include <QRegularExpression>
#include <QTextStream>

#include "includes/shared_ptr.h"
#include "constants/timeconstants.h"
#include "constants/playlistsettings.h"
#include "parserbase.h"
#include "plsparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

PLSParser::PLSParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : ParserBase(tagreader_client, collection_backend, parent) {}

ParserBase::LoadResult PLSParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  QMap<int, Song> songs;
  static const QRegularExpression n_re(u"\\d+$"_s);

  while (!device->atEnd()) {
    QString line = QString::fromUtf8(device->readLine()).trimmed();
    qint64 equals = line.indexOf(u'=');
    QString key = line.left(equals).toLower();
    QString value = line.mid(equals + 1);

    QRegularExpressionMatch re_match = n_re.match(key);
    int n = re_match.captured(0).toInt();

    if (key.startsWith("file"_L1)) {
      Song song = LoadSong(value, 0, 0, dir, collection_lookup);

      // Use the title and length we've already loaded if any
      if (!songs[n].title().isEmpty()) song.set_title(songs[n].title());
      if (songs[n].length_nanosec() != -1) {
        song.set_length_nanosec(songs[n].length_nanosec());
      }

      songs[n] = song;
    }
    else if (key.startsWith("title"_L1)) {
      songs[n].set_title(value);
    }
    else if (key.startsWith("length"_L1)) {
      qint64 seconds = value.toLongLong();
      if (seconds > 0) {
        songs[n].set_length_nanosec(seconds * kNsecPerSec);
      }
    }
  }

  return songs.values();

}

void PLSParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name)

  QTextStream s(device);
  s << "[playlist]" << Qt::endl;
  s << "Version=2" << Qt::endl;
  s << "NumberOfEntries=" << songs.count() << Qt::endl;

  int n = 1;
  for (const Song &song : songs) {
    s << "File" << n << "=" << URLOrFilename(song.url(), dir, path_type) << Qt::endl;
    s << "Title" << n << "=" << song.title() << Qt::endl;
    s << "Length" << n << "=" << song.length_nanosec() / kNsecPerSec << Qt::endl;
    ++n;
  }

}

bool PLSParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("[playlist]");
}
