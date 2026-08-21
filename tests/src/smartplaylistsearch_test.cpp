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

#include "core/settings.h"
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
  EXPECT_TRUE(sql.contains("genre LIKE ?"_L1)) << "sql was: " << sql.toStdString();

}

}  // namespace
