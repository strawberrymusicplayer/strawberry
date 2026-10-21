/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include "config.h"

#include "gtest_include.h"

#include <QString>
#include <QUrl>

#include "core/enginemetadata.h"
#include "core/song.h"
#include "engine/streammetadataparser.h"

using namespace Qt::Literals::StringLiterals;

namespace {

/**
 * Creates metadata with sentinel values in every field for preservation checks.
 */
EngineMetadata MakeMetadata() {
  EngineMetadata metadata;
  metadata.type = EngineMetadata::Type::Next;
  metadata.media_url = QUrl(u"https://media.example/item"_s);
  metadata.stream_url = QUrl(u"https://stream.example/live"_s);
  metadata.title = u"Original title"_s;
  metadata.artist = u"Original artist"_s;
  metadata.album = u"Original album"_s;
  metadata.comment = u"Original comment"_s;
  metadata.genre = u"Original genre"_s;
  metadata.length = 123456;
  metadata.year = 2025;
  metadata.track = 7;
  metadata.filetype = Song::FileType::Unknown;
  metadata.samplerate = 44100;
  metadata.bitdepth = 24;
  metadata.bitrate = 320000;
  metadata.lyrics = u"Original lyrics"_s;
  return metadata;
}

/**
 * Creates sentinel metadata with artist and album cleared for fill-in tests.
 */
EngineMetadata MakeMetadataWithEmptyTags() {
  EngineMetadata metadata = MakeMetadata();
  metadata.artist.clear();
  metadata.album.clear();
  return metadata;
}

/**
 * Checks every metadata field, including fields unrelated to stream parsing.
 */
void ExpectMetadataEqual(const EngineMetadata &actual, const EngineMetadata &expected) {
  EXPECT_EQ(actual.type, expected.type);
  EXPECT_EQ(actual.media_url, expected.media_url);
  EXPECT_EQ(actual.stream_url, expected.stream_url);
  EXPECT_EQ(actual.title, expected.title);
  EXPECT_EQ(actual.artist, expected.artist);
  EXPECT_EQ(actual.album, expected.album);
  EXPECT_EQ(actual.comment, expected.comment);
  EXPECT_EQ(actual.genre, expected.genre);
  EXPECT_EQ(actual.length, expected.length);
  EXPECT_EQ(actual.year, expected.year);
  EXPECT_EQ(actual.track, expected.track);
  EXPECT_EQ(actual.filetype, expected.filetype);
  EXPECT_EQ(actual.samplerate, expected.samplerate);
  EXPECT_EQ(actual.bitdepth, expected.bitdepth);
  EXPECT_EQ(actual.bitrate, expected.bitrate);
  EXPECT_EQ(actual.lyrics, expected.lyrics);
}

TEST(StreamMetadataParserTest, ParsesConfirmedStreamPayload) {
  EngineMetadata metadata = MakeMetadataWithEmptyTags();
  const QString json = uR"({"artist":"ИГОРЬ КАПИТАННИКОВ;ЛЮДМИЛА БЕРЛИНСКАЯ","duration":"00:02:49","name":"ТЫ МНЕ ВЕРИШЬ ИЛИ НЕТ?","type":"М"})"_s;
  metadata.title = json;
  StreamMetadataParser::Parse(metadata);

  EngineMetadata expected = MakeMetadataWithEmptyTags();
  expected.title = u"ТЫ МНЕ ВЕРИШЬ ИЛИ НЕТ?"_s;
  expected.artist = u"ИГОРЬ КАПИТАННИКОВ;ЛЮДМИЛА БЕРЛИНСКАЯ"_s;
  ExpectMetadataEqual(metadata, expected);
}

TEST(StreamMetadataParserTest, PrefersTitleAndFallsBackToName) {
  struct TestCase {
    const char *name;
    QString json;
    QString expected_title;
  };
  const TestCase test_cases[] = {
      {"title preferred", uR"({"title":"Title value","name":"Name value"})"_s, u"Title value"_s},
      {"name fallback", uR"({"name":" Name value "})"_s, u"Name value"_s},
      {"empty title falls back", uR"({"title":"  ","name":"Name value"})"_s, u"Name value"_s},
      {"invalid title falls back", uR"({"title":17,"name":"Name value"})"_s, u"Name value"_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EngineMetadata metadata = MakeMetadata();
    metadata.title = test_case.json;
    StreamMetadataParser::Parse(metadata);
    EXPECT_EQ(metadata.title, test_case.expected_title);
    EXPECT_EQ(metadata.artist, u"Original artist"_s);
    EXPECT_EQ(metadata.album, u"Original album"_s);
  }
}

TEST(StreamMetadataParserTest, FillsOnlyPresentArtistAndAlbumFields) {
  struct TestCase {
    const char *name;
    QString json;
    QString expected_artist;
    QString expected_album;
  };
  const TestCase test_cases[] = {
      {"neither tag", uR"({"title":"JSON title"})"_s, u""_s, u""_s},
      {"artist only", uR"({"title":"JSON title","artist":" Artist "})"_s, u"Artist"_s, u""_s},
      {"album only", uR"({"title":"JSON title","album":" Album "})"_s, u""_s, u"Album"_s},
      {"both tags", uR"({"title":"JSON title","artist":" Artist ","album":" Album "})"_s, u"Artist"_s, u"Album"_s},
      {"empty and whitespace tags", uR"({"title":"JSON title","artist":"  ","album":""})"_s, u""_s, u""_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EngineMetadata metadata = MakeMetadataWithEmptyTags();
    metadata.title = test_case.json;
    EngineMetadata expected = MakeMetadataWithEmptyTags();
    expected.title = u"JSON title"_s;
    expected.artist = test_case.expected_artist;
    expected.album = test_case.expected_album;
    StreamMetadataParser::Parse(metadata);
    ExpectMetadataEqual(metadata, expected);
  }
}

TEST(StreamMetadataParserTest, PreservesPreexistingTags) {
  EngineMetadata metadata = MakeMetadata();
  metadata.title = uR"({"title":"JSON title","artist":"JSON artist","album":"JSON album"})"_s;
  StreamMetadataParser::Parse(metadata);

  EngineMetadata expected = MakeMetadata();
  expected.title = u"JSON title"_s;
  ExpectMetadataEqual(metadata, expected);
}

TEST(StreamMetadataParserTest, TrimsJsonCandidateAndDecodedFields) {
  EngineMetadata metadata = MakeMetadataWithEmptyTags();
  metadata.title = uR"json(
  { "title" : "  Trimmed title  ", "artist" : "  Trimmed artist  ", "album" : "  Trimmed album  " }
)json"_s;
  StreamMetadataParser::Parse(metadata);

  EXPECT_EQ(metadata.title, u"Trimmed title"_s);
  EXPECT_EQ(metadata.artist, u"Trimmed artist"_s);
  EXPECT_EQ(metadata.album, u"Trimmed album"_s);
}

TEST(StreamMetadataParserTest, DecodesUnicodeEscapesQuotesAndBackslashes) {
  EngineMetadata metadata = MakeMetadataWithEmptyTags();
  metadata.title = uR"({"title":"Привет, \"мир\" \\ путь","artist":"\u0418\u0433\u043e\u0440\u044c"})"_s;
  StreamMetadataParser::Parse(metadata);

  EXPECT_EQ(metadata.title, u"Привет, \"мир\" \\ путь"_s);
  EXPECT_EQ(metadata.artist, u"Игорь"_s);
}

TEST(StreamMetadataParserTest, DoesNotSplitSeparatorsInsideJson) {
  EngineMetadata metadata = MakeMetadataWithEmptyTags();
  metadata.title = uR"({"title":"Artist - Title ~ Album","artist":"A ~ B - C","album":"Album - side ~ two"})"_s;
  StreamMetadataParser::Parse(metadata);

  EXPECT_EQ(metadata.title, u"Artist - Title ~ Album"_s);
  EXPECT_EQ(metadata.artist, u"A ~ B - C"_s);
  EXPECT_EQ(metadata.album, u"Album - side ~ two"_s);
}

TEST(StreamMetadataParserTest, IgnoresWrongArtistAndAlbumTypes) {
  struct TestCase {
    const char *name;
    QString json;
  };
  const TestCase test_cases[] = {
      {"number and null", uR"({"title":"JSON title","artist":42,"album":null})"_s},
      {"object and array", uR"({"title":"JSON title","artist":{},"album":[]})"_s},
      {"booleans", uR"({"title":"JSON title","artist":true,"album":false})"_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EngineMetadata metadata = MakeMetadataWithEmptyTags();
    metadata.title = test_case.json;
    EngineMetadata expected = MakeMetadataWithEmptyTags();
    expected.title = u"JSON title"_s;
    StreamMetadataParser::Parse(metadata);
    ExpectMetadataEqual(metadata, expected);
  }
}

TEST(StreamMetadataParserTest, LeavesUnsupportedAndInvalidJsonUnchanged) {
  struct TestCase {
    const char *name;
    QString candidate;
  };
  const TestCase test_cases[] = {
      {"wrong title type", uR"({"title":17,"artist":"New artist"})"_s},
      {"object title type", uR"({"title":{"value":"Title"},"artist":"New artist"})"_s},
      {"array title type", uR"({"title":["Title"],"artist":"New artist"})"_s},
      {"boolean title type", uR"({"title":true,"artist":"New artist"})"_s},
      {"null title", uR"({"title":null,"name":false,"artist":"New artist"})"_s},
      {"empty titles", uR"({"title":"  ","name":""})"_s},
      {"no title fields", uR"({"artist":"New artist","album":"New album"})"_s},
      {"unknown schema", uR"({"unknown":"Artist - Title ~ Album"})"_s},
      {"empty object", u"{}"_s},
      {"empty array", u"[]"_s},
      {"array values with separators", uR"(["Artist - Title ~ Album"] )"_s},
      {"array with supported-looking object", uR"([{"title":"Title","artist":"Artist"}])"_s},
      {"malformed object", uR"({"title":"Incomplete")"_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    const EngineMetadata expected = MakeMetadataWithEmptyTags();
    EngineMetadata metadata = expected;
    metadata.title = test_case.candidate;
    // Unsupported JSON must not alter artist, album, or unrelated metadata.
    EngineMetadata expected_after = expected;
    expected_after.title = test_case.candidate;
    StreamMetadataParser::Parse(metadata);
    ExpectMetadataEqual(metadata, expected_after);
  }
}

TEST(StreamMetadataParserTest, MalformedJsonFallsBackToLegacyParsing) {
  struct TestCase {
    const char *name;
    QString candidate;
    QString expected_artist;
    QString expected_title;
  };
  const TestCase test_cases[] = {
      {"truncated object with separator", uR"({"artist":"Partial","title":"First - Second)"_s, uR"({"artist":"Partial","title":"First)"_s, u"Second"_s},
      {"malformed array with separator", u"[Broken - Track"_s, u"[Broken"_s, u"Track"_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EngineMetadata metadata = MakeMetadataWithEmptyTags();
    metadata.title = test_case.candidate;
    StreamMetadataParser::Parse(metadata);
    EXPECT_EQ(metadata.artist, test_case.expected_artist);
    EXPECT_EQ(metadata.title, test_case.expected_title);
    EXPECT_TRUE(metadata.album.isEmpty());
  }
}

TEST(StreamMetadataParserTest, LegacyParsingUsesPreferredSeparatorAndTrimsParts) {
  struct TestCase {
    const char *name;
    QString text;
    QString expected_artist;
    QString expected_title;
    QString expected_album;
  };
  const TestCase test_cases[] = {
      {"dash and ignored extra parts", u" Artist - Song - Album - ignored "_s, u"Artist"_s, u"Song"_s, u"Album"_s},
      {"simple dash", u"Artist - Title"_s, u"Artist"_s, u"Title"_s, u""_s},
      {"tilde separator", u" Artist ~ Song ~ Album ~ ignored "_s, u"Artist"_s, u"Song"_s, u"Album"_s},
      {"simple tilde", u"Artist~Title"_s, u"Artist"_s, u"Title"_s, u""_s},
      {"dash preferred to tilde", u"Artist~Alt - Title~Extra - Album"_s, u"Artist~Alt"_s, u"Title~Extra"_s, u"Album"_s},
      {"empty artist", u" - Title - Album"_s, u""_s, u"Title"_s, u"Album"_s},
      {"empty title", u"Artist -  - Album"_s, u"Artist"_s, u""_s, u"Album"_s},
      {"empty album", u"Artist - Title - "_s, u"Artist"_s, u"Title"_s, u""_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EngineMetadata metadata = MakeMetadataWithEmptyTags();
    metadata.title = test_case.text;
    StreamMetadataParser::Parse(metadata);
    EXPECT_EQ(metadata.artist, test_case.expected_artist);
    EXPECT_EQ(metadata.title, test_case.expected_title);
    EXPECT_EQ(metadata.album, test_case.expected_album);
  }
}

TEST(StreamMetadataParserTest, LegacyParsingLeavesPlainTitleAndHonorsExistingTagGuard) {
  struct TestCase {
    const char *name;
    QString title;
    QString artist;
    QString album;
  };
  const TestCase test_cases[] = {
      {"plain title", u"Plain title"_s, u""_s, u""_s},
      {"whitespace-only title", u" \t\n "_s, u""_s, u""_s},
      {"existing artist", u"New artist - New title - New album"_s, u"Existing artist"_s, u""_s},
      {"existing album", u"New artist - New title - New album"_s, u""_s, u"Existing album"_s},
      {"empty title", u""_s, u""_s, u""_s},
  };

  for (const TestCase &test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EngineMetadata expected = MakeMetadataWithEmptyTags();
    expected.title = test_case.title;
    expected.artist = test_case.artist;
    expected.album = test_case.album;
    EngineMetadata metadata = expected;
    StreamMetadataParser::Parse(metadata);
    ExpectMetadataEqual(metadata, expected);
  }
}

}  // namespace
