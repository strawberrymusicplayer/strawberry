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

#include <QString>
#include <QUrl>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

#include "includes/shared_ptr.h"
#include "core/memorydatabase.h"
#include "plex/plexrequest.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

TEST(PlexRequestTest, CoverImageId) {

  EXPECT_EQ(u"metadata-123-thumb-1700000000"_s, PlexRequest::CoverImageId(QUrl(u"https://plex.example:32400/library/metadata/123/thumb/1700000000"_s)));
  // The server can be behind a path.
  EXPECT_EQ(u"metadata-123-thumb-1700000000"_s, PlexRequest::CoverImageId(QUrl(u"https://example.com/plex/library/metadata/123/thumb/1700000000"_s)));
  EXPECT_TRUE(PlexRequest::CoverImageId(QUrl(u"https://images.example/art.jpg"_s)).isEmpty());
  EXPECT_TRUE(PlexRequest::CoverImageId(QUrl(u"https://plex.example/library/metadata/../thumb/1"_s)).isEmpty());
  EXPECT_TRUE(PlexRequest::CoverImageId(QUrl::fromLocalFile(u"/tmp/library/metadata/1/thumb/2"_s)).isEmpty());

}

// A server URL entered in the settings can have the access token in the query, which must not be saved in the cover URL.
TEST(PlexRequestTest, CoverUrlWithoutQuery) {

  const QUrl cover_url = PlexRequest::CoverUrl(QUrl(u"https://plex.example:32400/?X-Plex-Token=secret#fragment"_s), u"/library/metadata/123/thumb/1700000000"_s);
  EXPECT_EQ(QUrl(u"https://plex.example:32400/library/metadata/123/thumb/1700000000"_s), cover_url);
  EXPECT_FALSE(cover_url.toString().contains(u"secret"_s));

}

TEST(PlexRequestTest, CoverUrlForServerMovesToCurrentServer) {

  const QUrl cover_url(u"http://192.168.1.2:32400/library/metadata/123/thumb/1700000000?X-Plex-Token=secret"_s);
  EXPECT_EQ(QUrl(u"https://plex.example:32400/library/metadata/123/thumb/1700000000"_s), PlexRequest::CoverUrlForServer(cover_url, QUrl(u"https://plex.example:32400"_s)));
  // Without a server URL, only the token is removed.
  EXPECT_EQ(QUrl(u"http://192.168.1.2:32400/library/metadata/123/thumb/1700000000"_s), PlexRequest::CoverUrlForServer(cover_url, QUrl()));

}

// Covers which are not from the Plex server, for example signed URLs, are kept with their query.
TEST(PlexRequestTest, CoverUrlForServerKeepsOtherUrls) {

  const QUrl cover_url(u"https://images.example/art.jpg?signature=abc"_s);
  EXPECT_EQ(cover_url, PlexRequest::CoverUrlForServer(cover_url, QUrl(u"https://plex.example:32400"_s)));

}

// A downloaded cover which is not in the cache anymore is downloaded again from the server.
TEST(PlexRequestTest, CoverUrlForServerDownloadsMissingCacheFileAgain) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QUrl server_url(u"https://plex.example:32400"_s);

  const QUrl missing_cover_url = QUrl::fromLocalFile(temp_dir.filePath(u"metadata-123-thumb-1700000000.jpg"_s));
  EXPECT_EQ(QUrl(u"https://plex.example:32400/library/metadata/123/thumb/1700000000"_s), PlexRequest::CoverUrlForServer(missing_cover_url, server_url));

  const QUrl cached_cover_url = QUrl::fromLocalFile(temp_dir.filePath(u"metadata-456-thumb-1700000000.jpg"_s));
  QFile file(cached_cover_url.toLocalFile());
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.close();
  EXPECT_EQ(cached_cover_url, PlexRequest::CoverUrlForServer(cached_cover_url, server_url));

  // A missing file which is not a cached Plex cover is kept.
  const QUrl other_cover_url = QUrl::fromLocalFile(temp_dir.filePath(u"cover art.jpg"_s));
  EXPECT_EQ(other_cover_url, PlexRequest::CoverUrlForServer(other_cover_url, server_url));

}

// Different servers can have the same thumb paths, so each server has its own cache directory.
TEST(PlexRequestTest, CoverCacheDirForServer) {

  const QUrl server_url(u"https://plex.example:32400"_s);
  EXPECT_TRUE(PlexRequest::CoverCacheDir(u"abc123"_s, server_url).endsWith(u"/abc123"_s));
  EXPECT_NE(PlexRequest::CoverCacheDir(u"abc123"_s, server_url), PlexRequest::CoverCacheDir(u"def456"_s, server_url));

  // Manually configured servers don't have a machine identifier, they are identified by the address, without the token.
  EXPECT_NE(PlexRequest::CoverCacheDir(QString(), server_url), PlexRequest::CoverCacheDir(QString(), QUrl(u"https://other.example:32400"_s)));
  EXPECT_EQ(PlexRequest::CoverCacheDir(QString(), server_url), PlexRequest::CoverCacheDir(QString(), QUrl(u"https://plex.example:32400/?X-Plex-Token=secret"_s)));
  EXPECT_FALSE(PlexRequest::CoverCacheDir(QString(), QUrl(u"https://plex.example:32400/?X-Plex-Token=secret"_s)).contains(u"secret"_s));
  // A machine identifier which can't be used in a directory name is not used.
  EXPECT_FALSE(PlexRequest::CoverCacheDir(u"../abc"_s, server_url).contains(u".."_s));

}

// The server can be behind a reverse proxy with /library/ in its path, the thumb path is the last /library/.
TEST(PlexRequestTest, CoverUrlsWithLibraryInServerPath) {

  const QUrl server_url(u"https://example.com/library/music-server"_s);
  const QUrl cover_url(u"https://example.com/library/music-server/library/metadata/123/thumb/1700000000"_s);
  EXPECT_EQ(cover_url, PlexRequest::CoverUrl(server_url, u"/library/metadata/123/thumb/1700000000"_s));
  EXPECT_EQ(u"metadata-123-thumb-1700000000"_s, PlexRequest::CoverImageId(cover_url));

  // The saved cover URL is moved to the current server without repeating the server path.
  EXPECT_EQ(cover_url, PlexRequest::CoverUrlForServer(QUrl(u"http://192.168.1.2:32400/library/music-server/library/metadata/123/thumb/1700000000?X-Plex-Token=secret"_s), server_url));
  EXPECT_EQ(QUrl(u"https://plex.example:32400/library/metadata/123/thumb/1700000000"_s), PlexRequest::CoverUrlForServer(cover_url, QUrl(u"https://plex.example:32400"_s)));

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  EXPECT_EQ(cover_url, PlexRequest::CoverUrlForServer(QUrl::fromLocalFile(temp_dir.filePath(u"metadata-123-thumb-1700000000.jpg"_s)), server_url));

}

// Older versions saved the access token in the cover URLs, also in the playlists, the database schema update removes it.
TEST(PlexRequestTest, SchemaUpdateRemovesTokenFromCoverUrls) {

  MemoryDatabase database(nullptr);
  QSqlDatabase db = database.Connect();
  QSqlQuery q(db);

  ASSERT_TRUE(q.exec(u"INSERT INTO plex_songs (url, art_automatic) VALUES ('plex:/library/parts/1/file.flac', 'https://plex.example:32400/library/metadata/1/thumb/2?X-Plex-Token=secret')"_s));
  ASSERT_TRUE(q.exec(u"INSERT INTO plex_songs (url, art_automatic) VALUES ('plex:/library/parts/2/file.flac', 'file:///home/user/.cache/strawberry/plexalbumcovers/metadata-2-thumb-3.jpg')"_s));
  ASSERT_TRUE(q.exec(u"INSERT INTO playlist_items (playlist, source, art_automatic) VALUES (1, 13, 'https://plex.example:32400/library/metadata/1/thumb/2?X-Plex-Token=secret')"_s));
  // Other sources are not changed.
  ASSERT_TRUE(q.exec(u"INSERT INTO playlist_items (playlist, source, art_automatic) VALUES (1, 5, 'https://images.example/art.jpg?X-Plex-Token=keep')"_s));

  QFile file(u":/schema/schema-30.sql"_s);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  database.ExecSchemaCommands(db, QString::fromUtf8(file.readAll()), 29);

  ASSERT_TRUE(q.exec(u"SELECT art_automatic FROM plex_songs ORDER BY ROWID"_s));
  ASSERT_TRUE(q.next());
  EXPECT_EQ(u"https://plex.example:32400/library/metadata/1/thumb/2"_s, q.value(0).toString());
  ASSERT_TRUE(q.next());
  EXPECT_EQ(u"file:///home/user/.cache/strawberry/plexalbumcovers/metadata-2-thumb-3.jpg"_s, q.value(0).toString());

  ASSERT_TRUE(q.exec(u"SELECT art_automatic FROM playlist_items ORDER BY ROWID"_s));
  ASSERT_TRUE(q.next());
  EXPECT_EQ(u"https://plex.example:32400/library/metadata/1/thumb/2"_s, q.value(0).toString());
  ASSERT_TRUE(q.next());
  EXPECT_EQ(u"https://images.example/art.jpg?X-Plex-Token=keep"_s, q.value(0).toString());

}

}  // namespace
