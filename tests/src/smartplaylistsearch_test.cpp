/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "gtest_include.h"

#include <QByteArray>
#include <QString>
#include <QVariantList>
#include <QTemporaryDir>
#include <QStringList>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/settings.h"
#include "core/song.h"
#include "smartplaylists/playlistquerygenerator.h"
#include "smartplaylists/smartplaylistsearch.h"
#include "smartplaylists/smartplaylistsearchterm.h"

using namespace Qt::Literals::StringLiterals;

namespace {

SmartPlaylistSearch GenreSearch() {

  return SmartPlaylistSearch(SmartPlaylistSearch::SearchType::And,
                             SmartPlaylistSearch::TermList() << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Genre, SmartPlaylistSearchTerm::Operator::Equals, u"Classical"_s),
                             SmartPlaylistSearch::SortType::Random,
                             SmartPlaylistSearchTerm::Field::Title,
                             -1);

}

TEST(SmartPlaylistSearchTest, SqlIncludesGenreColumn) {

  const SmartPlaylistSearch search = GenreSearch();
  QVariantList bound_values;
  const QString sql = search.ToSql(u"songs"_s, bound_values);
  EXPECT_TRUE(sql.contains("genre"_L1)) << "sql: " << sql.toStdString();
}

TEST(SmartPlaylistSearchTest, SaveAndLoadRoundTrip) {

  const SmartPlaylistSearch search = GenreSearch();

  PlaylistQueryGenerator generator(u"Test"_s, search, false);
  const QByteArray data = generator.Save();

  PlaylistQueryGenerator loaded_generator;
  loaded_generator.Load(data);
  const SmartPlaylistSearch loaded = loaded_generator.search();

  EXPECT_EQ(loaded.terms_.count(), 1);
  EXPECT_EQ(static_cast<int>(loaded.search_type_), static_cast<int>(SmartPlaylistSearch::SearchType::And));
  EXPECT_EQ(static_cast<int>(loaded.sort_type_), static_cast<int>(SmartPlaylistSearch::SortType::Random));
  EXPECT_EQ(loaded.limit_, -1);
  EXPECT_FALSE(loaded_generator.is_dynamic());

  if (!loaded.terms_.isEmpty()) {
    const SmartPlaylistSearchTerm &term = loaded.terms_.first();
    EXPECT_EQ(static_cast<int>(term.field_), static_cast<int>(SmartPlaylistSearchTerm::Field::Genre));
    EXPECT_EQ(static_cast<int>(term.operator_), static_cast<int>(SmartPlaylistSearchTerm::Operator::Equals));
    EXPECT_EQ(term.value_.toString(), u"Classical"_s);
  }

  QVariantList bound_values;
  const QString sql = loaded.ToSql(u"songs"_s, bound_values);
  EXPECT_TRUE(sql.contains("genre"_L1));

  EXPECT_TRUE(loaded == search);
}

// Mirrors what SmartPlaylistsModel does: write the generator into a Settings array, then read it back in a fresh Settings object.
TEST(SmartPlaylistSearchTest, SettingsRoundTrip) {

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString filename = dir.filePath(u"test.conf"_s);

  const SmartPlaylistSearch search = GenreSearch();
  PlaylistQueryGenerator generator(u"Test"_s, search, false);
  const QByteArray data = generator.Save();

  {
    Settings s(filename, QSettings::IniFormat);
    s.beginGroup(u"SerializedSmartPlaylists"_s);
    const int count = s.beginReadArray(u"songs"_s);
    s.endArray();
    s.beginWriteArray(u"songs"_s, count + 1);
    s.setArrayIndex(count);
    s.setValue(u"name"_s, u"Test"_s);
    s.setValue(u"type"_s, static_cast<int>(generator.type()));
    s.setValue(u"data"_s, data);
    s.endArray();
    s.endGroup();
    s.sync();
  }


  QByteArray read_data;
  {
    Settings s(filename, QSettings::IniFormat);
    s.beginGroup(u"SerializedSmartPlaylists"_s);
    const int count = s.beginReadArray(u"songs"_s);
    ASSERT_EQ(count, 1);
    s.setArrayIndex(0);
    read_data = s.value(u"data"_s).toByteArray();
    s.endArray();
    s.endGroup();
  }

  EXPECT_EQ(read_data, data);

  PlaylistQueryGenerator loaded_generator;
  loaded_generator.Load(read_data);
  const SmartPlaylistSearch loaded = loaded_generator.search();
  EXPECT_EQ(loaded.terms_.count(), 1);

  QVariantList bound_values;
  const QString sql = loaded.ToSql(u"songs"_s, bound_values);
  EXPECT_TRUE(sql.contains("genre = ? COLLATE NOCASE"_L1)) << "sql was: " << sql.toStdString();

}

// Returns the file names of the songs matching the search term, by running the SQL on songs with the file name and the file type.
QStringList MatchingSongs(const SmartPlaylistSearchTerm &term, const QList<QPair<QString, Song::FileType>> &songs) {

  QStringList matching;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"smartplaylistsearchterm_test"_s);
    db.setDatabaseName(u":memory:"_s);
    if (!db.open()) return QStringList() << u"Could not open database"_s;

    QSqlQuery q(db);
    q.exec(u"CREATE TABLE songs (url TEXT, filetype INTEGER)"_s);
    for (const QPair<QString, Song::FileType> &song : songs) {
      q.prepare(u"INSERT INTO songs (url, filetype) VALUES (?, ?)"_s);
      q.addBindValue(QString::fromUtf8(QUrl::fromLocalFile(song.first).toEncoded()));
      q.addBindValue(static_cast<int>(song.second));
      q.exec();
    }

    QVariantList bound_values;
    q.prepare(u"SELECT url FROM songs WHERE "_s + term.ToSql(bound_values) + u" ORDER BY rowid"_s);
    for (const QVariant &value : std::as_const(bound_values)) {
      q.addBindValue(value);
    }
    if (q.exec()) {
      while (q.next()) {
        matching << QUrl::fromEncoded(q.value(0).toByteArray()).toLocalFile();
      }
    }
    else {
      matching << u"Query failed"_s;
    }
  }
  QSqlDatabase::removeDatabase(u"smartplaylistsearchterm_test"_s);

  return matching;

}

const QList<QPair<QString, Song::FileType>> kSongs = QList<QPair<QString, Song::FileType>>() << qMakePair(u"/music/a b.flac"_s, Song::FileType::FLAC)
                                                                                           << qMakePair(u"/music/b.mpc"_s, Song::FileType::MPC)
                                                                                           << qMakePair(u"/music/c.ogg"_s, Song::FileType::OggFlac)
                                                                                           << qMakePair(u"/music/d.mp3"_s, Song::FileType::MPEG)
                                                                                           << qMakePair(u"/music/e.it"_s, Song::FileType::IT)
                                                                                           << qMakePair(u"/music/f.dat"_s, Song::FileType::Unknown);

// Regression test: the path was not turned into a URL like the stored URLs for "not equals", so it excluded nothing.
TEST(SmartPlaylistSearchTermTest, FilepathNotEqualsExcludesThePath) {

  EXPECT_EQ(QStringList() << u"/music/a b.flac"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filepath, SmartPlaylistSearchTerm::Operator::Equals, u"/music/a b.flac"_s), kSongs));
  EXPECT_EQ(QStringList() << u"/music/b.mpc"_s << u"/music/c.ogg"_s << u"/music/d.mp3"_s << u"/music/e.it"_s << u"/music/f.dat"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filepath, SmartPlaylistSearchTerm::Operator::NotEquals, u"/music/a b.flac"_s), kSongs));

}

// Regression test: the text operators were applied to the number of the file type, "contains FLAC" (2) also matched MPC (12) and the module formats (20-24).
TEST(SmartPlaylistSearchTermTest, FiletypeContainsMatchesTheNameOfTheFiletype) {

  EXPECT_EQ(QStringList() << u"/music/a b.flac"_s << u"/music/c.ogg"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Contains, u"Flac"_s), kSongs));
  EXPECT_EQ(QStringList() << u"/music/b.mpc"_s << u"/music/d.mp3"_s << u"/music/e.it"_s << u"/music/f.dat"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::NotContains, u"Flac"_s), kSongs));

}

TEST(SmartPlaylistSearchTermTest, FiletypeStartsWithAndEndsWithMatchTheNameOrExtension) {

  // MPEG by its name, MPC by its name and extension, and MP3 files by the extension.
  EXPECT_EQ(QStringList() << u"/music/b.mpc"_s << u"/music/d.mp3"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::StartsWith, u"mp"_s), kSongs));
  EXPECT_EQ(QStringList() << u"/music/d.mp3"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::EndsWith, u"3"_s), kSongs));

  // FLAC, and Ogg FLAC by its extension, not the module formats, named "Module Music Format", which only contain it.
  EXPECT_EQ(QStringList() << u"/music/a b.flac"_s << u"/music/c.ogg"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::StartsWith, u"f"_s), kSongs));
  // Also not the module formats.
  EXPECT_EQ(QStringList() << u"/music/a b.flac"_s << u"/music/b.mpc"_s << u"/music/c.ogg"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::EndsWith, u"c"_s), kSongs));

}

// The other file extensions recognised as the file type are also matched, not only the extension used when writing the file type.
TEST(SmartPlaylistSearchTermTest, FiletypeContainsMatchesOtherExtensions) {

  const QList<QPair<QString, Song::FileType>> songs = QList<QPair<QString, Song::FileType>>() << qMakePair(u"/music/a.mp2"_s, Song::FileType::MPEG)
                                                                                              << qMakePair(u"/music/b.m4a"_s, Song::FileType::MP4)
                                                                                              << qMakePair(u"/music/c.dff"_s, Song::FileType::DSDIFF)
                                                                                              << qMakePair(u"/music/d.flac"_s, Song::FileType::FLAC);

  EXPECT_EQ(QStringList() << u"/music/a.mp2"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Contains, u"mp2"_s), songs));
  EXPECT_EQ(QStringList() << u"/music/b.m4a"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Contains, u"m4a"_s), songs));
  EXPECT_EQ(QStringList() << u"/music/a.mp2"_s << u"/music/c.dff"_s << u"/music/d.flac"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::NotContains, u"m4a"_s), songs));
  EXPECT_EQ(QStringList() << u"/music/c.dff"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Contains, u"dff"_s), songs));

}

TEST(SmartPlaylistSearchTermTest, FiletypeContainsNothingMatching) {

  EXPECT_TRUE(MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Contains, u"xyz"_s), kSongs).isEmpty());
  // The file types without an extension of their own get the extension of the unknown file type when writing files, which is not matched.
  const QList<QPair<QString, Song::FileType>> songs = QList<QPair<QString, Song::FileType>>() << qMakePair(u"/music/a.dat"_s, Song::FileType::Unknown)
                                                                                              << qMakePair(u"/music/b.pcm"_s, Song::FileType::PCM)
                                                                                              << qMakePair(u"/music/c.cda"_s, Song::FileType::CDDA)
                                                                                              << qMakePair(u"/music/d.stream"_s, Song::FileType::Stream);
  EXPECT_TRUE(MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Contains, u"dat"_s), songs).isEmpty());
  EXPECT_EQ(songs.count(), MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::NotContains, u"dat"_s), songs).count());
  EXPECT_EQ(kSongs.count(), MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::NotContains, u"xyz"_s), kSongs).count());

}

// Regression test: "empty" compared the number of the file type with an empty string, so it matched nothing.
TEST(SmartPlaylistSearchTermTest, FiletypeEmptyMatchesUnknownFiletype) {

  EXPECT_EQ(QStringList() << u"/music/f.dat"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Empty, QString()), kSongs));
  EXPECT_EQ(kSongs.count() - 1, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::NotEmpty, QString()), kSongs).count());

}

TEST(SmartPlaylistSearchTermTest, FiletypeEqualsMatchesTheExtension) {

  EXPECT_EQ(QStringList() << u"/music/d.mp3"_s, MatchingSongs(SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Filetype, SmartPlaylistSearchTerm::Operator::Equals, u"mp3"_s), kSongs));

}

}  // namespace
