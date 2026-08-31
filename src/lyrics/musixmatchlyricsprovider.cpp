/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include "apicredentials.h"

#include <memory>

#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDateTime>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

#include "includes/shared_ptr.h"
#include "utilities/cryptutils.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "constants/musixmatchsettings.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "musixmatchlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {
constexpr char kSettingsGroup[] = "Musixmatch";
constexpr char kApiUrl[] = "https://apic.musixmatch.com/ws/1.1/";
constexpr char kUserAgent[] = "Dalvik/2.1.0 (Linux; U; Android 13; Pixel 6 Build/T3B2.230316.003)";
constexpr char kCookie[] = "AWSELBCORS=0; AWSELB=0";
constexpr char kBuildNumber[] = "2022090901";
constexpr char kModel[] = "manufacturer/Google brand/Google model/Pixel 6";
constexpr char kLang[] = "en_US";

QString CompiledAppId() {
#ifdef MUSIXMATCH_APP_ID
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(MUSIXMATCH_APP_ID));
#else
  return QString();
#endif
}

QString CompiledAppSecret() {
#ifdef MUSIXMATCH_APP_SECRET
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(MUSIXMATCH_APP_SECRET));
#else
  return QString();
#endif
}

QString RandomGuid() {
  const quint64 value = QRandomGenerator::global()->generate64();
  return QStringLiteral("%1").arg(value, 16, 16, QLatin1Char('0'));
}

}  // namespace

MusixmatchLyricsProvider::MusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonLyricsProvider(u"Musixmatch"_s, false, false, network, parent),
      requesting_user_token_(false) {
  MusixmatchLyricsProvider::ReloadSettings();
}

bool MusixmatchLyricsProvider::has_compiled_api_credentials() const {
#if defined(MUSIXMATCH_APP_ID) && defined(MUSIXMATCH_APP_SECRET)
  return true;
#else
  return false;
#endif
}

QString MusixmatchLyricsProvider::api_credentials_settings_group() const {
  return QLatin1String(kSettingsGroup);
}

QString MusixmatchLyricsProvider::api_credentials_use_custom_key() const {
  return QLatin1String(MusixmatchSettings::kUseCustomApiCredentials);
}

QString MusixmatchLyricsProvider::api_credentials_id_key() const {
  return QLatin1String(MusixmatchSettings::kClientId);
}

QString MusixmatchLyricsProvider::api_credentials_secret_key() const {
  return QLatin1String(MusixmatchSettings::kClientSecret);
}

void MusixmatchLyricsProvider::ReloadSettings() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  const bool use_custom_api_credentials = !has_compiled_api_credentials() || s.value(MusixmatchSettings::kUseCustomApiCredentials, false).toBool();
  const QString client_id = use_custom_api_credentials ? s.value(MusixmatchSettings::kClientId).toString() : CompiledAppId();
  const QString client_secret = use_custom_api_credentials ? s.value(MusixmatchSettings::kClientSecret).toString() : CompiledAppSecret();
  s.endGroup();

  if (client_id != client_id_ || client_secret != client_secret_) {
    user_token_.clear();
  }
  client_id_ = client_id;
  client_secret_ = client_secret;

}

QUrl MusixmatchLyricsProvider::SignedUrl(const QString &endpoint, const ParamList &params) const {

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(kApiUrl) + endpoint);
  url.setQuery(url_query);

  const QByteArray date = QDateTime::currentDateTimeUtc().toString(u"yyyyMMdd"_s).toUtf8();
  const QByteArray signature = Utilities::HmacSha1(client_secret_.toUtf8(), url.toEncoded() + date);
  const QString signature_b64 = QString::fromLatin1(signature.toBase64()) + u'\n';

  url_query.addQueryItem(u"signature"_s, QString::fromLatin1(QUrl::toPercentEncoding(signature_b64)));
  url_query.addQueryItem(u"signature_protocol"_s, u"sha1"_s);
  url.setQuery(url_query);

  return url;

}

QNetworkReply *MusixmatchLyricsProvider::CreateAndroidGetRequest(const QUrl &url) {

  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(kUserAgent));
  network_request.setRawHeader("Cookie", QByteArray(kCookie));
  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &MusixmatchLyricsProvider::HandleSSLErrors);
  replies_ << reply;

  return reply;

}

void MusixmatchLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  LyricsSearchContextPtr search = make_shared<LyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.append(search);

  if (client_id_.isEmpty() || client_secret_.isEmpty()) {
    Error(tr("Missing Musixmatch client ID and/or client secret"));
    EndSearch(search);
    return;
  }

  if (user_token_.isEmpty()) {
    pending_token_requests_.append(search);
    RequestUserToken();
    return;
  }

  SendLyricsRequest(search);

}

void MusixmatchLyricsProvider::RequestUserToken() {

  if (requesting_user_token_) return;
  requesting_user_token_ = true;

  const ParamList params = ParamList()
    << Param(u"adv_id"_s, QUuid::createUuid().toString(QUuid::WithoutBraces))
    << Param(u"root"_s, u"0"_s)
    << Param(u"sideloaded"_s, u"0"_s)
    << Param(u"app_id"_s, client_id_)
    << Param(u"build_number"_s, QLatin1String(kBuildNumber))
    << Param(u"guid"_s, RandomGuid())
    << Param(u"lang"_s, QLatin1String(kLang))
    << Param(u"model"_s, QLatin1String(kModel))
    << Param(u"timestamp"_s, QDateTime::currentDateTimeUtc().toString(u"yyyy-MM-ddThh:mm:ss"_s) + u'Z')
    << Param(u"format"_s, u"json"_s);

  const QUrl url = SignedUrl(u"token.get"_s, params);
  QNetworkReply *reply = CreateAndroidGetRequest(url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleTokenReply(reply); });

  qLog(Debug) << "MusixmatchLyrics: Requesting user token.";

}

MusixmatchLyricsProvider::JsonObjectResult MusixmatchLyricsProvider::ParseJsonObject(QNetworkReply *reply) {

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
        for (const QJsonValueConstRef &value : array_errors) {
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

  return result;

}

void MusixmatchLyricsProvider::HandleTokenReply(QNetworkReply *reply) {

  requesting_user_token_ = false;

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QList<LyricsSearchContextPtr> pending_searches = pending_token_requests_;
  pending_token_requests_.clear();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    for (const LyricsSearchContextPtr &pending_search : pending_searches) {
      EndSearch(pending_search);
    }
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (!json_object.contains("message"_L1) || !json_object["message"_L1].isObject()) {
    Error(u"Json token reply is missing message object."_s, json_object);
    for (const LyricsSearchContextPtr &pending_search : pending_searches) {
      EndSearch(pending_search);
    }
    return;
  }
  const QJsonObject object_message = json_object["message"_L1].toObject();

  if (!object_message.contains("body"_L1) || !object_message["body"_L1].isObject()) {
    Error(u"Json token reply message is missing body."_s, object_message);
    for (const LyricsSearchContextPtr &pending_search : pending_searches) {
      EndSearch(pending_search);
    }
    return;
  }
  const QJsonObject object_body = object_message["body"_L1].toObject();

  if (!object_body.contains("user_token"_L1) || !object_body["user_token"_L1].isString()) {
    Error(u"Json token reply body is missing user_token."_s, object_body);
    for (const LyricsSearchContextPtr &pending_search : pending_searches) {
      EndSearch(pending_search);
    }
    return;
  }

  user_token_ = object_body["user_token"_L1].toString();
  qLog(Debug) << "MusixmatchLyrics: Got user token.";

  for (const LyricsSearchContextPtr &pending_search : pending_searches) {
    SendLyricsRequest(pending_search);
  }

}

void MusixmatchLyricsProvider::SendLyricsRequest(LyricsSearchContextPtr search) {

  const ParamList params = ParamList()
    << Param(u"app_id"_s, client_id_)
    << Param(u"format"_s, u"json"_s)
    << Param(u"usertoken"_s, user_token_)
    << Param(u"q_artist"_s, search->request.artist)
    << Param(u"q_track"_s, search->request.title);

  const QUrl url = SignedUrl(u"matcher.lyrics.get"_s, params);
  QNetworkReply *reply = CreateAndroidGetRequest(url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleLyricsReply(reply, search); });

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << search->request.artist << search->request.title;

}

bool MusixmatchLyricsProvider::RetryLyricsRequestWithFreshToken(LyricsSearchContextPtr search) {

  if (search->token_retried) return false;

  search->token_retried = true;
  user_token_.clear();
  pending_token_requests_.append(search);
  RequestUserToken();

  return true;

}

void MusixmatchLyricsProvider::HandleLyricsReply(QNetworkReply *reply, LyricsSearchContextPtr search) {

  QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  // The user token is bound to a single session and can expire or be rejected independently of app_id/secret - Musixmatch can signal this with an HTTP-level 401 (checked here, before the generic unsuccessful-result handling below) or with status_code 401 in the JSON response body (checked further down, once the body has parsed successfully). Retry exactly once with a freshly obtained token.
  if (json_object_result.http_status_code == 401) {
    if (RetryLyricsRequestWithFreshToken(search)) {
      end_search.dismiss();
      return;
    }
    Error(u"Musixmatch user token was rejected again after renewal."_s);
    return;
  }

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) return;

  if (!json_object.contains("message"_L1) || !json_object["message"_L1].isObject()) {
    Error(u"Json reply is missing message object."_s, json_object);
    return;
  }
  const QJsonObject object_message = json_object["message"_L1].toObject();

  if (!object_message.contains("header"_L1) || !object_message["header"_L1].isObject()) {
    Error(u"Json reply message object is missing header."_s, object_message);
    return;
  }
  const QJsonObject object_header = object_message["header"_L1].toObject();
  const int status_code = object_header["status_code"_L1].toInt();

  // The user token is bound to a single session and can expire or be rejected independently of app_id/secret - Musixmatch signals this with status_code 401 rather than an HTTP-level error. Retry exactly once with a freshly obtained token.
  if (status_code == 401) {
    if (RetryLyricsRequestWithFreshToken(search)) {
      end_search.dismiss();
      return;
    }
    Error(u"Musixmatch user token was rejected again after renewal."_s);
    return;
  }

  // No match found for this artist/title - not an error.
  if (status_code == 404) return;

  if (status_code != 200) {
    Error(QStringLiteral("Received status code %1.").arg(status_code));
    return;
  }

  if (!object_message.contains("body"_L1) || !object_message["body"_L1].isObject()) {
    // A track with no lyrics (or an unmatched query) returns an empty array body instead of an object - not an error.
    return;
  }
  const QJsonObject object_body = object_message["body"_L1].toObject();

  if (!object_body.contains("lyrics"_L1) || !object_body["lyrics"_L1].isObject()) {
    Error(u"Json body is missing lyrics."_s, object_body);
    return;
  }
  const QJsonObject object_lyrics = object_body["lyrics"_L1].toObject();

  if (!object_lyrics.contains("lyrics_body"_L1) || !object_lyrics["lyrics_body"_L1].isString()) {
    Error(u"Json lyrics is missing lyrics_body."_s, object_lyrics);
    return;
  }
  const QString lyrics_body = object_lyrics["lyrics_body"_L1].toString();
  if (lyrics_body.isEmpty()) return;

  LyricsSearchResult result;
  result.artist = search->request.artist;
  result.album = search->request.album;
  result.title = search->request.title;
  result.lyrics = lyrics_body;
  search->results.append(result);

}

void MusixmatchLyricsProvider::EndSearch(LyricsSearchContextPtr search) {

  requests_search_.removeAll(search);
  pending_token_requests_.removeAll(search);

  if (search->results.isEmpty()) {
    qLog(Debug) << "MusixmatchLyrics: No lyrics for" << search->request.artist << search->request.title;
  }
  else {
    qLog(Debug) << "MusixmatchLyrics: Got lyrics for" << search->request.artist << search->request.title;
  }
  Q_EMIT SearchFinished(search->id, search->results);

}
