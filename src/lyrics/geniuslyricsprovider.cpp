/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <memory>

#include <QApplication>
#include <QThread>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QMessageBox>
#include <QMutexLocker>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/oauthenticator.h"
#include "jsonlyricsprovider.h"
#include "htmllyricsprovider.h"
#include "geniuslyricsprovider.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {
constexpr char kSettingsGroup[] = "GeniusLyrics";
constexpr char kOAuthAuthorizeUrl[] = "https://api.genius.com/oauth/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://api.genius.com/oauth/token";
constexpr char kOAuthRedirectUrl[] = "http://localhost:63111/";  // Genius does not accept a random port number. This port must match the URL of the ClientID.
constexpr char kOAuthScope[] = "me";
constexpr char kUrlSearch[] = "https://api.genius.com/search/";
constexpr char kClientIDB64[] = "RUNTNXU4U1VyMU1KUU5hdTZySEZteUxXY2hkanFiY3lfc2JjdXBpNG5WMU9SNUg4dTBZelEtZTZCdFg2dl91SQ==";
constexpr char kClientSecretB64[] = "VE9pMU9vUjNtTXZ3eFR3YVN0QVRyUjVoUlhVWDI1Ylp5X240eEt1M0ZkYlNwRG5JUnd0LXFFbHdGZkZkRWY2VzJ1S011UnQzM3c2Y3hqY0tVZ3NGN2c=";
}  // namespace

GeniusLyricsProvider::GeniusLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonLyricsProvider(u"Genius"_s, true, true, network, parent),
      oauth_(new OAuthenticator(network, this)) {

  oauth_->set_settings_group(QLatin1String(kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Authorization_Code);
  oauth_->set_authorize_url(QUrl(QLatin1String(kOAuthAuthorizeUrl)));
  oauth_->set_redirect_url(QUrl(QLatin1String(kOAuthRedirectUrl)));
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_client_id(QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)));
  oauth_->set_client_secret(QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64)));
  oauth_->set_scope(QLatin1String(kOAuthScope));
  oauth_->set_use_local_redirect_server(true);
  oauth_->set_random_port(false);

  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &GeniusLyricsProvider::OAuthFinished);

  oauth_->LoadSession();

}

bool GeniusLyricsProvider::authenticated() const {

  return oauth_->authenticated();

}

bool GeniusLyricsProvider::use_authorization_header() const {

  return true;

}

void GeniusLyricsProvider::Authenticate() {

  oauth_->Authenticate();

}

void GeniusLyricsProvider::Deauthenticate() {

  oauth_->ClearSession();

}

QByteArray GeniusLyricsProvider::authorization_header() const {

  return oauth_->authorization_header();

}

void GeniusLyricsProvider::OAuthFinished(const bool success, const QString &error) {

  if (success) {
    qLog(Debug) << "Genius: Authentication was successful.";
    Q_EMIT AuthenticationComplete(true);
    Q_EMIT AuthenticationSuccess();
  }
  else {
    qLog(Debug) << "Genius: Authentication failed.";
    Q_EMIT AuthenticationFailure(error);
    Q_EMIT AuthenticationComplete(false, error);
  }

}

void GeniusLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!authenticated()) {
    EndSearch(id, request);
    return;
  }

  GeniusLyricsSearchContextPtr search = make_shared<GeniusLyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.insert(id, search);

  QUrlQuery url_query;
  url_query.addQueryItem(u"q"_s, QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("%1 %2").arg(request.artist, request.title))));

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kUrlSearch)), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

}

void GeniusLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!requests_search_.contains(id)) return;
  GeniusLyricsSearchContextPtr search = requests_search_.value(id);

  const QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

  const QJsonObject json_object = GetJsonObject(reply).json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("meta"_L1)) {
    Error(u"Json reply is missing meta object."_s, json_object);
    return;
  }
  if (!json_object["meta"_L1].isObject()) {
    Error(u"Json reply meta is not an object."_s, json_object);
    return;
  }
  QJsonObject obj_meta = json_object["meta"_L1].toObject();
  if (!obj_meta.contains("status"_L1)) {
    Error(u"Json reply meta object is missing status."_s, obj_meta);
    return;
  }
  int status = obj_meta["status"_L1].toInt();
  if (status != 200) {
    if (obj_meta.contains("message"_L1)) {
      Error(QStringLiteral("Received error %1: %2.").arg(status).arg(obj_meta["message"_L1].toString()));
    }
    else {
      Error(QStringLiteral("Received error %1.").arg(status));
    }
    return;
  }

  if (!json_object.contains("response"_L1)) {
    Error(u"Json reply is missing response."_s, json_object);
    return;
  }
  if (!json_object["response"_L1].isObject()) {
    Error(u"Json response is not an object."_s, json_object);
    return;
  }
  const QJsonObject obj_response = json_object["response"_L1].toObject();
  if (!obj_response.contains("hits"_L1)) {
    Error(u"Json response is missing hits."_s, obj_response);
    return;
  }
  if (!obj_response["hits"_L1].isArray()) {
    Error(u"Json hits is not an array."_s, obj_response);
    return;
  }
  const QJsonArray array_hits = obj_response["hits"_L1].toArray();

  for (const QJsonValue &value_hit : array_hits) {
    if (!value_hit.isObject()) {
      continue;
    }
    QJsonObject obj_hit = value_hit.toObject();
    if (!obj_hit.contains("result"_L1)) {
      continue;
    }
    if (!obj_hit["result"_L1].isObject()) {
      continue;
    }
    QJsonObject obj_result = obj_hit["result"_L1].toObject();
    if (!obj_result.contains("title"_L1) || !obj_result.contains("primary_artist"_L1) || !obj_result.contains("url"_L1) || !obj_result["primary_artist"_L1].isObject()) {
      Error(u"Missing one or more values in result object"_s, obj_result);
      continue;
    }
    QJsonObject primary_artist = obj_result["primary_artist"_L1].toObject();
    if (!primary_artist.contains("name"_L1)) continue;

    QString artist = primary_artist["name"_L1].toString();
    QString title = obj_result["title"_L1].toString();

    // Ignore results where both the artist and title don't match.
    if (!artist.startsWith(search->request.albumartist, Qt::CaseInsensitive) &&
        !artist.startsWith(search->request.artist, Qt::CaseInsensitive) &&
        !title.startsWith(search->request.title, Qt::CaseInsensitive)) {
      continue;
    }

    QUrl url(obj_result["url"_L1].toString());
    if (!url.isValid()) continue;
    if (search->requests_lyric_.contains(url)) continue;

    GeniusLyricsLyricContext lyric;
    lyric.artist = artist;
    lyric.title = title;
    lyric.url = url;

    search->requests_lyric_.insert(url, lyric);

    QNetworkReply *new_reply = CreateGetRequest(url);
    QObject::connect(new_reply, &QNetworkReply::finished, this, [this, new_reply, search, url]() { HandleLyricReply(new_reply, search->id, url); });

  }

}

void GeniusLyricsProvider::HandleLyricReply(QNetworkReply *reply, const int search_id, const QUrl &url) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!requests_search_.contains(search_id)) return;
  GeniusLyricsSearchContextPtr search = requests_search_.value(search_id);

  if (!search->requests_lyric_.contains(url)) {
    EndSearch(search);
    return;
  }
  const GeniusLyricsLyricContext lyric = search->requests_lyric_.value(url);

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    EndSearch(search, lyric);
    return;
  }
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search, lyric);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(u"Empty reply received from server."_s);
    EndSearch(search, lyric);
    return;
  }

  const QString content = QString::fromUtf8(data);
  QString lyrics = HtmlLyricsProvider::ParseLyricsFromHTML(content, QRegularExpression(u"<div[^>]*>"_s), QRegularExpression(u"<\\/div>"_s), QRegularExpression(u"<div data-lyrics-container=[^>]+>"_s), true);
  if (lyrics.isEmpty()) {
    lyrics = HtmlLyricsProvider::ParseLyricsFromHTML(content, QRegularExpression(u"<div[^>]*>"_s), QRegularExpression(u"<\\/div>"_s), QRegularExpression(u"<div class=\"lyrics\">"_s), true);
  }

  if (!lyrics.isEmpty()) {
    LyricsSearchResult result(lyrics);
    result.artist = lyric.artist;
    result.title = lyric.title;
    search->results.append(result);
  }

  EndSearch(search, lyric);

}

void GeniusLyricsProvider::EndSearch(GeniusLyricsSearchContextPtr search, const GeniusLyricsLyricContext &lyric) {

  if (search->requests_lyric_.contains(lyric.url)) {
    search->requests_lyric_.remove(lyric.url);
  }
  if (search->requests_lyric_.count() == 0) {
    requests_search_.remove(search->id);
    EndSearch(search->id, search->request, search->results);
  }

}

void GeniusLyricsProvider::EndSearch(const int id, const LyricsSearchRequest &request, const LyricsSearchResults &results) {

  if (results.isEmpty()) {
    qLog(Debug) << "GeniusLyrics: No lyrics for" << request.artist << request.title;
  }
  else {
    qLog(Debug) << "GeniusLyrics: Got lyrics for" << request.artist << request.title;
  }

  Q_EMIT SearchFinished(id, results);

}

