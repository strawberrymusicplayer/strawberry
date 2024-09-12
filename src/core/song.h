/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <optional>

#include <QtGlobal>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QMetaType>
#include <QList>
#include <QSet>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QUrl>
#include <QFileInfo>
#include <QIcon>

class SqlQuery;
class QSqlRecord;

class EngineMetadata;

namespace spb {
namespace tagreader {
class SongMetadata;
}  // namespace tagreader
}  // namespace spb

#ifdef HAVE_LIBGPOD
struct _Itdb_Track;
#endif

#ifdef HAVE_LIBMTP
struct LIBMTP_track_struct;
#endif

class SqlRow;

class Song {

 public:

  enum class Source {
    Unknown = 0,
    LocalFile = 1,
    Collection = 2,
    CDDA = 3,
    Device = 4,
    Stream = 5,
    Tidal = 6,
    Subsonic = 7,
    Qobuz = 8,
    SomaFM = 9,
    RadioParadise = 10,
    Spotify = 11
  };

  // Don't change these values - they're stored in the database, and defined in the tag reader protobuf.
  // If a new lossless file is added, also add it to IsFileLossless().

  enum class FileType {
    Unknown = 0,
    WAV = 1,
    FLAC = 2,
    WavPack = 3,
    OggFlac = 4,
    OggVorbis = 5,
    OggOpus = 6,
    OggSpeex = 7,
    MPEG = 8,
    MP4 = 9,
    ASF = 10,
    AIFF = 11,
    MPC = 12,
    TrueAudio = 13,
    DSF = 14,
    DSDIFF = 15,
    PCM = 16,
    APE = 17,
    MOD = 18,
    S3M = 19,
    XM = 20,
    IT = 21,
    SPC = 22,
    VGM = 23,
    CDDA = 90,
    Stream = 91
  };

  static const QStringList kColumns;
  static const QStringList kRowIdColumns;
  static const QString kColumnSpec;
  static const QString kRowIdColumnSpec;
  static const QString kBindSpec;
  static const QString kUpdateSpec;

  static const QStringList kTextSearchColumns;
  static const QStringList kIntSearchColumns;
  static const QStringList kUIntSearchColumns;
  static const QStringList kInt64SearchColumns;
  static const QStringList kFloatSearchColumns;
  static const QStringList kNumericalSearchColumns;
  static const QStringList kSearchColumns;

  using RegularExpressionList = QList<QRegularExpression>;
  static const RegularExpressionList kAlbumDisc;
  static const RegularExpressionList kRemastered;
  static const RegularExpressionList kExplicit;
  static const RegularExpressionList kAlbumMisc;
  static const RegularExpressionList kTitleMisc;

  static const QStringList kArticles;

  static const QStringList kAcceptedExtensions;
  static const QStringList kRejectedExtensions;

  Song(const Source source = Source::Unknown);
  Song(const Song &other);
  ~Song();

  bool operator==(const Song &other) const;
  bool operator!=(const Song &other) const;
  Song &operator=(const Song &other);

  // Simple accessors
  int id() const;
  bool is_valid() const;

  const QString &title() const;
  const QString &album() const;
  const QString &artist() const;
  const QString &albumartist() const;
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
  qint64 filesize() const;
  qint64 mtime() const;
  qint64 ctime() const;
  bool unavailable() const;

  QString fingerprint() const;

  uint playcount() const;
  uint skipcount() const;
  qint64 lastplayed() const;
  qint64 lastseen() const;

  bool compilation_detected() const;
  bool compilation_on() const;
  bool compilation_off() const;

  bool art_embedded() const;
  const QUrl &art_automatic() const;
  const QUrl &art_manual() const;
  bool art_unset() const;

  const QString &cue_path() const;

  float rating() const;

  const QString &acoustid_id() const;
  const QString &acoustid_fingerprint() const;

  const QString &musicbrainz_album_artist_id() const;
  const QString &musicbrainz_artist_id() const;
  const QString &musicbrainz_original_artist_id() const;
  const QString &musicbrainz_album_id() const;
  const QString &musicbrainz_original_album_id() const;
  const QString &musicbrainz_recording_id() const;
  const QString &musicbrainz_track_id() const;
  const QString &musicbrainz_disc_id() const;
  const QString &musicbrainz_release_group_id() const;
  const QString &musicbrainz_work_id() const;

  std::optional<double> ebur128_integrated_loudness_lufs() const;
  std::optional<double> ebur128_loudness_range_lu() const;

  bool init_from_file() const;

  const QString &title_sortable() const;
  const QString &album_sortable() const;
  const QString &artist_sortable() const;
  const QString &albumartist_sortable() const;

  const QUrl &stream_url() const;

  // Setters
  void set_id(const int id);
  void set_valid(const bool v);

  void set_title(const QString &v);
  void set_album(const QString &v);
  void set_artist(const QString &v);
  void set_albumartist(const QString &v);
  void set_track(const int v);
  void set_disc(const int v);
  void set_year(const int v);
  void set_originalyear(int v);
  void set_genre(const QString &v);
  void set_compilation(bool v);
  void set_composer(const QString &v);
  void set_performer(const QString &v);
  void set_grouping(const QString &v);
  void set_comment(const QString &v);
  void set_lyrics(const QString &v);

  void set_artist_id(const QString &v);
  void set_album_id(const QString &v);
  void set_song_id(const QString &v);

  void set_beginning_nanosec(const qint64 v);
  void set_end_nanosec(const qint64 v);
  void set_length_nanosec(const qint64 v);

  void set_bitrate(const int v);
  void set_samplerate(const int v);
  void set_bitdepth(const int v);

  void set_source(const Source v);
  void set_directory_id(const int v);
  void set_url(const QUrl &v);
  void set_basefilename(const QString &v);
  void set_filetype(const FileType v);
  void set_filesize(const qint64 v);
  void set_mtime(const qint64 v);
  void set_ctime(const qint64 v);
  void set_unavailable(const bool v);

  void set_fingerprint(const QString &v);

  void set_playcount(const uint v);
  void set_skipcount(const uint v);
  void set_lastplayed(const qint64 v);
  void set_lastseen(const qint64 v);

  void set_compilation_detected(const bool v);
  void set_compilation_on(const bool v);
  void set_compilation_off(const bool v);

  void set_art_embedded(const bool v);
  void set_art_automatic(const QUrl &v);
  void set_art_manual(const QUrl &v);
  void set_art_unset(const bool v);

  void set_cue_path(const QString &v);

  void set_rating(const float v);

  void set_acoustid_id(const QString &v);
  void set_acoustid_fingerprint(const QString &v);

  void set_musicbrainz_album_artist_id(const QString &v);
  void set_musicbrainz_artist_id(const QString &v);
  void set_musicbrainz_original_artist_id(const QString &v);
  void set_musicbrainz_album_id(const QString &v);
  void set_musicbrainz_original_album_id(const QString &v);
  void set_musicbrainz_recording_id(const QString &v);
  void set_musicbrainz_track_id(const QString &v);
  void set_musicbrainz_disc_id(const QString &v);
  void set_musicbrainz_release_group_id(const QString &v);
  void set_musicbrainz_work_id(const QString &v);

  void set_ebur128_integrated_loudness_lufs(const std::optional<double> v);
  void set_ebur128_loudness_range_lu(const std::optional<double> v);

  void set_stream_url(const QUrl &v);

  const QUrl &effective_stream_url() const;
  const QString &effective_albumartist() const;
  const QString &effective_albumartist_sortable() const;
  const QString &effective_album() const;
  int effective_originalyear() const;
  const QString &playlist_albumartist() const;
  const QString &playlist_albumartist_sortable() const;

  bool is_metadata_good() const;
  bool is_collection_song() const;
  bool is_stream() const;
  bool is_radio() const;
  bool is_cdda() const;
  bool is_compilation() const;
  bool stream_url_can_expire() const;
  bool is_module_music() const;
  bool has_cue() const;

  bool art_automatic_is_valid() const;
  bool art_manual_is_valid() const;
  bool has_valid_art() const;
  void clear_art_automatic();
  void clear_art_manual();

  bool write_tags_supported() const;
  bool additional_tags_supported() const;
  bool albumartist_supported() const;
  bool composer_supported() const;
  bool performer_supported() const;
  bool grouping_supported() const;
  bool genre_supported() const;
  bool compilation_supported() const;
  bool rating_supported() const;
  bool comment_supported() const;
  bool lyrics_supported() const;

  static bool save_embedded_cover_supported(const FileType filetype);
  bool save_embedded_cover_supported() const { return url().isLocalFile() && save_embedded_cover_supported(filetype()) && !has_cue(); };

  static int ColumnIndex(const QString &field);
  static QString JoinSpec(const QString &table);

  // Pretty accessors
  QString PrettyTitle() const;
  QString PrettyTitleWithArtist() const;
  QString PrettyLength() const;
  QString PrettyYear() const;
  QString PrettyOriginalYear() const;

  QString TitleWithCompilationArtist() const;

  QString SampleRateBitDepthToText() const;

  static QString Ebur128LoudnessLUFSToText(const std::optional<double> v);
  QString Ebur128LoudnessLUFSToText() const;

  static QString Ebur128LoudnessRangeLUToText(const std::optional<double> v);
  QString Ebur128LoudnessRangeLUToText() const;

  QString PrettyRating() const;

  bool IsEditable() const;

  // Comparison functions
  bool IsMetadataEqual(const Song &other) const;
  bool IsPlayStatisticsEqual(const Song &other) const;
  bool IsRatingEqual(const Song &other) const;
  bool IsFingerprintEqual(const Song &other) const;
  bool IsAcoustIdEqual(const Song &other) const;
  bool IsMusicBrainzEqual(const Song &other) const;
  bool IsEBUR128Equal(const Song &other) const;
  bool IsArtEqual(const Song &other) const;
  bool IsAllMetadataEqual(const Song &other) const;

  bool IsOnSameAlbum(const Song &other) const;
  bool IsSimilar(const Song &other) const;

  static Source SourceFromURL(const QUrl &url);
  static QString TextForSource(const Source source);
  static QString DescriptionForSource(const Source source);
  static Source SourceFromText(const QString &source);
  static QIcon IconForSource(const Source source);
  static QString TextForFiletype(const FileType filetype);
  static QString ExtensionForFiletype(const FileType filetype);
  static QIcon IconForFiletype(const FileType filetype);

  QString TextForSource() const { return TextForSource(source()); }
  QString DescriptionForSource() const { return DescriptionForSource(source()); }
  QIcon IconForSource() const { return IconForSource(source()); }
  QString TextForFiletype() const { return TextForFiletype(filetype()); }
  QIcon IconForFiletype() const { return IconForFiletype(filetype()); }

  bool IsFileLossless() const;
  static FileType FiletypeByMimetype(const QString &mimetype);
  static FileType FiletypeByDescription(const QString &text);
  static FileType FiletypeByExtension(const QString &ext);
  static QString ImageCacheDir(const Source source);

  // Sort songs alphabetically using their pretty title
  static int CompareSongsName(const Song &song1, const Song &song2);
  static void SortSongsListAlphabetically(QList<Song> *songs);

  // Constructors
  void Init(const QString &title, const QString &artist, const QString &album, const qint64 length_nanosec);
  void Init(const QString &title, const QString &artist, const QString &album, const qint64 beginning, const qint64 end);
  void InitFromProtobuf(const spb::tagreader::SongMetadata &pb);
  void InitFromQuery(const QSqlRecord &r, const bool reliable_metadata, const int col = 0);
  void InitFromQuery(const SqlQuery &query, const bool reliable_metadata, const int col = 0);
  void InitFromQuery(const SqlRow &row, const bool reliable_metadata, const int col = 0);
  void InitFromFilePartial(const QString &filename, const QFileInfo &fileinfo);
  void InitArtManual();
  void InitArtAutomatic();

#ifdef HAVE_LIBGPOD
  void InitFromItdb(_Itdb_Track *track, const QString &prefix);
  void ToItdb(_Itdb_Track *track) const;
#endif

#ifdef HAVE_LIBMTP
  void InitFromMTP(const LIBMTP_track_struct *track, const QString &host);
  void ToMTP(LIBMTP_track_struct *track) const;
#endif

  // Save
  void BindToQuery(SqlQuery *query) const;
#ifdef HAVE_DBUS
  void ToXesam(QVariantMap *map) const;
#endif
  void ToProtobuf(spb::tagreader::SongMetadata *pb) const;

  bool MergeFromEngineMetadata(const EngineMetadata &engine_metadata);

  // Copies important statistics from the other song to this one, overwriting any data that already exists.
  // Useful when you want updated tags from disk but you want to keep user stats.
  void MergeUserSetData(const Song &other, const bool merge_playcount, const bool merge_rating);

  // Two songs that are on the same album will have the same AlbumKey.
  // It is more efficient to use IsOnSameAlbum, but this function can be used when you need to hash the key to do fast lookups.
  QString AlbumKey() const;

  static bool ContainsRegexList(const QString &str, const RegularExpressionList &regex_list);
  static QString StripRegexList(QString str, const RegularExpressionList &regex_list);
  static bool AlbumContainsDisc(const QString &album);
  static QString AlbumRemoveDisc(const QString &album);
  static QString AlbumRemoveMisc(const QString &album);
  static QString AlbumRemoveDiscMisc(const QString &album);
  static QString TitleRemoveMisc(const QString &title);

 private:
  struct Private;

  static QString sortable(const QString &v);

  QSharedDataPointer<Private> d;
};

using SongList = QList<Song>;
using SongMap = QMap<QString, Song>;

Q_DECLARE_METATYPE(Song)
Q_DECLARE_METATYPE(SongList)
Q_DECLARE_METATYPE(SongMap)
Q_DECLARE_METATYPE(Song::Source)
Q_DECLARE_METATYPE(Song::FileType)

size_t qHash(const Song &song);
// Hash function using field checked in IsSimilar function
size_t HashSimilar(const Song &song);

#endif  // SONG_H
