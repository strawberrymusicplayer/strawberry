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

#include <memory>

#include "gtest_include.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QUrl>
#include <QSignalSpy>
#include <QHostAddress>
#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QTcpServer>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "core/urlhandlers.h"
#include "covermanager/albumcoverloader.h"
#include "credentialsmanager/credentialsmanager.h"
#include "credentialsmanager/credentialsreply.h"
#include "constants/subsonicsettings.h"
#include "subsonic/subsonicservice.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class SubsonicServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {

    // Settings without an organization name always have an access error, which makes the settings backend of the credentials manager fail to save passwords.
    organization_name_ = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(u"Strawberry"_s);

    // The settings backend stores nothing for Subsonic, so the password is read as not found, and a released songs request fails at once with a missing password instead of going to the network.
    Settings s;
    s.beginGroup(CredentialsManager::kSettingsGroup);
    s.setValue(CredentialsManager::kBackend, CredentialsManager::BackendToString(CredentialsManager::BackendType::Settings));
    s.endGroup();
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.remove(SubsonicSettings::kPassword);
    s.setValue(SubsonicSettings::kEnabled, true);
    s.setValue(SubsonicSettings::kUrl, QUrl(u"http://localhost:1"_s));
    s.setValue(SubsonicSettings::kUsername, u"user"_s);
    s.endGroup();

    database_ = make_shared<MemoryDatabase>(nullptr);
    // The tests save and delete passwords with the credentials manager of the service, so they are queued after the requests of the service.
    credentials_manager_ = make_shared<CredentialsManager>();
    service_ = std::make_unique<SubsonicService>(make_shared<TaskManager>(), database_, make_shared<NetworkAccessManager>(), credentials_manager_, make_shared<UrlHandlers>(), make_shared<AlbumCoverLoader>(nullptr));

  }

  void TearDown() override {
    service_.reset();
    QCoreApplication::setOrganizationName(organization_name_);
  }

  // Processes events until the password is loaded, returns false on timeout.
  bool WaitForPassword(const int timeout_msec = 10000) const {
    QElapsedTimer timer;
    timer.start();
    while (service_->password_loading()) {
      if (timer.elapsed() > timeout_msec) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    // Let queued signals from the password reply finish.
    QCoreApplication::processEvents();
    return true;
  }

  QString organization_name_;
  SharedPtr<MemoryDatabase> database_;
  SharedPtr<CredentialsManager> credentials_manager_;
  std::unique_ptr<SubsonicService> service_;
};

TEST_F(SubsonicServiceTest, GetSongsWaitsForPassword) {

  ASSERT_TRUE(service_->password_loading());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  EXPECT_EQ(spy.count(), 0);

  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(1).toString(), u"Missing username or password."_s);

}

TEST_F(SubsonicServiceTest, AbortWhilePasswordLoadingCancelsGetSongs) {

  ASSERT_TRUE(service_->password_loading());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  service_->ResetSongsRequest();

  ASSERT_TRUE(WaitForPassword());
  EXPECT_EQ(spy.count(), 0);

}

// Waits for a credentials reply, returns false on timeout or failure.
bool WaitForCredentialsReply(const CredentialsReplyPtr &reply) {
  QSignalSpy spy(&*reply, &CredentialsReply::Finished);
  return spy.wait(10000) && reply->success();
}

// Applying the settings while a collection refresh is running must not send the requests that follow with an empty password.
TEST_F(SubsonicServiceTest, ReloadSettingsKeepsPasswordWhileLoading) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));

  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_EQ(service_->password(), u"secret"_s);
  ASSERT_TRUE(WaitForPassword());
  EXPECT_EQ(service_->password(), u"secret"_s);

  // A password which was deleted is cleared when it has been read.
  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));
  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_EQ(service_->password(), u"secret"_s);
  ASSERT_TRUE(WaitForPassword());
  EXPECT_TRUE(service_->password().isEmpty());

}

// The password is cleared when Subsonic is disabled, since it's not read then.
TEST_F(SubsonicServiceTest, ReloadSettingsClearsPasswordWhenDisabled) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kEnabled, false);
    s.endGroup();
  }
  service_->ReloadSettings();
  EXPECT_FALSE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A password in the settings file from an older version is used at once, and migrated to the credentials manager.
TEST_F(SubsonicServiceTest, ReloadSettingsUsesPasswordFromSettingsFile) {

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kPassword, QString::fromLatin1(QByteArray("old_secret").toBase64()));
    s.endGroup();
  }

  service_->ReloadSettings();
  EXPECT_FALSE(service_->password_loading());
  EXPECT_EQ(service_->password(), u"old_secret"_s);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// The password from the settings file is migrated, but not used, when Subsonic is disabled.
TEST_F(SubsonicServiceTest, ReloadSettingsDoesNotUsePasswordFromSettingsFileWhenDisabled) {

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kEnabled, false);
    s.setValue(SubsonicSettings::kPassword, QString::fromLatin1(QByteArray("old_secret").toBase64()));
    s.endGroup();
  }

  service_->ReloadSettings();
  EXPECT_FALSE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());

  // The read is queued after the migration.
  CredentialsReplyPtr read_reply = credentials_manager_->ReadPasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService));
  ASSERT_TRUE(WaitForCredentialsReply(read_reply));
  EXPECT_EQ(read_reply->password(), u"old_secret"_s);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// The password for the previous server must not be sent to a new server while the password is read again.
TEST_F(SubsonicServiceTest, ReloadSettingsClearsPasswordWhenServerChanges) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(u"http://localhost:2"_s));
    s.endGroup();
  }
  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());
  ASSERT_TRUE(WaitForPassword());
  EXPECT_EQ(service_->password(), u"secret"_s);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// The password for the previous user must not be sent for a new user while the password is read again.
TEST_F(SubsonicServiceTest, ReloadSettingsClearsPasswordWhenUsernameChanges) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUsername, u"other_user"_s);
    s.endGroup();
  }
  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());
  ASSERT_TRUE(WaitForPassword());

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A collection refresh which is running when the server is changed is stopped, so it doesn't send the next requests for the previous server to the new server.
TEST_F(SubsonicServiceTest, ReloadSettingsStopsRefreshWhenServerChanges) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  // The refresh is started and the server is changed before the first reply arrives.
  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  ASSERT_EQ(spy.count(), 0);
  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(u"http://localhost:2"_s));
    s.endGroup();
  }
  service_->ReloadSettings();
  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<SongMap>().isEmpty());
  EXPECT_FALSE(spy.at(0).at(1).toString().isEmpty());

  // The stopped refresh doesn't send results anymore.
  ASSERT_TRUE(WaitForPassword());
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  EXPECT_EQ(spy.count(), 1);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A collection refresh which is running when Subsonic is disabled is stopped, since the password is cleared.
TEST_F(SubsonicServiceTest, ReloadSettingsStopsRefreshWhenDisabled) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  ASSERT_EQ(spy.count(), 0);
  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kEnabled, false);
    s.endGroup();
  }
  service_->ReloadSettings();
  EXPECT_EQ(spy.count(), 1);
  EXPECT_TRUE(service_->password().isEmpty());

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A collection refresh which was kept running while the password was read again is stopped when the password isn't found anymore.
TEST_F(SubsonicServiceTest, ReloadSettingsStopsRefreshWhenPasswordIsNotFound) {

  // The server accepts the requests, but doesn't reply, so the refresh keeps running.
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    s.endGroup();
  }

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));
  ASSERT_EQ(spy.count(), 0);

  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_EQ(spy.count(), 0);
  ASSERT_TRUE(WaitForPassword());
  EXPECT_TRUE(service_->password().isEmpty());
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(1).toString(), u"Missing username or password."_s);

}

// Applying the settings without changing the server keeps a running collection refresh.
TEST_F(SubsonicServiceTest, ReloadSettingsKeepsRefreshWhenServerIsUnchanged) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  service_->ReloadSettings();
  EXPECT_EQ(spy.count(), 0);

  // The refresh fails, since there's no server, but it's the result of the refresh which was kept running.
  EXPECT_TRUE(spy.wait(10000));
  service_->ResetSongsRequest();

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A Subsonic server with albums with one song each, which can ignore the offset of the album list like some servers do.
class FakeSubsonicServer : public QTcpServer {
 public:
  explicit FakeSubsonicServer(const int albums, const bool ignore_offset, const QJsonValue &song_duration = 60) : albums_(albums), ignore_offset_(ignore_offset), song_duration_(song_duration), album_list_requests_(0) {
    QObject::connect(this, &QTcpServer::newConnection, this, [this]() {
      while (QTcpSocket *socket = nextPendingConnection()) {
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() { ReadRequests(socket); });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      }
    });
  }

  int album_list_requests() const { return album_list_requests_; }
  const QStringList &album_requests() const { return album_requests_; }

  // Leaves out values which are optional in the Subsonic API, like servers which don't send them.
  void set_omitted_album_fields(const QStringList &fields) { omitted_album_fields_ = fields; }
  void set_omitted_song_fields(const QStringList &fields) { omitted_song_fields_ = fields; }
  void set_album_without_songs(const QString &album_id) { album_without_songs_ = album_id; }
  void set_album_with_empty_song_array(const QString &album_id) { album_with_empty_song_array_ = album_id; }

 private:
  void ReadRequests(QTcpSocket *socket) {

    QByteArray &buffer = buffers_[socket];
    buffer += socket->readAll();
    qsizetype end = 0;
    while ((end = buffer.indexOf("\r\n\r\n")) != -1) {
      const QByteArray request_line = buffer.left(buffer.indexOf("\r\n"));
      buffer.remove(0, end + 4);
      const QUrl url(QString::fromLatin1(request_line.split(' ').value(1)));
      const QByteArray data = QJsonDocument(QJsonObject{{u"subsonic-response"_s, OmitFields(Reply(url))}}).toJson(QJsonDocument::Compact);
      socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(data.size()) + "\r\n\r\n" + data);
    }

  }

  QJsonObject OmitFields(QJsonObject response) const {

    if (response.contains("albumList2"_L1)) {
      QJsonArray albums;
      const QJsonArray albums_reply = response["albumList2"_L1].toObject()["album"_L1].toArray();
      for (const QJsonValue &value_album : albums_reply) {
        QJsonObject object_album = value_album.toObject();
        for (const QString &field : omitted_album_fields_) object_album.remove(field);
        albums << object_album;
      }
      response.insert(u"albumList2"_s, QJsonObject{{u"album"_s, albums}});
    }

    if (response.contains("album"_L1)) {
      QJsonObject object_album = response["album"_L1].toObject();
      if (object_album["id"_L1].toString() == album_without_songs_) {
        object_album.remove(u"song"_s);
      }
      else if (object_album["id"_L1].toString() == album_with_empty_song_array_) {
        object_album.insert(u"song"_s, QJsonArray());
      }
      else {
        QJsonArray songs;
        const QJsonArray songs_reply = object_album["song"_L1].toArray();
        for (const QJsonValue &value_song : songs_reply) {
          QJsonObject object_song = value_song.toObject();
          for (const QString &field : omitted_song_fields_) object_song.remove(field);
          songs << object_song;
        }
        object_album.insert(u"song"_s, songs);
      }
      response.insert(u"album"_s, object_album);
    }

    return response;

  }

  QJsonObject Reply(const QUrl &url) {

    const QUrlQuery query(url);
    QJsonObject response{{u"status"_s, u"ok"_s}, {u"version"_s, u"1.16.1"_s}};

    if (url.path().endsWith("/getAlbumList2.view"_L1)) {
      ++album_list_requests_;
      const int size = query.queryItemValue(u"size"_s).toInt();
      const int offset = ignore_offset_ ? 0 : query.queryItemValue(u"offset"_s).toInt();
      QJsonArray albums;
      for (int i = offset; i < albums_ && i < offset + size; ++i) {
        albums << QJsonObject{{u"id"_s, QStringLiteral("album-%1").arg(i)}, {u"name"_s, QStringLiteral("Album %1").arg(i)}, {u"artist"_s, u"Artist"_s}};
      }
      response.insert(u"albumList2"_s, QJsonObject{{u"album"_s, albums}});
    }
    else if (url.path().endsWith("/getAlbum.view"_L1)) {
      album_requests_ << query.queryItemValue(u"id"_s);
      const QString album_id = query.queryItemValue(u"id"_s);
      const QJsonObject song{{u"id"_s, QString(u"song-"_s + album_id)}, {u"title"_s, u"Song"_s}, {u"album"_s, album_id}, {u"artist"_s, u"Artist"_s}, {u"size"_s, 1000}, {u"suffix"_s, u"flac"_s}, {u"duration"_s, song_duration_}, {u"type"_s, u"music"_s}};
      response.insert(u"album"_s, QJsonObject{{u"id"_s, album_id}, {u"song"_s, QJsonArray{song}}});
    }

    return response;

  }

  const int albums_;
  const bool ignore_offset_;
  const QJsonValue song_duration_;
  int album_list_requests_;
  QStringList album_requests_;
  QStringList omitted_album_fields_;
  QStringList omitted_song_fields_;
  QString album_without_songs_;
  QString album_with_empty_song_array_;
  QHash<QTcpSocket*, QByteArray> buffers_;
};

// Refreshes the collection from the server, returns the number of songs, or -1 when the refresh failed or timed out.
int RefreshFromServer(SubsonicService *service, const SharedPtr<CredentialsManager> &credentials_manager, const FakeSubsonicServer &server, SongMap *songs = nullptr) {

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    s.setValue(SubsonicSettings::kDownloadAlbumCovers, false);
    s.endGroup();
  }

  if (!WaitForCredentialsReply(credentials_manager->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s))) return -1;
  service->ReloadSettings();

  QSignalSpy spy(service, &SubsonicService::SongsResults);
  service->GetSongs();
  const bool finished = spy.wait(20000);
  service->ResetSongsRequest();

  if (!WaitForCredentialsReply(credentials_manager->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService)))) return -1;

  if (!finished || !spy.at(0).at(1).toString().isEmpty()) return -1;
  if (songs) *songs = spy.at(0).at(0).value<SongMap>();
  return static_cast<int>(spy.at(0).at(0).value<SongMap>().count());

}

// Regression test: when the server ignored the offset of the album list, it sent the first page again for every page, and the next page was requested forever.
TEST_F(SubsonicServiceTest, AlbumListPagingStopsWhenServerIgnoresOffset) {

  FakeSubsonicServer server(600, true);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

  EXPECT_EQ(500, RefreshFromServer(&*service_, credentials_manager_, server));
  EXPECT_EQ(2, server.album_list_requests());
  EXPECT_EQ(500, server.album_requests().count());

}

// The album list is still requested page by page, until a page isn't full.
TEST_F(SubsonicServiceTest, AlbumListIsPaged) {

  FakeSubsonicServer server(1003, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

  EXPECT_EQ(1003, RefreshFromServer(&*service_, credentials_manager_, server));
  EXPECT_EQ(3, server.album_list_requests());
  EXPECT_EQ(1003, server.album_requests().count());
  EXPECT_TRUE(server.album_requests().contains(u"album-1002"_s));

}

// Regression test: the duration was read as an integer, so a duration with decimals, which some servers send, became 0.
TEST_F(SubsonicServiceTest, FractionalDurationIsKept) {

  FakeSubsonicServer server(1, false, 245.3);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

  SongMap songs;
  ASSERT_EQ(1, RefreshFromServer(&*service_, credentials_manager_, server, &songs));
  EXPECT_EQ(245'300'000'000LL, songs.first().length_nanosec());

}

// Some servers send the duration as a string, 4.1 seconds in nanoseconds is just below 4100000000 as a double, so it's rounded.
TEST_F(SubsonicServiceTest, FractionalDurationStringIsKept) {

  FakeSubsonicServer server(1, false, u"4.1"_s);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

  SongMap songs;
  ASSERT_EQ(1, RefreshFromServer(&*service_, credentials_manager_, server, &songs));
  EXPECT_EQ(4'100'000'000LL, songs.first().length_nanosec());

}

// Whole seconds, sent by most servers, are still read.
TEST_F(SubsonicServiceTest, DurationIsRead) {

  FakeSubsonicServer server(1, false, 60);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

  SongMap songs;
  ASSERT_EQ(1, RefreshFromServer(&*service_, credentials_manager_, server, &songs));
  EXPECT_EQ(60'000'000'000LL, songs.first().length_nanosec());

}

// Regression test: songs without values which are optional in the Subsonic API were skipped with an error.
TEST_F(SubsonicServiceTest, SongWithoutOptionalValuesIsAdded) {

  FakeSubsonicServer server(1, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_omitted_song_fields(QStringList() << u"size"_s << u"suffix"_s << u"duration"_s << u"type"_s << u"album"_s << u"artist"_s);

  EXPECT_EQ(1, RefreshFromServer(&*service_, credentials_manager_, server));

}

// Regression test: albums without an artist, which is optional in the Subsonic API, were skipped with an error.
TEST_F(SubsonicServiceTest, AlbumWithoutArtistIsAdded) {

  FakeSubsonicServer server(1, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_omitted_album_fields(QStringList() << u"artist"_s);

  EXPECT_EQ(1, RefreshFromServer(&*service_, credentials_manager_, server));
  EXPECT_EQ(QStringList() << u"album-0"_s, server.album_requests());

}

// Regression test: an album without songs has no song array, which was an error.
TEST_F(SubsonicServiceTest, AlbumWithoutSongsIsNotAnError) {

  FakeSubsonicServer server(2, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_album_without_songs(u"album-0"_s);

  EXPECT_EQ(1, RefreshFromServer(&*service_, credentials_manager_, server));
  EXPECT_EQ(2, server.album_requests().count());

}

// Review: a collection with only albums without songs is empty, not an unknown error.
TEST_F(SubsonicServiceTest, CollectionWithAlbumWithoutSongsIsEmpty) {

  FakeSubsonicServer server(1, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_album_without_songs(u"album-0"_s);

  EXPECT_EQ(0, RefreshFromServer(&*service_, credentials_manager_, server));
  EXPECT_EQ(QStringList() << u"album-0"_s, server.album_requests());

}

// The same for an album with an empty song array.
TEST_F(SubsonicServiceTest, CollectionWithAlbumWithEmptySongArrayIsEmpty) {

  FakeSubsonicServer server(1, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_album_with_empty_song_array(u"album-0"_s);

  EXPECT_EQ(0, RefreshFromServer(&*service_, credentials_manager_, server));

}

// An album without songs doesn't hide the errors of the other albums.
TEST_F(SubsonicServiceTest, AlbumWithoutSongsKeepsErrors) {

  FakeSubsonicServer server(2, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_album_without_songs(u"album-0"_s);
  server.set_omitted_song_fields(QStringList() << u"title"_s);

  EXPECT_EQ(-1, RefreshFromServer(&*service_, credentials_manager_, server));
  EXPECT_EQ(2, server.album_requests().count());

}

// Songs without an ID or a title are still skipped.
TEST_F(SubsonicServiceTest, SongWithoutTitleIsAnError) {

  FakeSubsonicServer server(1, false);
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  server.set_omitted_song_fields(QStringList() << u"title"_s);

  EXPECT_EQ(-1, RefreshFromServer(&*service_, credentials_manager_, server));

}

}  // namespace
