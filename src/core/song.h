/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SONG_H
#define SONG_H

#include "config.h"

#include <QtGlobal>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QMetaType>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QImage>
#include <QIcon>

class QSqlQuery;

namespace Engine {
struct SimpleMetaBundle;
}  // namespace Engine

namespace pb {
namespace tagreader {
class SongMetadata;
}  // namespace tagreader
}  // namespace pb

#ifdef HAVE_LIBGPOD
struct _Itdb_Track;
#endif

#ifdef HAVE_LIBMTP
struct LIBMTP_track_struct;
#endif

class SqlRow;

class Song {

 public:

  enum Source {
    Source_Unknown = 0,
    Source_LocalFile = 1,
    Source_Collection = 2,
    Source_CDDA = 3,
    Source_Device = 4,
    Source_Stream = 5,
    Source_Tidal = 6,
    Source_Subsonic = 7,
    Source_Qobuz = 8,
  };

  // Don't change these values - they're stored in the database, and defined in the tag reader protobuf.
  // If a new lossless file is added, also add it to IsFileLossless().

  enum FileType {
    FileType_Unknown = 0,
    FileType_WAV = 1,
    FileType_FLAC = 2,
    FileType_WavPack = 3,
    FileType_OggFlac = 4,
    FileType_OggVorbis = 5,
    FileType_OggOpus = 6,
    FileType_OggSpeex = 7,
    FileType_MPEG = 8,
    FileType_MP4 = 9,
    FileType_ASF = 10,
    FileType_AIFF = 11,
    FileType_MPC = 12,
    FileType_TrueAudio = 13,
    FileType_DSF = 14,
    FileType_DSDIFF = 15,
    FileType_PCM = 16,
    FileType_APE = 17,
    FileType_CDDA = 90,
    FileType_Stream = 91,
  };

  Song(Song::Source source = Song::Source_Unknown);
  Song(const Song &other);
  ~Song();

  static const QStringList kColumns;
  static const QString kColumnSpec;
  static const QString kBindSpec;
  static const QString kUpdateSpec;

  static const QStringList kFtsColumns;
  static const QString kFtsColumnSpec;
  static const QString kFtsBindSpec;
  static const QString kFtsUpdateSpec;

  static const QString kManuallyUnsetCover;
  static const QString kEmbeddedCover;

  static const QRegularExpression kAlbumRemoveDisc;
  static const QRegularExpression kAlbumRemoveMisc;
  static const QRegularExpression kTitleRemoveMisc;

  static const QString kVariousArtists;

  static const QStringList kArticles;

  static QString JoinSpec(const QString &table);

  static Source SourceFromURL(const QUrl &url);
  static QString TextForSource(Source source);
  static Song::Source SourceFromText(const QString &source);
  static QIcon IconForSource(Source source);
  static QString TextForFiletype(FileType filetype);
  static QString ExtensionForFiletype(FileType filetype);
  static QIcon IconForFiletype(FileType filetype);

  QString TextForSource() const { return TextForSource(source()); }
  QIcon IconForSource() const { return IconForSource(source()); }
  QString TextForFiletype() const { return TextForFiletype(filetype()); }
  QIcon IconForFiletype() const { return IconForFiletype(filetype()); }

  bool IsFileLossless() const;
  static FileType FiletypeByMimetype(const QString &mimetype);
  static FileType FiletypeByDescription(const QString &text);
  static FileType FiletypeByExtension(const QString &ext);
  static QString ImageCacheDir(const Song::Source source);

  // Sort songs alphabetically using their pretty title
  static int CompareSongsName(const Song &song1, const Song &song2);
  static void SortSongsListAlphabetically(QList<Song> *songs);

  // Constructors
  void Init(const QString &title, const QString &artist, const QString &album, qint64 length_nanosec);
  void Init(const QString &title, const QString &artist, const QString &album, qint64 beginning, qint64 end);
  void InitFromProtobuf(const pb::tagreader::SongMetadata &pb);
  void InitFromQuery(const SqlRow &query, bool reliable_metadata, int col = 0);
  void InitFromFilePartial(const QString &filename);  // Just store the filename: incomplete but fast
  void InitArtManual();  // Check if there is already a art in the cache and store the filename in art_manual

  bool MergeFromSimpleMetaBundle(const Engine::SimpleMetaBundle &bundle);

#ifdef HAVE_LIBGPOD
  void InitFromItdb(_Itdb_Track *track, const QString &prefix);
  void ToItdb(_Itdb_Track *track) const;
#endif

#ifdef HAVE_LIBMTP
  void InitFromMTP(const LIBMTP_track_struct *track, const QString &host);
  void ToMTP(LIBMTP_track_struct *track) const;
#endif

  // Copies important statistics from the other song to this one, overwriting any data that already exists.
  // Useful when you want updated tags from disk but you want to keep user stats.
  void MergeUserSetData(const Song &other);

  // Save
  void BindToQuery(QSqlQuery *query) const;
  void BindToFtsQuery(QSqlQuery *query) const;
  void ToXesam(QVariantMap *map) const;
  void ToProtobuf(pb::tagreader::SongMetadata *pb) const;

  // Simple accessors
  bool is_valid() const;
  bool is_unavailable() const;
  int id() const;

  const QString &title() const;
  const QString &title_sortable() const;
  const QString &album() const;
  const QString &album_sortable() const;
  const QString &artist() const;
  const QString &artist_sortable() const;
  const QString &albumartist() const;
  const QString &albumartist_sortable() const;
  int track() const;
  int disc() const;
  int year() const;
  int originalyear() const;
  const QString &genre() const;
  bool compilation() const;
  const QString &composer() const;
  const QString &performer() const;
  const QString &grouping() const;
  const QString &comment() const;
  const QString &lyrics() const;

  QString artist_id() const;
  QString album_id() const;
  QString song_id() const;

  qint64 beginning_nanosec() const;
  qint64 end_nanosec() const;
  qint64 length_nanosec() const;

  int bitrate() const;
  int samplerate() const;
  int bitdepth() const;

  Source source() const;
  int directory_id() const;
  const QUrl &url() const;
  const QString &basefilename() const;
  FileType filetype() const;
  int filesize() const;
  qint64 mtime() const;
  qint64 ctime() const;

  int playcount() const;
  int skipcount() const;
  int lastplayed() const;

  bool compilation_detected() const;
  bool compilation_off() const;
  bool compilation_on() const;

  const QUrl &art_automatic() const;
  const QUrl &art_manual() const;

  const QString &cue_path() const;
  bool has_cue() const;

  float rating() const;

  const QString &effective_album() const;
  int effective_originalyear() const;
  const QString &effective_albumartist() const;
  const QString &effective_albumartist_sortable() const;

  bool is_collection_song() const;
  bool is_stream() const;
  bool is_cdda() const;
  bool is_metadata_good() const;
  bool art_automatic_is_valid() const;
  bool art_manual_is_valid() const;
  bool is_compilation() const;

  // Playlist views are special because you don't want to fill in album artists automatically for compilations, but you do for normal albums:
  const QString &playlist_albumartist() const;
  const QString &playlist_albumartist_sortable() const;

  // Returns true if this Song had it's cover manually unset by user.
  bool has_manually_unset_cover() const;
  // This method represents an explicit request to unset this song's cover.
  void manually_unset_cover();

  // Returns true if this song (it's media file) has an embedded cover.
  bool has_embedded_cover() const;
  // Sets a flag saying that this song (it's media file) has an embedded cover.
  void set_embedded_cover();

  const QUrl &stream_url() const;
  const QUrl &effective_stream_url() const;
  const QImage &image() const;

  const QString &error() const;

  // Pretty accessors
  QString PrettyTitle() const;
  QString PrettyTitleWithArtist() const;
  QString PrettyLength() const;
  QString PrettyYear() const;
  QString PrettyOriginalYear() const;

  QString TitleWithCompilationArtist() const;

  QString SampleRateBitDepthToText() const;

  QString PrettyRating() const;

  // Setters
  bool IsEditable() const;

  void set_id(int id);
  void set_valid(bool v);

  void set_title(const QString &v);
  void set_album(const QString &v);
  void set_artist(const QString &v);
  void set_albumartist(const QString &v);
  void set_track(int v);
  void set_disc(int v);
  void set_year(int v);
  void set_originalyear(int v);
  void set_genre(const QString &v);
  void set_genre_id3(int id);
  void set_compilation(bool v);
  void set_composer(const QString &v);
  void set_performer(const QString &v);
  void set_grouping(const QString &v);
  void set_comment(const QString &v);
  void set_lyrics(const QString &v);

  void set_artist_id(const QString &v);
  void set_album_id(const QString &v);
  void set_song_id(const QString &v);

  void set_beginning_nanosec(qint64 v);
  void set_end_nanosec(qint64 v);
  void set_length_nanosec(qint64 v);

  void set_bitrate(int v);
  void set_samplerate(int v);
  void set_bitdepth(int v);

  void set_source(Source v);
  void set_directory_id(int v);
  void set_url(const QUrl &v);
  void set_basefilename(const QString &v);
  void set_filetype(FileType v);
  void set_filesize(int v);
  void set_mtime(qint64 v);
  void set_ctime(qint64 v);
  void set_unavailable(bool v);

  void set_playcount(int v);
  void set_skipcount(int v);
  void set_lastplayed(int v);

  void set_compilation_detected(bool v);
  void set_compilation_on(bool v);
  void set_compilation_off(bool v);

  void set_art_automatic(const QUrl &v);
  void set_art_manual(const QUrl &v);

  void set_cue_path(const QString &v);

  void set_rating(const float v);

  void set_stream_url(const QUrl &v);
  void set_image(const QImage &i);

  // Comparison functions
  bool IsMetadataEqual(const Song &other) const;
  bool IsOnSameAlbum(const Song &other) const;
  bool IsSimilar(const Song &other) const;

  bool operator==(const Song &other) const;
  bool operator!=(const Song &other) const;

  // Two songs that are on the same album will have the same AlbumKey.
  // It is more efficient to use IsOnSameAlbum, but this function can be used when you need to hash the key to do fast lookups.
  QString AlbumKey() const;

  Song &operator=(const Song &other);

 private:
  struct Private;

  QString sortable(const QString &v) const;

  QSharedDataPointer<Private> d;
};
Q_DECLARE_METATYPE(Song)

typedef QList<Song> SongList;
Q_DECLARE_METATYPE(QList<Song>)

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const Song &song);
#else
uint qHash(const Song &song);
#endif
// Hash function using field checked in IsSimilar function
uint HashSimilar(const Song &song);

#endif  // SONG_H

