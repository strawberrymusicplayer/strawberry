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
#include <functional>

#include "gtest_include.h"

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/taskmanager.h"
#include "core/memorydatabase.h"
#include "core/networkaccessmanager.h"
#include "core/urlhandlers.h"

#ifdef HAVE_TIDAL
#  include "constants/tidalsettings.h"
#  include "tidal/tidalservice.h"
#  include "tidal/tidalurlhandler.h"
#  include "tidal/tidalrequest.h"
#endif

#ifdef HAVE_QOBUZ
#  include "constants/qobuzsettings.h"
#  include "qobuz/qobuzservice.h"
#  include "qobuz/qobuzurlhandler.h"
#  include "qobuz/qobuzrequest.h"
#endif

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

// Answers the favourite songs requests with a total of 5 songs, but only the first page has songs, the next pages are empty, like when the total counts songs which are not available.
class FakeEmptyPageNetworkAccessManager : public NetworkAccessManager {
 public:
  static constexpr int kTotal = 5;
  static constexpr int kFirstPageCount = 2;

  // The path the songs are requested from, and a function which makes the reply for an offset and the songs.
  QString songs_path;
  std::function<QByteArray(const int offset, const QJsonArray &items)> make_reply;
  // The item for a song in the reply.
  QJsonValue song_item;
  // The offsets the songs were requested with.
  QList<int> requested_offsets;

 protected:
  QNetworkReply *createRequest(const Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override {

    Q_UNUSED(op)
    Q_UNUSED(outgoing_data)

    const QUrl url = network_request.url();
    if (!url.path().endsWith(songs_path)) {
      return new FakeNetworkReply(network_request, "{}", this);
    }

    const int offset = QUrlQuery(url).queryItemValue(u"offset"_s).toInt();
    requested_offsets << offset;

    QJsonArray items;
    if (offset == 0) {
      for (int i = 0; i < kFirstPageCount; ++i) items << song_item;
    }

    return new FakeNetworkReply(network_request, make_reply(offset, items), this);

  }
};

#ifdef HAVE_TIDAL

// Regression test: an empty page didn't move the offset, so the same page was requested again, forever when the total counts songs which are not returned.
TEST(StreamingRequestTest, TidalEmptyPageIsNotRequestedAgain) {

  {
    Settings s;
    s.remove(QLatin1String(TidalSettings::kSettingsGroup));
    s.beginGroup(QLatin1String(TidalSettings::kSettingsGroup));
    s.setValue(QLatin1String(TidalSettings::kUseCustomClientId), true);
    s.setValue(QLatin1String(TidalSettings::kClientId), u"client_id"_s);
    s.setValue(u"token_type"_s, u"Bearer"_s);
    s.setValue(u"access_token"_s, u"token"_s);
    s.setValue(u"user_id"_s, 1);
    s.endGroup();
  }

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<MemoryDatabase> database = make_shared<MemoryDatabase>(task_manager);
  SharedPtr<FakeEmptyPageNetworkAccessManager> network = make_shared<FakeEmptyPageNetworkAccessManager>();
  network->songs_path = u"/favorites/tracks"_s;
  network->song_item = QJsonObject{ { u"item"_s, QJsonObject() } };
  network->make_reply = [](const int offset, const QJsonArray &items) {
    QJsonObject object;
    object["limit"_L1] = 100;
    object["offset"_L1] = offset;
    object["totalNumberOfItems"_L1] = FakeEmptyPageNetworkAccessManager::kTotal;
    object["items"_L1] = items;
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
  };

  TidalService service(task_manager, database, network, make_shared<UrlHandlers>(), nullptr);
  ASSERT_TRUE(service.authenticated());
  TidalUrlHandler url_handler(task_manager, &service);

  TidalRequest request(&service, &url_handler, network, TidalRequest::Type::FavouriteSongs, nullptr);
  QSignalSpy spy(&request, &TidalRequest::Results);
  request.Process();

  EXPECT_TRUE(spy.wait(5000));
  EXPECT_EQ(QList<int>() << 0 << FakeEmptyPageNetworkAccessManager::kFirstPageCount, network->requested_offsets);

}

#endif  // HAVE_TIDAL

#ifdef HAVE_QOBUZ

// Regression test: an empty page didn't move the offset, so the same page was requested again, forever when the total counts songs which are not returned.
TEST(StreamingRequestTest, QobuzEmptyPageIsNotRequestedAgain) {

  {
    Settings s;
    s.remove(QLatin1String(QobuzSettings::kSettingsGroup));
    s.beginGroup(QLatin1String(QobuzSettings::kSettingsGroup));
    s.setValue(QLatin1String(QobuzSettings::kAppId), u"app_id"_s);
    s.setValue(QLatin1String(QobuzSettings::kAppSecret), u"app_secret"_s);
    s.setValue(QLatin1String(QobuzSettings::kUserAuthToken), u"token"_s);
    s.endGroup();
  }

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<MemoryDatabase> database = make_shared<MemoryDatabase>(task_manager);
  SharedPtr<FakeEmptyPageNetworkAccessManager> network = make_shared<FakeEmptyPageNetworkAccessManager>();
  network->songs_path = u"/favorite/getUserFavorites"_s;
  network->song_item = QJsonObject();
  network->make_reply = [](const int offset, const QJsonArray &items) {
    QJsonObject tracks;
    tracks["limit"_L1] = 100;
    tracks["offset"_L1] = offset;
    tracks["total"_L1] = FakeEmptyPageNetworkAccessManager::kTotal;
    tracks["items"_L1] = items;
    QJsonObject object;
    object["tracks"_L1] = tracks;
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
  };

  QobuzService service(task_manager, database, network, make_shared<UrlHandlers>(), nullptr);
  ASSERT_TRUE(service.authenticated());
  QobuzUrlHandler url_handler(task_manager, &service);

  QobuzRequest request(&service, &url_handler, network, QobuzRequest::Type::FavouriteSongs, nullptr);
  QSignalSpy spy(&request, &QobuzRequest::Results);
  request.Process();

  EXPECT_TRUE(spy.wait(5000));
  EXPECT_EQ(QList<int>() << 0 << FakeEmptyPageNetworkAccessManager::kFirstPageCount, network->requested_offsets);

}

#endif  // HAVE_QOBUZ

}  // namespace
