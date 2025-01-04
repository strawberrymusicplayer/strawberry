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

#include "includes/shared_ptr.h"
#include "constants/playlistsettings.h"
#include "parserbase.h"
#include "asxiniparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

AsxIniParser::AsxIniParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : ParserBase(tagreader_client, collection_backend, parent) {}

bool AsxIniParser::TryMagic(const QByteArray &data) const {
  return data.toLower().contains("[reference]");
}

ParserBase::LoadResult AsxIniParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  Q_UNUSED(playlist_path);

  SongList ret;

  while (!device->atEnd()) {
    QString line = QString::fromUtf8(device->readLine()).trimmed();
    qint64 equals = line.indexOf(u'=');
    QString key = line.left(equals).toLower();
    QString value = line.mid(equals + 1);

    if (key.startsWith("ref"_L1)) {
      Song song = LoadSong(value, 0, 0, dir, collection_lookup);
      if (song.is_valid()) {
        ret << song;
      }
    }
  }

  return ret;

}

void AsxIniParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name)

  QTextStream s(device);
  s << "[Reference]" << Qt::endl;

  int n = 1;
  for (const Song &song : songs) {
    s << "Ref" << n << "=" << URLOrFilename(song.url(), dir, path_type) << Qt::endl;
    ++n;
  }

}
