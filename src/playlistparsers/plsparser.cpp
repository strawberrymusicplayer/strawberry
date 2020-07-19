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
#include <QRegularExpressionMatch>
#include <QTextStream>

#include "core/timeconstants.h"
#include "playlistparsers/parserbase.h"
#include "plsparser.h"

class CollectionBackendInterface;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#  define qt_endl Qt::endl
#else
#  define qt_endl endl
#endif

PLSParser::PLSParser(CollectionBackendInterface *collection, QObject *parent)
    : ParserBase(collection, parent) {}

SongList PLSParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir) const {

  Q_UNUSED(playlist_path);

  QMap<int, Song> songs;
  QRegularExpression n_re("\\d+$");

  while (!device->atEnd()) {
    QString line = QString::fromUtf8(device->readLine()).trimmed();
    int equals = line.indexOf('=');
    QString key = line.left(equals).toLower();
    QString value = line.mid(equals + 1);

    QRegularExpressionMatch re_match = n_re.match(key);
    int n = re_match.captured(0).toInt();

    if (key.startsWith("file")) {
      Song song = LoadSong(value, 0, dir);

      // Use the title and length we've already loaded if any
      if (!songs[n].title().isEmpty()) song.set_title(songs[n].title());
      if (songs[n].length_nanosec() != -1)
        song.set_length_nanosec(songs[n].length_nanosec());

      songs[n] = song;
    }
    else if (key.startsWith("title")) {
      songs[n].set_title(value);
    }
    else if (key.startsWith("length")) {
      qint64 seconds = value.toLongLong();
      if (seconds > 0) {
        songs[n].set_length_nanosec(seconds * kNsecPerSec);
      }
    }
  }

  return songs.values();

}

void PLSParser::Save(const SongList &songs, QIODevice *device, const QDir &dir, Playlist::Path path_type) const {

  QTextStream s(device);
  s << "[playlist]" << qt_endl;
  s << "Version=2" << qt_endl;
  s << "NumberOfEntries=" << songs.count() << qt_endl;

  int n = 1;
  for (const Song &song : songs) {
    s << "File" << n << "=" << URLOrFilename(song.url(), dir, path_type) << qt_endl;
    s << "Title" << n << "=" << song.title() << qt_endl;
    s << "Length" << n << "=" << song.length_nanosec() / kNsecPerSec << qt_endl;
    ++n;
  }

}

bool PLSParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("[playlist]");
}
