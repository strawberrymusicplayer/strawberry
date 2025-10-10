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

#include <memory>

#include <QApplication>
#include <QThread>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QMessageBox>

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

void GeniusLyricsProvider::ClearSession() {

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

  qLog(Debug) << name_ << "Sending request for" << url_query.query();

}

GeniusLyricsProvider::JsonObjectResult GeniusLyricsProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("errors"_L1) && json_object["errors"_L1].isArray()) {
        const QJsonArray array_errors = json_object["errors"_L1].toArray();
        for (const auto &value : array_errors) {
          if (!value.isObject()) continue;
          const QJsonObject object_error = value.toObject();
          if (!object_error.contains("category"_L1) || !object_error.contains("code"_L1) || !object_error.contains("detail"_L1)) {
            continue;
          }
          const QString category = object_error["category"_L1].toString();
          const QString code = object_error["code"_L1].toString();
          const QString detail = object_error["detail"_L1].toString();
          result.error_code = ErrorCode::APIError;
          result.error_message = QStringLiteral("%1 (%2) (%3)").arg(category, code, detail);
        }
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
    oauth_->ClearSession();
  }

  return result;

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

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
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
  const QJsonObject object_meta = json_object["meta"_L1].toObject();
  if (!object_meta.contains("status"_L1)) {
    Error(u"Json reply meta object is missing status."_s, object_meta);
    return;
  }
  const int status = object_meta["status"_L1].toInt();
  if (status != 200) {
    if (object_meta.contains("message"_L1)) {
      Error(QStringLiteral("Received error %1: %2.").arg(status).arg(object_meta["message"_L1].toString()));
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
    const QJsonObject object_hit = value_hit.toObject();
    if (!object_hit.contains("result"_L1)) {
      continue;
    }
    if (!object_hit["result"_L1].isObject()) {
      continue;
    }
    const QJsonObject object_result = object_hit["result"_L1].toObject();
    if (!object_result.contains("title"_L1) || !object_result.contains("primary_artist"_L1) || !object_result.contains("url"_L1) || !object_result["primary_artist"_L1].isObject()) {
      Error(u"Missing one or more values in result object"_s, object_result);
      continue;
    }
    const QJsonObject primary_artist = object_result["primary_artist"_L1].toObject();
    if (!primary_artist.contains("name"_L1)) continue;

    const QString artist = primary_artist["name"_L1].toString();
    const QString title = object_result["title"_L1].toString();

    // Ignore results where the artist or title don't begin or end the same
    if (!StartsOrEndsMatch(artist, search->request.artist) || !StartsOrEndsMatch(title, search->request.title)) {
      continue;
    }

    const QUrl url(object_result["url"_L1].toString());
    if (!url.isValid()) continue;
    if (search->requests_lyric_.contains(url)) continue;

    GeniusLyricsLyricContext lyric;
    lyric.artist = artist;
    lyric.title = title;
    lyric.url = url;

    search->requests_lyric_.insert(url, lyric);

    QNetworkReply *new_reply = CreateGetRequest(url);
    QObject::connect(new_reply, &QNetworkReply::finished, this, [this, new_reply, search, url]() { HandleLyricReply(new_reply, search->id, url); });

    qLog(Debug) << name_ << "Sending request for" << url;

    // If full match, don't bother iterating further
    if (artist == search->request.albumartist && artist == search->request.artist && title == search->request.title) {
      break;
    }
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

  static const QRegularExpression start_tag(u"<div[^>]*>"_s);
  static const QRegularExpression end_tag(u"<\\/div>"_s);
  static const QRegularExpression lyrics_start(u"<div data-lyrics-container=[^>]+>"_s);

  static const QRegularExpression regex_html_tag_span_trans(u"<span class=\"LyricsHeader__Translations[^>]*>[^<]*</span>"_s);
  static const QRegularExpression regex_html_tag_div_ellipsis(u"<div class=\"LyricsHeader__TextEllipsis[^>]*>[^<]*</div>"_s);
  static const QRegularExpression regex_html_tag_span_contribs(u"<span class=\"ContributorsCreditSong__Contributors[^>]*>[^<]*</span>"_s);
  static const QRegularExpression regex_html_tag_div_bio(u"<div class=\"SongBioPreview__Container[^>]*>.*?</div>"_s);
  static const QRegularExpression regex_html_tag_h2(u"<h2 [^>]*>[^<]*</h2>"_s);
  static const QList<QRegularExpression> regex_removes{ regex_html_tag_span_trans, regex_html_tag_div_ellipsis, regex_html_tag_span_contribs, regex_html_tag_div_bio, regex_html_tag_h2 };

  const QString lyrics = HtmlLyricsProvider::ParseLyricsFromHTML(QString::fromUtf8(data), start_tag, end_tag, lyrics_start, true, regex_removes);
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

bool GeniusLyricsProvider::StartsOrEndsMatch(QString s, QString t) {

  constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;

  static const QRegularExpression puncts_regex(u"[!,.:;]"_s);
  static const QRegularExpression quotes_regex(u"[’‘´`]"_s);

  s.remove(puncts_regex).replace(quotes_regex, u"'"_s);
  t.remove(puncts_regex).replace(quotes_regex, u"'"_s);

  return (s.compare(t, cs) == 0 && !s.isEmpty()) || (!s.isEmpty() && !t.isEmpty() && (s.startsWith(t, cs) || t.startsWith(s, cs) || s.endsWith(t, cs) || t.endsWith(s, cs)));

}
