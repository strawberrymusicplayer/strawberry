/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CUEPARSER_H
#define CUEPARSER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMap>
#include <QStringList>
#include <QDir>

#include "includes/shared_ptr.h"
#include "constants/playlistsettings.h"
#include "core/song.h"
#include "parserbase.h"

class QIODevice;
class TagReaderClient;
class CollectionBackendInterface;

// This parser will try to detect the real encoding of a .cue file
// but there's a great chance it will fail, so it's probably best to assume that the parser is UTF compatible only.
class CueParser : public ParserBase {
  Q_OBJECT

 public:
  explicit CueParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent = nullptr);

  QString name() const override { return QStringLiteral("CUE"); }
  QStringList file_extensions() const override { return QStringList() << QStringLiteral("cue"); }
  QString mime_type() const override { return QStringLiteral("application/x-cue"); }
  bool load_supported() const override { return true; }
  bool save_supported() const override { return false; }

  bool TryMagic(const QByteArray &data) const override;

  LoadResult Load(QIODevice *device, const QString &playlist_path = QLatin1String(""), const QDir &dir = QDir(), const bool collection_lookup = true) const override;

  // Returns the songs of each media file in the CUE sheet, a CUE sheet can have several FILE sections, mapped by the local filename of the media file.
  QMap<QString, SongList> LoadByFile(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup = true) const;

  // Returns the local filenames of the audio media files in the CUE sheet which have tracks, without reading the media files, which is much faster than loading the songs.
  QStringList MediaFiles(QIODevice *device, const QDir &dir) const;
  void Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir = QDir(), const PlaylistSettings::PathType path_type = PlaylistSettings::PathType::Automatic) const override;

  static QString FindCueFilename(const QString &filename);

 private:
  // A single TRACK entry in .cue file.
  struct CueEntry {
    QString file;

    QString index;

    QString title;
    QString artist;
    QString album_artist;
    QString album;

    QString composer;
    QString album_composer;

    QString genre;
    QString date;
    QString disc;

    QString PrettyArtist() const { return artist.isEmpty() ? album_artist : artist; }
    QString PrettyComposer() const { return composer.isEmpty() ? album_composer : composer; }

    CueEntry(const QString &_file, const QString &_index, const QString &_title, const QString &_artist, const QString &_album_artist, const QString &_album, const QString &_composer, const QString &_album_composer, const QString &_genre, const QString &_date, const QString &_disc) :
    file(_file), index(_index), title(_title), artist(_artist), album_artist(_album_artist), album(_album), composer(_composer), album_composer(_album_composer), genre(_genre), date(_date), disc(_disc) {}
  };

  // Parses the tracks of the CUE sheet, without reading the media files.
  static QList<CueEntry> ParseEntries(QIODevice *device, const QDir &dir);
  // Returns true when the indices of the track are valid, so Load() can return a song for it.
  static bool EntryHasValidIndices(const QList<CueEntry> &entries, const qsizetype i);
  static bool UpdateSong(const CueEntry &entry, const QString &next_index, Song *song);
  static bool UpdateLastSong(const CueEntry &entry, Song *song);

  static QStringList SplitCueLine(const QString &line);
  static qint64 IndexToMarker(const QString &index);
};

#endif  // CUEPARSER_H
