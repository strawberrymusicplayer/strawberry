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
#include <QByteArray>
#include <QDir>
#include <QString>
#include <QTextStream>

#include "core/shared_ptr.h"
#include "settings/playlistsettingspage.h"
#include "parserbase.h"
#include "asxiniparser.h"

class CollectionBackendInterface;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
constexpr auto qt_endl = Qt::endl;
#else
constexpr auto qt_endl = endl;
#endif

AsxIniParser::AsxIniParser(SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : ParserBase(collection_backend, parent) {}

bool AsxIniParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("[reference]");
}

SongList AsxIniParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_search) const {

  Q_UNUSED(playlist_path);

  SongList ret;

  while (!device->atEnd()) {
    QString line = QString::fromUtf8(device->readLine()).trimmed();
    qint64 equals = line.indexOf('=');
    QString key = line.left(equals).toLower();
    QString value = line.mid(equals + 1);

    if (key.startsWith("ref")) {
      Song song = LoadSong(value, 0, dir, collection_search);
      if (song.is_valid()) {
        ret << song;
      }
    }
  }

  return ret;

}

void AsxIniParser::Save(const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettingsPage::PathType path_type) const {

  QTextStream s(device);
  s << "[Reference]" << qt_endl;

  int n = 1;
  for (const Song &song : songs) {
    s << "Ref" << n << "=" << URLOrFilename(song.url(), dir, path_type) << qt_endl;
    ++n;
  }

}
