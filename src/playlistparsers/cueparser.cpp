/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QFileInfo>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QTextStream>
#include <QStringConverter>

#include "includes/shared_ptr.h"
#include "constants/timeconstants.h"
#include "core/logging.h"
#include "utilities/textencodingutils.h"
#include "constants/playlistsettings.h"
#include "parserbase.h"
#include "cueparser.h"

using namespace Qt::Literals::StringLiterals;

class CollectionBackendInterface;

namespace {
constexpr char kFileLineRegExp[] = "(\\S+)\\s+(?:\"([^\"]+)\"|(\\S+))\\s*(?:\"([^\"]+)\"|(\\S+))?";
constexpr char kIndexRegExp[] = "(\\d{1,3}):(\\d{2}):(\\d{2})";

constexpr char kPerformer[] = "performer";
constexpr char kTitle[] = "title";
constexpr char kSongWriter[] = "songwriter";
// composer may be in cue file and is synonym for songwriter
constexpr char kComposer[] = "composer";
constexpr char kFile[] = "file";
constexpr char kTrack[] = "track";
constexpr char kIndex[] = "index";
constexpr char kAudioTrackType[] = "audio";
constexpr char kRem[] = "rem";
constexpr char kGenre[] = "genre";
constexpr char kDate[] = "date";
constexpr char kDisc[] = "discnumber";
}  // namespace

CueParser::CueParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : ParserBase(tagreader_client, collection_backend, parent) {}

ParserBase::LoadResult CueParser::Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup) const {

  SongList ret;

  QTextStream text_stream(device);

  const QByteArray data_chunk = device->peek(1024);

  std::optional<QStringConverter::Encoding> encoding = QStringConverter::encodingForData(data_chunk);
  if (encoding.has_value()) {
    text_stream.setEncoding(encoding.value());
  }
  else {
    const QByteArray encoding_name = Utilities::TextEncodingFromData(data_chunk);
    if (!encoding_name.isEmpty()) {
      encoding = QStringConverter::encodingForName(encoding_name.constData());
      if (encoding.has_value()) {
        text_stream.setEncoding(encoding.value());
      }
    }
  }

  QString dir_path = dir.absolutePath();
  // Read the first line already
  QString line = text_stream.readLine();

  QList<CueEntry> entries;

  QString album_artist;
  QString album;
  QString album_composer;
  QString album_genre;
  QString album_date;
  QString disc;

  // -- whole file
  while (!text_stream.atEnd()) {

    QString file;
    QString file_type;

    // -- FILE section
    do {
      QStringList splitted = SplitCueLine(line);

      // Uninteresting or incorrect line
      if (splitted.size() < 2) {
        continue;
      }
      const QString &line_name = splitted[0];
      const QString &line_value = splitted[1];

      if (line_name.compare(QLatin1String(kFile), Qt::CaseInsensitive) == 0) {
        file = QDir::isAbsolutePath(line_value) ? line_value : dir.absoluteFilePath(line_value);
        if (splitted.size() > 2) {
          file_type = splitted[2];
        }
      }
      else if (line_name.compare(QLatin1String(kPerformer), Qt::CaseInsensitive) == 0) {
        album_artist = line_value;
      }
      else if (line_name.compare(QLatin1String(kTitle), Qt::CaseInsensitive) == 0) {
        album = line_value;
      }
      else if (line_name.compare(QLatin1String(kSongWriter), Qt::CaseInsensitive) == 0) {
        album_composer = line_value;
      }
      else if (line_name.compare(QLatin1String(kComposer), Qt::CaseInsensitive) == 0) {
        album_composer = line_value;
      }
      else if (line_name.compare(QLatin1String(kRem), Qt::CaseInsensitive) == 0) {
        if (splitted.size() < 3) {
          break;
        }
        if (line_value.compare(QLatin1String(kGenre), Qt::CaseInsensitive) == 0) {
          album_genre = splitted[2];
        }
        else if (line_value.compare(QLatin1String(kDate), Qt::CaseInsensitive) == 0) {
          album_date = splitted[2];
        }
        else if (line_value.compare(QLatin1String(kDisc), Qt::CaseInsensitive) == 0) {
          disc = splitted[2];
        }
      }
      // End of the header -> go into the track mode
      else if (line_name.compare(QLatin1String(kTrack), Qt::CaseInsensitive) == 0) {
        break;
      }
      // Ignore the rest of possible field types for now...
    } while (!(line = text_stream.readLine()).isNull());

    if (line.isNull()) {
      qLog(Warning) << "The .cue file from" << dir_path << "defines no tracks!";
      return ret;
    }

    // If this is a data file, all of its tracks will be ignored
    bool valid_file = file_type.compare("BINARY"_L1, Qt::CaseInsensitive) != 0 && file_type.compare("MOTOROLA"_L1, Qt::CaseInsensitive) != 0;

    QString track_type;
    QString index;
    QString artist;
    QString composer;
    QString title;
    QString date;
    QString genre;

    // TRACK section
    do {
      QStringList splitted = SplitCueLine(line);

      // Uninteresting or incorrect line
      if (splitted.size() < 2) {
        continue;
      }

      const QString &line_name = splitted[0];
      const QString &line_value = splitted[1];
      QString line_additional = splitted.size() > 2 ? splitted[2].toLower() : ""_L1;

      if (line_name.compare(QLatin1String(kTrack), Qt::CaseInsensitive) == 0) {

        // The beginning of another track's definition - we're saving the current one for later (if it's valid of course)
        // please note that the same code is repeated just after this 'do-while' loop
        if (valid_file && !index.isEmpty() && (track_type.isEmpty() || track_type.compare(QLatin1String(kAudioTrackType), Qt::CaseInsensitive) == 0)) {
          entries.append(CueEntry(file, index, title, artist, album_artist, album, composer, album_composer, (genre.isEmpty() ? album_genre : genre), (date.isEmpty() ? album_date : date), disc));
        }

        // Clear the state
        track_type = index = artist = composer = title = date = genre = ""_L1;

        if (!line_additional.isEmpty()) {
          track_type = line_additional;
        }

      }
      else if (line_name.compare(QLatin1String(kIndex), Qt::CaseInsensitive) == 0) {

        // We need the index's position field
        if (!line_additional.isEmpty()) {

          // If there's none "01" index, we'll just take the first one also, we'll take the "01" index even if it's the last one
          if (line_value == "01"_L1 || index.isEmpty()) {

            index = line_additional;
          }
        }
      }
      else if (line_name.compare(QLatin1String(kTitle), Qt::CaseInsensitive) == 0) {
        title = line_value;
      }
      else if (line_name.compare(QLatin1String(kDate), Qt::CaseInsensitive) == 0) {
        date = line_value;
      }
      else if (line_name.compare(QLatin1String(kPerformer), Qt::CaseInsensitive) == 0) {
        artist = line_value;
      }
      else if (line_name.compare(QLatin1String(kSongWriter), Qt::CaseInsensitive) == 0) {
        composer = line_value;
      }
      else if (line_name.compare(QLatin1String(kComposer), Qt::CaseInsensitive) == 0) {
        composer = line_value;
      }
      // End of tracks for the current file -> parse next one
      else if (line_name.compare(QLatin1String(kRem), Qt::CaseInsensitive) == 0 && splitted.size() >= 3) {
        if (line_value.compare(QLatin1String(kGenre), Qt::CaseInsensitive) == 0) {
          genre = splitted[2];
        }
        else if (line_value.compare(QLatin1String(kDate), Qt::CaseInsensitive) == 0) {
          date = splitted[2];
        }
      }
      else if (line_name.compare(QLatin1String(kFile), Qt::CaseInsensitive) == 0) {
        break;
      }

      // Just ignore the rest of possible field types for now...
    } while (!(line = text_stream.readLine()).isNull());

    // We didn't add the last song yet...
    if (valid_file && !index.isEmpty() && (track_type.isEmpty() || track_type.compare(QLatin1String(kAudioTrackType), Qt::CaseInsensitive) == 0)) {
      entries.append(CueEntry(file, index, title, artist, album_artist, album, composer, album_composer, (genre.isEmpty() ? album_genre : genre), (date.isEmpty() ? album_date : date), disc));
    }
  }

  QDateTime cue_mtime = QFileInfo(playlist_path).lastModified();

  // Finalize parsing songs
  for (int i = 0; i < entries.length(); i++) {
    CueEntry entry = entries.at(i);

    Song song = LoadSong(entry.file, IndexToMarker(entry.index), 0, dir, collection_lookup);

    // Cue song has mtime equal to qMax(media_file_mtime, cue_sheet_mtime)
    if (cue_mtime.isValid()) {
      song.set_mtime(qMax(cue_mtime.toSecsSinceEpoch(), song.mtime()));
    }
    song.set_cue_path(playlist_path);

    // Overwrite the stuff, we may have read from the file or collection, using the current .cue metadata

    song.set_track(i + 1);

    // The last TRACK for every FILE gets it's 'end' marker from the media file's length
    if (i + 1 < entries.size() && entries.at(i).file == entries.at(i + 1).file) {
      // Incorrect indices?
      if (!UpdateSong(entry, entries.at(i + 1).index, &song)) {
        continue;
      }
    }
    else {
      // Incorrect index?
      if (!UpdateLastSong(entry, &song)) {
        continue;
      }
    }

    ret << song;
  }

  return ret;
}

// This and the kFileLineRegExp do most of the "dirty" work, namely: splitting the raw .cue
// line into logical parts and getting rid of all the unnecessary whitespaces and quoting.
QStringList CueParser::SplitCueLine(const QString &line) {

  QRegularExpression line_regexp(QString::fromLatin1(kFileLineRegExp));
  QRegularExpressionMatch re_match = line_regexp.match(line.trimmed());
  if (!re_match.hasMatch()) {
    return QStringList();
  }

  // Let's remove the empty entries while we're at it
  static const QRegularExpression regex_entry(u".+"_s);
  static const QRegularExpression regex_exclude(u"^\"\"$"_s);
  return re_match.capturedTexts().filter(regex_entry).mid(1, -1).replaceInStrings(regex_exclude, ""_L1);

}

// Updates the song with data from the .cue entry. This one mustn't be used for the last song in the .cue file.
bool CueParser::UpdateSong(const CueEntry &entry, const QString &next_index, Song *song) {

  qint64 beginning = IndexToMarker(entry.index);
  qint64 end = IndexToMarker(next_index);

  // Incorrect indices (we won't be able to calculate beginning or end)
  if (beginning == -1 || end == -1) {
    return false;
  }

  // Believe the CUE: Init() forces validity
  song->Init(entry.title, entry.PrettyArtist(), entry.album, beginning, end);
  song->set_albumartist(entry.album_artist);
  song->set_composer(entry.PrettyComposer());
  song->set_genre(entry.genre);

  int year = entry.date.toInt();
  if (year > 0) song->set_year(year);
  int disc = entry.disc.toInt();
  if (disc > 0) song->set_disc(disc);

  return true;

}

// Updates the song with data from the .cue entry. This one must be used only for the last song in the .cue file.
bool CueParser::UpdateLastSong(const CueEntry &entry, Song *song) {

  qint64 beginning = IndexToMarker(entry.index);

  // Incorrect index (we won't be able to calculate beginning)
  if (beginning == -1) {
    return false;
  }

  // Believe the CUE and force validity (like UpdateSong() does)
  song->set_valid(true);

  song->set_title(entry.title);
  song->set_artist(entry.PrettyArtist());
  song->set_album(entry.album);
  song->set_albumartist(entry.album_artist);
  song->set_genre(entry.genre);
  song->set_composer(entry.PrettyComposer());

  int year = entry.date.toInt();
  if (year > 0) song->set_year(year);
  int disc = entry.disc.toInt();
  if (disc > 0) song->set_disc(disc);

  // We don't do anything with the end here because it's already set to the end of the media file (if it exists)
  song->set_beginning_nanosec(beginning);

  return true;

}

qint64 CueParser::IndexToMarker(const QString &index) {

  QRegularExpression index_regexp(QString::fromLatin1(kIndexRegExp));
  QRegularExpressionMatch re_match = index_regexp.match(index);
  if (!re_match.hasMatch()) {
    return -1;
  }

  QStringList splitted = re_match.capturedTexts().mid(1, -1);
  qint64 frames = splitted.at(0).toLongLong() * 60 * 75 + splitted.at(1).toLongLong() * 75 + splitted.at(2).toLongLong();
  return (frames * kNsecPerSec) / 75;

}

void CueParser::Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type) const {

  Q_UNUSED(playlist_name);
  Q_UNUSED(songs);
  Q_UNUSED(device);
  Q_UNUSED(dir);
  Q_UNUSED(path_type);

  Q_EMIT Error(tr("Saving CUE files is not supported."));

  // TODO

}

// Looks for a track starting with one of the .cue's keywords.
bool CueParser::TryMagic(const QByteArray &data) const {

  QStringList splitted = QString::fromUtf8(data.constData()).split(u'\n');

  for (int i = 0; i < splitted.length(); i++) {
    QString line = splitted.at(i).trimmed();
    if (line.startsWith(QLatin1String(kPerformer), Qt::CaseInsensitive) ||
        line.startsWith(QLatin1String(kTitle), Qt::CaseInsensitive) ||
        line.startsWith(QLatin1String(kFile), Qt::CaseInsensitive) ||
        line.startsWith(QLatin1String(kTrack), Qt::CaseInsensitive)) {
      return true;
    }
  }

  return false;

}

QString CueParser::FindCueFilename(const QString &filename) {

  const QStringList cue_files = QStringList() << filename + u".cue"_s
                                              << filename.section(u'.', 0, -2) + u".cue"_s;

  for (const QString &cuefile : cue_files) {
    if (QFileInfo::exists(cuefile)) return cuefile;
  }

  return QString();

}
