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

#include <cstring>

#include "gtest_include.h"

#include <QtGlobal>
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "subsonic/subsonicbaserequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// A finished reply with the given HTTP status and data.
class FinishedNetworkReply : public QNetworkReply {
 public:
  explicit FinishedNetworkReply(const QByteArray &data, const int http_status_code = 200, QObject *parent = nullptr) : QNetworkReply(parent), data_(data), pos_(0) {
    const QNetworkRequest network_request(QUrl(u"https://subsonic.example.org/rest/getAlbumList2.view"_s));
    setRequest(network_request);
    setUrl(network_request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, http_status_code);
    if (http_status_code >= 400) {
      setError(QNetworkReply::ContentNotFoundError, u"Not found"_s);
    }
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    setFinished(true);
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

// Parses replies like the Subsonic requests, ParseJsonObject() doesn't use the service.
class TestSubsonicRequest : public SubsonicBaseRequest {
 public:
  TestSubsonicRequest() : SubsonicBaseRequest(nullptr, SharedPtr<NetworkAccessManager>()) {}

  JsonObjectResult Parse(const QByteArray &data, const int http_status_code = 200) {
    FinishedNetworkReply reply(data, http_status_code);
    return ParseJsonObject(&reply);
  }

 protected:
  void Error(const QString &error, const QVariant &debug = QVariant()) override {
    Q_UNUSED(error)
    Q_UNUSED(debug)
  }
};

// Regression test: Subsonic servers send API errors with HTTP status 200 in "subsonic-response", which was returned as a successful reply, so a wrong password was shown as "Json reply is missing albumList.".
TEST(SubsonicBaseRequestTest, FailedResponseIsAPIError) {

  TestSubsonicRequest request;
  const SubsonicBaseRequest::JsonObjectResult result = request.Parse(R"({"subsonic-response": {"status": "failed", "version": "1.16.1", "error": {"code": 40, "message": "Wrong username or password"}}})");

  EXPECT_FALSE(result.success());
  EXPECT_EQ(SubsonicBaseRequest::ErrorCode::APIError, result.error_code);
  EXPECT_EQ(u"Wrong username or password (40)"_s, result.error_message);
  EXPECT_TRUE(result.json_object.isEmpty());

}

// A failed status is an error also when the server doesn't send an error object.
TEST(SubsonicBaseRequestTest, FailedResponseWithoutErrorIsAPIError) {

  TestSubsonicRequest request;
  const SubsonicBaseRequest::JsonObjectResult result = request.Parse(R"({"subsonic-response": {"status": "failed", "version": "1.16.1"}})");

  EXPECT_FALSE(result.success());
  EXPECT_EQ(SubsonicBaseRequest::ErrorCode::APIError, result.error_code);
  EXPECT_FALSE(result.error_message.isEmpty());

}

// An error without a message still shows the code.
TEST(SubsonicBaseRequestTest, FailedResponseWithoutMessageShowsCode) {

  TestSubsonicRequest request;
  const SubsonicBaseRequest::JsonObjectResult result = request.Parse(R"({"subsonic-response": {"status": "failed", "error": {"code": 70}}})");

  EXPECT_EQ(SubsonicBaseRequest::ErrorCode::APIError, result.error_code);
  EXPECT_TRUE(result.error_message.endsWith(u"(70)"_s)) << result.error_message.toStdString();

}

// A successful reply returns the response object.
TEST(SubsonicBaseRequestTest, OkResponseReturnsResponseObject) {

  TestSubsonicRequest request;
  const SubsonicBaseRequest::JsonObjectResult result = request.Parse(R"({"subsonic-response": {"status": "ok", "version": "1.16.1", "albumList2": {"album": []}}})");

  EXPECT_TRUE(result.success()) << result.error_message.toStdString();
  EXPECT_TRUE(result.json_object.contains("albumList2"_L1));

}

// An error object outside "subsonic-response" is still an API error.
TEST(SubsonicBaseRequestTest, TopLevelErrorIsAPIError) {

  TestSubsonicRequest request;
  const SubsonicBaseRequest::JsonObjectResult result = request.Parse(R"({"error": {"code": 10, "message": "Required parameter is missing"}})");

  EXPECT_EQ(SubsonicBaseRequest::ErrorCode::APIError, result.error_code);
  EXPECT_EQ(u"Required parameter is missing (10)"_s, result.error_message);

}

// A failed response sent with an HTTP error keeps the server's message instead of the HTTP error.
TEST(SubsonicBaseRequestTest, FailedResponseWithHttpErrorShowsServerMessage) {

  TestSubsonicRequest request;
  const SubsonicBaseRequest::JsonObjectResult result = request.Parse(R"({"subsonic-response": {"status": "failed", "error": {"code": 70, "message": "Album not found"}}})", 404);

  EXPECT_EQ(SubsonicBaseRequest::ErrorCode::APIError, result.error_code);
  EXPECT_EQ(u"Album not found (70)"_s, result.error_message);

}

}  // namespace
