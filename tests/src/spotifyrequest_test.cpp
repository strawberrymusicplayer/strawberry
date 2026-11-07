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

#include "config.h"

#include <memory>

#include "gtest_include.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QMap>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/taskmanager.h"
#include "core/memorydatabase.h"
#include "core/networkaccessmanager.h"
#include "constants/spotifysettings.h"
#include "spotify/spotifyservice.h"
#include "spotify/spotifyrequest.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// A finished reply with the given data, finished() is emitted from the event loop like for a real reply.
class FakeNetworkReply : public QNetworkReply {
 public:
  explicit FakeNetworkReply(const QNetworkRequest &network_request, const QByteArray &data, QObject *parent = nullptr) : QNetworkReply(parent), data_(data), pos_(0) {
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    setFinished(true);
    QTimer::singleShot(0, this, &QNetworkReply::finished);
  }

  void abort() override {}
  qint64 bytesAvailable() const override { return data_.size() - pos_ + QNetworkReply::bytesAvailable(); }

 protected:
  qint64 readData(char *data, const qint64 maxlen) override {
    const qint64 len = qMin(maxlen, data_.size() - pos_);
    if (len <= 0) return -1;
    memcpy(data, data_.constData() + pos_, static_cast<size_t>(len));
    pos_ += len;
    return len;
  }

 private:
  const QByteArray data_;
  qint64 pos_;
};

// Answers the Spotify API requests: the followed artists in pages of 5 with a cursor and without an offset, like the me/following endpoint, the replies set for a path, and no albums for the other artists.
class FakeSpotifyNetworkAccessManager : public NetworkAccessManager {
 public:
  static constexpr int kArtistsPageSize = 5;

  int artists_total = 12;
  // Replies for the paths ending with the key.
  QMap<QString, QByteArray> replies;
  QList<QUrl> requested_urls;

  // Returns the IDs of the albums the songs were requested for.
  QStringList AlbumSongsRequested() const {
    QStringList album_ids;
    static const QRegularExpression regex_album_songs(u"/albums/([^/]+)/tracks$"_s);
    for (const QUrl &url : requested_urls) {
      const QRegularExpressionMatch match = regex_album_songs.match(url.path());
      if (match.hasMatch()) album_ids << match.captured(1);
    }
    album_ids.sort();
    return album_ids;
  }

 protected:
  QNetworkReply *createRequest(const Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {

    Q_UNUSED(op)
    Q_UNUSED(outgoing_data)

    const QUrl url = network_request.url();
    requested_urls << url;

    QByteArray data = "{}";
    for (QMap<QString, QByteArray>::const_iterator it = replies.constBegin(); it != replies.constEnd(); ++it) {
      if (url.path().endsWith(it.key())) {
        return new FakeNetworkReply(network_request, it.value(), this);
      }
    }
    if (url.path().endsWith("/me/following"_L1)) {
      data = FollowingPage(QUrlQuery(url).queryItemValue(u"after"_s));
    }
    else if (url.path().endsWith("/albums"_L1)) {
      data = R"({"items": [], "limit": 50, "offset": 0, "total": 0, "next": null})";
    }

    return new FakeNetworkReply(network_request, data, this);

  }

 private:
  // The cursor is the ID of the last artist of the previous page.
  QByteArray FollowingPage(const QString &after) const {

    const int first = after.isEmpty() ? 0 : after.mid(6).toInt() + 1;
    const int last = qMin(first + kArtistsPageSize, artists_total) - 1;

    QJsonArray items;
    for (int i = first; i <= last; ++i) {
      QJsonObject artist;
      artist["id"_L1] = QStringLiteral("artist%1").arg(i);
      artist["name"_L1] = QStringLiteral("Artist %1").arg(i);
      artist["genres"_L1] = QJsonArray();
      items << artist;
    }

    QJsonObject cursors;
    QJsonObject artists;
    artists["items"_L1] = items;
    artists["total"_L1] = artists_total;
    artists["limit"_L1] = kArtistsPageSize;
    if (last < artists_total - 1) {
      cursors["after"_L1] = QStringLiteral("artist%1").arg(last);
      artists["next"_L1] = QStringLiteral("https://api.spotify.com/v1/me/following?type=artist&after=artist%1&limit=%2").arg(last).arg(kArtistsPageSize);
    }
    else {
      cursors["after"_L1] = QJsonValue::Null;
      artists["next"_L1] = QJsonValue::Null;
    }
    artists["cursors"_L1] = cursors;

    QJsonObject object;
    object["artists"_L1] = artists;
    return QJsonDocument(object).toJson(QJsonDocument::Compact);

  }
};

// Returns the "artists" object of a reply from the me/following endpoint.
QJsonObject FollowingArtists(const QByteArray &json) {

  return QJsonDocument::fromJson(json).object()["artists"_L1].toObject();

}

// Regression test: the followed artists were paged with an offset, which the me/following endpoint doesn't support, so only the first 20 artists were received.
TEST(SpotifyRequestTest, NextArtistsCursorFromFollowedArtists) {

  const QJsonObject artists = FollowingArtists(R"({"artists": {"items": [], "next": "https://api.spotify.com/v1/me/following?type=artist&after=0I2XqVXqHScXjHhk6AYYRe&limit=20", "total": 45, "cursors": {"after": "0I2XqVXqHScXjHhk6AYYRe"}, "limit": 20, "href": "https://api.spotify.com/v1/me/following?type=artist&limit=20"}})");
  EXPECT_EQ(u"0I2XqVXqHScXjHhk6AYYRe"_s, SpotifyRequest::NextArtistsCursor(artists));

}

TEST(SpotifyRequestTest, NoNextArtistsCursorOnLastPage) {

  // On the last page, the next URL and the cursor are null.
  EXPECT_TRUE(SpotifyRequest::NextArtistsCursor(FollowingArtists(R"({"artists": {"items": [], "next": null, "total": 45, "cursors": {"after": null}, "limit": 20}})")).isEmpty());
  // The cursor is only followed when there is a next page.
  EXPECT_TRUE(SpotifyRequest::NextArtistsCursor(FollowingArtists(R"({"artists": {"items": [], "next": null, "total": 45, "cursors": {"after": "abc"}, "limit": 20}})")).isEmpty());
  // Search replies are paged with an offset, and don't have a cursor.
  EXPECT_TRUE(SpotifyRequest::NextArtistsCursor(FollowingArtists(R"({"artists": {"items": [], "next": "https://api.spotify.com/v1/search?offset=20&limit=20", "total": 45, "offset": 0, "limit": 20}})")).isEmpty());

}

// Lets the service use a fake session, the test settings are in a temporary directory, so the access token is not saved anywhere.
// The session is cleared without client credentials, so custom credentials are set, then the test doesn't depend on the credentials compiled in.
void SetFakeSpotifySession() {

  Settings s;
  s.beginGroup(QLatin1String(SpotifySettings::kSettingsGroup));
  s.setValue(QLatin1String(SpotifySettings::kUseCustomApiCredentials), true);
  s.setValue(QLatin1String(SpotifySettings::kClientId), u"client_id"_s);
  s.setValue(QLatin1String(SpotifySettings::kClientSecret), u"client_secret"_s);
  s.setValue(u"token_type"_s, u"Bearer"_s);
  s.setValue(u"access_token"_s, u"token"_s);
  s.endGroup();

}

// Regression test: the next pages of followed artists were requested with an offset, which the me/following endpoint ignores, and the replies were rejected since they don't have an offset, so only the first 20 artists were received.
TEST(SpotifyRequestTest, FollowedArtistsArePagedWithCursor) {

  SetFakeSpotifySession();

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<MemoryDatabase> database = make_shared<MemoryDatabase>(task_manager);
  SharedPtr<FakeSpotifyNetworkAccessManager> network = make_shared<FakeSpotifyNetworkAccessManager>();
  SpotifyService service(task_manager, database, network, nullptr);
  ASSERT_TRUE(service.authenticated());

  SpotifyRequest request(&service, network, SpotifyRequest::Type::FavouriteArtists, nullptr);
  QSignalSpy spy(&request, &SpotifyRequest::Results);
  request.Process();
  ASSERT_TRUE(spy.wait(30000));

  QStringList following_afters;
  QSet<QString> artist_album_ids;
  static const QRegularExpression regex_artist_albums(u"/artists/([^/]+)/albums$"_s);
  for (const QUrl &url : std::as_const(network->requested_urls)) {
    if (url.path().endsWith("/me/following"_L1)) {
      const QUrlQuery url_query(url);
      EXPECT_FALSE(url_query.hasQueryItem(u"offset"_s)) << url.toString().toStdString();
      following_afters << url_query.queryItemValue(u"after"_s);
    }
    const QRegularExpressionMatch match = regex_artist_albums.match(url.path());
    if (match.hasMatch()) {
      artist_album_ids << match.captured(1);
    }
  }

  // All three pages are requested with the cursor from the previous page, and the requests stop after the last page.
  EXPECT_EQ(QStringList() << QString() << u"artist4"_s << u"artist9"_s, following_afters);
  // The albums are requested for all the artists from all the pages.
  EXPECT_EQ(network->artists_total, artist_album_ids.count());
  EXPECT_FALSE(spy.first().value(2).toString().contains(u"Offset"_s)) << spy.first().value(2).toString().toStdString();

}

// Regression test: an album by another artist ended the processing of the reply, so the next albums of the artist were lost, and only the first listed artist was compared, so albums where the artist is listed after another artist were skipped.
TEST(SpotifyRequestTest, ArtistAlbumsByOtherArtistsAreSkipped) {

  SetFakeSpotifySession();

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<MemoryDatabase> database = make_shared<MemoryDatabase>(task_manager);
  SharedPtr<FakeSpotifyNetworkAccessManager> network = make_shared<FakeSpotifyNetworkAccessManager>();
  network->artists_total = 1;
  network->replies.insert(u"/artists/artist0/albums"_s, R"({"items": [)"
                                                        R"({"id": "other", "name": "Other", "images": [], "artists": [{"id": "artist9", "name": "Artist 9"}]},)"
                                                        R"({"id": "featured", "name": "Featured", "images": [], "artists": [{"id": "artist9", "name": "Artist 9"}, {"id": "artist0", "name": "Artist 0"}]},)"
                                                        R"({"id": "own", "name": "Own", "images": [], "artists": [{"id": "artist0", "name": "Artist 0"}]})"
                                                        R"(], "limit": 50, "offset": 0, "total": 3, "next": null})");
  SpotifyService service(task_manager, database, network, nullptr);
  ASSERT_TRUE(service.authenticated());

  SpotifyRequest request(&service, network, SpotifyRequest::Type::FavouriteArtists, nullptr);
  QSignalSpy spy(&request, &SpotifyRequest::Results);
  request.Process();
  ASSERT_TRUE(spy.wait(30000));

  // The songs are requested for the albums of the artist, also the one listing the artist after another artist, and not for the album by the other artist.
  EXPECT_EQ(QStringList() << u"featured"_s << u"own"_s, network->AlbumSongsRequested());

}

}  // namespace
