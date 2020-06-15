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

#ifndef CUEPARSER_H
#define CUEPARSER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDir>

#include "core/song.h"
#include "parserbase.h"
#include "playlist/playlist.h"

class QIODevice;
class CollectionBackendInterface;

// This parser will try to detect the real encoding of a .cue file
// but there's a great chance it will fail so it's probably best to assume that the parser is UTF compatible only.
class CueParser : public ParserBase {
  Q_OBJECT

 public:
  static const char *kFileLineRegExp;
  static const char *kIndexRegExp;

  static const char *kPerformer;
  static const char *kTitle;
  static const char *kSongWriter;
  static const char *kFile;
  static const char *kTrack;
  static const char *kIndex;
  static const char *kAudioTrackType;
  static const char *kRem;
  static const char *kGenre;
  static const char *kDate;
  static const char *kDisc;

  explicit CueParser(CollectionBackendInterface *collection, QObject *parent = nullptr);

  QString name() const override { return "CUE"; }
  QStringList file_extensions() const override { return QStringList() << "cue"; }
  QString mime_type() const override { return "application/x-cue"; }

  bool TryMagic(const QByteArray &data) const override;

  SongList Load(QIODevice *device, const QString &playlist_path = "", const QDir &dir = QDir()) const override;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir = QDir(), Playlist::Path path_type = Playlist::Path_Automatic) const override;

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

  bool UpdateSong(const CueEntry &entry, const QString &next_index, Song *song) const;
  bool UpdateLastSong(const CueEntry &entry, Song *song) const;

  QStringList SplitCueLine(const QString &line) const;
  qint64 IndexToMarker(const QString &index) const;
};

#endif  // CUEPARSER_H
