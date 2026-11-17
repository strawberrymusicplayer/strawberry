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
#include <QScopeGuard>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "utilities/cryptutils.h"
#include "utilities/strutils.h"
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
constexpr char kApiUrl[] = "https://api.musixmatch.com/ws/1.1/";

QString CompiledApiKey() {
#ifdef MUSIXMATCH_API_KEY
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(MUSIXMATCH_API_KEY));
#else
  return QString();
#endif
}

}  // namespace

MusixmatchLyricsProvider::MusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonLyricsProvider(u"Musixmatch"_s, true, false, false, network, parent) {
  MusixmatchLyricsProvider::ReloadSettings();
}

bool MusixmatchLyricsProvider::has_compiled_api_credentials() const {
#ifdef MUSIXMATCH_API_KEY
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
  return QLatin1String(MusixmatchSettings::kApiKey);
}

void MusixmatchLyricsProvider::ReloadSettings() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  const bool use_custom_api_credentials = !has_compiled_api_credentials() || s.value(MusixmatchSettings::kUseCustomApiCredentials, false).toBool();
  api_key_ = use_custom_api_credentials ? s.value(MusixmatchSettings::kApiKey).toString() : CompiledApiKey();
  s.endGroup();

}

QNetworkReply *MusixmatchLyricsProvider::CreateApiRequest(const QString &endpoint, const ParamList &params) {

  return CreateGetRequest(QUrl(QLatin1String(kApiUrl) + endpoint), ParamList(params) << Param(u"apikey"_s, api_key_) << Param(u"format"_s, u"json"_s));

}

void MusixmatchLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  LyricsSearchContextPtr search = make_shared<LyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.append(search);

  if (api_key_.isEmpty()) {
    Error(tr("Missing Musixmatch API key"));
    EndSearch(search);
    return;
  }

  SendTrackRequest(search);

}

MusixmatchLyricsProvider::JsonObjectResult MusixmatchLyricsProvider::ParseJsonObject(QNetworkReply *reply) {

  // The error string of the reply contains the URL, the query of which has the API key.

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(Utilities::RedactUrls(reply->errorString(), reply->url()), QString::number(reply->error())));
  }

  // The data is also read for an HTTP error, which is not parsed (QTBUG-135641).
  const QByteArray data = reply->readAll();

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  if (reply->error() != QNetworkReply::NoError) {
    result.error_code = ErrorCode::NetworkError;
    result.error_message = QStringLiteral("%1 (%2)").arg(Utilities::RedactUrls(reply->errorString(), reply->url()), QString::number(reply->error()));
    return result;
  }

  if (result.http_status_code != 200) {
    result.error_code = ErrorCode::HttpError;
    result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    return result;
  }

  QJsonParseError json_parse_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
  if (json_parse_error.error != QJsonParseError::NoError) {
    result.error_code = ErrorCode::ParseError;
    result.error_message = json_parse_error.errorString();
    return result;
  }
  result.json_object = json_document.object();

  return result;

}

void MusixmatchLyricsProvider::SendTrackRequest(LyricsSearchContextPtr search) {

  const ParamList params = ParamList() << Param(u"q_artist"_s, search->request.artist)
                                       << Param(u"q_track"_s, search->request.title);

  // The track is matched first, the lyrics are for the matched track, which can be a different track than requested, so the lyrics are scored with the matched track.
  QNetworkReply *reply = CreateApiRequest(u"matcher.track.get"_s, params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleTrackReply(reply, search); });

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << search->request.artist << search->request.title;

}

void MusixmatchLyricsProvider::SendLyricsRequest(LyricsSearchContextPtr search) {

  const ParamList params = ParamList() << Param(u"commontrack_id"_s, QString::number(search->commontrack_id));

  QNetworkReply *reply = CreateApiRequest(u"track.lyrics.get"_s, params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleLyricsReply(reply, search); });

  qLog(Debug) << "MusixmatchLyrics: Sending lyrics request for" << search->artist << search->title;

}

bool MusixmatchLyricsProvider::ParseReply(QNetworkReply *reply, QJsonObject &object_body) {

  if (!replies_.contains(reply)) return false;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return false;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (!json_object.contains("message"_L1) || !json_object["message"_L1].isObject()) {
    Error(u"Json reply is missing message object."_s, json_object);
    return false;
  }
  const QJsonObject object_message = json_object["message"_L1].toObject();

  if (!object_message.contains("header"_L1) || !object_message["header"_L1].isObject()) {
    Error(u"Json reply message object is missing header."_s, object_message);
    return false;
  }
  const QJsonObject object_header = object_message["header"_L1].toObject();
  const int status_code = object_header["status_code"_L1].toInt();

  switch (status_code) {
    case 200:
      break;
    case 401:
      Error(tr("Musixmatch API key was rejected."));
      return false;
    case 402:
      Error(tr("Musixmatch API usage limit was reached."));
      return false;
    case 404:
      // No match found for this artist and title, not an error.
      return false;
    default:
      Error(QStringLiteral("Received status code %1.").arg(status_code));
      return false;
  }

  if (!object_message.contains("body"_L1) || !object_message["body"_L1].isObject()) {
    // A track with no lyrics (or an unmatched query) returns an empty array body instead of an object, not an error.
    return false;
  }
  object_body = object_message["body"_L1].toObject();

  return true;

}

void MusixmatchLyricsProvider::HandleTrackReply(QNetworkReply *reply, LyricsSearchContextPtr search) {

  QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

  QJsonObject object_body;
  if (!ParseReply(reply, object_body)) return;

  if (!object_body.contains("track"_L1) || !object_body["track"_L1].isObject()) {
    Error(u"Json body is missing track."_s, object_body);
    return;
  }
  const QJsonObject object_track = object_body["track"_L1].toObject();

  // The matched track has no lyrics, is instrumental, or the lyrics may not be shown, not an error.
  if (object_track["has_lyrics"_L1].toInt() == 0 || object_track["instrumental"_L1].toInt() != 0 || object_track["restricted"_L1].toInt() != 0) return;

  search->commontrack_id = object_track["commontrack_id"_L1].toInteger();
  if (search->commontrack_id <= 0) {
    Error(u"Json track is missing commontrack_id."_s, object_track);
    return;
  }
  search->artist = object_track["artist_name"_L1].toString();
  search->album = object_track["album_name"_L1].toString();
  search->title = object_track["track_name"_L1].toString();

  end_search.dismiss();
  SendLyricsRequest(search);

}

void MusixmatchLyricsProvider::HandleLyricsReply(QNetworkReply *reply, LyricsSearchContextPtr search) {

  QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

  QJsonObject object_body;
  if (!ParseReply(reply, object_body)) return;

  if (!object_body.contains("lyrics"_L1) || !object_body["lyrics"_L1].isObject()) {
    Error(u"Json body is missing lyrics."_s, object_body);
    return;
  }
  const QJsonObject object_lyrics = object_body["lyrics"_L1].toObject();

  // Lyrics which may not be shown, also only in this country, have an empty body.
  if (object_lyrics["restricted"_L1].toInt() != 0) return;

  if (!object_lyrics.contains("lyrics_body"_L1) || !object_lyrics["lyrics_body"_L1].isString()) {
    Error(u"Json lyrics is missing lyrics_body."_s, object_lyrics);
    return;
  }
  QString lyrics = object_lyrics["lyrics_body"_L1].toString().trimmed();
  if (lyrics.isEmpty()) return;

  // The copyright of the lyrics must be shown with the lyrics.
  const QString lyrics_copyright = object_lyrics["lyrics_copyright"_L1].toString().trimmed();
  if (!lyrics_copyright.isEmpty()) {
    lyrics += "\n\n"_L1 + lyrics_copyright;
  }

  // The lyrics are scored with the track Musixmatch matched, not the requested track.
  LyricsSearchResult result;
  result.artist = search->artist;
  result.album = search->album;
  result.title = search->title;
  result.lyrics = lyrics;
  search->results.append(result);

}

void MusixmatchLyricsProvider::EndSearch(LyricsSearchContextPtr search) {

  requests_search_.removeAll(search);

  if (search->results.isEmpty()) {
    qLog(Debug) << "MusixmatchLyrics: No lyrics for" << search->request.artist << search->request.title;
  }
  else {
    qLog(Debug) << "MusixmatchLyrics: Got lyrics for" << search->request.artist << search->request.title;
  }
  Q_EMIT SearchFinished(search->id, search->results);

}
