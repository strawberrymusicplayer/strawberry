/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QPair>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QTextCodec>
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
#include <QtDebug>

#include "core/logging.h"
#include "core/utilities.h"
#include "core/network.h"
#include "internet/localredirectserver.h"
#include "jsonlyricsprovider.h"
#include "lyricsfetcher.h"
#include "lyricsprovider.h"
#include "geniuslyricsprovider.h"

const char *GeniusLyricsProvider::kSettingsGroup = "GeniusLyrics";
const char *GeniusLyricsProvider::kOAuthAuthorizeUrl = "https://api.genius.com/oauth/authorize";
const char *GeniusLyricsProvider::kOAuthAccessTokenUrl = "https://api.genius.com/oauth/token";
const char *GeniusLyricsProvider::kOAuthRedirectUrl = "http://localhost:63111/"; // Genius does not accept a random port number. This port must match the the URL of the ClientID.
const char *GeniusLyricsProvider::kUrlSearch = "https://api.genius.com/search/";
const char *GeniusLyricsProvider::kClientIDB64 = "RUNTNXU4U1VyMU1KUU5hdTZySEZteUxXY2hkanFiY3lfc2JjdXBpNG5WMU9SNUg4dTBZelEtZTZCdFg2dl91SQ==";
const char *GeniusLyricsProvider::kClientSecretB64 = "VE9pMU9vUjNtTXZ3eFR3YVN0QVRyUjVoUlhVWDI1Ylp5X240eEt1M0ZkYlNwRG5JUnd0LXFFbHdGZkZkRWY2VzJ1S011UnQzM3c2Y3hqY0tVZ3NGN2c=";

GeniusLyricsProvider::GeniusLyricsProvider(QObject *parent) : JsonLyricsProvider("Genius", true, true, parent), network_(new NetworkAccessManager(this)), server_(nullptr) {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  if (s.contains("access_token")) {
    access_token_ = s.value("access_token").toString();
  }
  s.endGroup();

}

GeniusLyricsProvider::~GeniusLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void GeniusLyricsProvider::Authenticate() {

  QUrl redirect_url(kOAuthRedirectUrl);

  if (!server_) {
    server_ = new LocalRedirectServer(this);
    server_->set_https(false);
    server_->set_port(redirect_url.port());
    if (!server_->Listen()) {
      AuthError(server_->error());
      server_->deleteLater();
      server_ = nullptr;
      return;
    }
    connect(server_, SIGNAL(Finished()), this, SLOT(RedirectArrived()));
  }

  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));
  if (code_challenge_.lastIndexOf(QChar('=')) == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  const ParamList params = ParamList() << Param("client_id", QByteArray::fromBase64(kClientIDB64))
                                       << Param("redirect_uri", redirect_url.toString())
                                       << Param("scope", "me")
                                       << Param("state", code_challenge_)
                                       << Param("response_type", "code");

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kOAuthAuthorizeUrl);
  url.setQuery(url_query);

  const bool result = QDesktopServices::openUrl(url);
  if (!result) {
    QMessageBox messagebox(QMessageBox::Information, tr("Genius Authentication"), tr("Please open this URL in your browser") + QString(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
    messagebox.setTextFormat(Qt::RichText);
    messagebox.exec();
  }

}

void GeniusLyricsProvider::RedirectArrived() {

  if (!server_) return;

  if (server_->error().isEmpty()) {
    QUrl url = server_->request_url();
    if (url.isValid()) {
      QUrlQuery url_query(url);
      if (url_query.hasQueryItem("error")) {
        AuthError(QUrlQuery(url).queryItemValue("error"));
      }
      else if (url_query.hasQueryItem("code")) {
        QUrl redirect_url(kOAuthRedirectUrl);
        redirect_url.setPort(server_->url().port());
        RequestAccessToken(url, redirect_url);
      }
      else {
        AuthError(tr("Redirect missing token code!"));
      }
    }
    else {
      AuthError(tr("Received invalid reply from web browser."));
    }
  }
  else {
    AuthError(server_->error());
  }

  server_->close();
  server_->deleteLater();
  server_ = nullptr;

}

void GeniusLyricsProvider::RequestAccessToken(const QUrl &url, const QUrl &redirect_url) {

  qLog(Debug) << "GeniusLyrics: Authorization URL Received" << url;

  QUrlQuery url_query(url);

  if (url.hasQuery() && url_query.hasQueryItem("code") && url_query.hasQueryItem("state")) {

    QString code = url_query.queryItemValue("code");
    QString state = url_query.queryItemValue("state");

    const ParamList params = ParamList() << Param("code", code)
                                         << Param("client_id", QByteArray::fromBase64(kClientIDB64))
                                         << Param("client_secret", QByteArray::fromBase64(kClientSecretB64))
                                         << Param("redirect_uri", redirect_url.toString())
                                         << Param("grant_type", "authorization_code")
                                         << Param("response_type", "code");

    QUrlQuery new_url_query;
    for (const Param &param : params) {
      new_url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    }

    QUrl new_url(kOAuthAccessTokenUrl);
    QNetworkRequest req(new_url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    QByteArray query = new_url_query.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkReply *reply = network_->post(req, query);
    replies_ << reply;
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleLoginSSLErrors(QList<QSslError>)));
    connect(reply, &QNetworkReply::finished, [=] { AccessTokenRequestFinished(reply); });

  }

  else {
    AuthError(tr("Redirect from Genius is missing query items code or state."));
    return;
  }

}

void GeniusLyricsProvider::HandleLoginSSLErrors(QList<QSslError> ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void GeniusLyricsProvider::AccessTokenRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      AuthError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status" and "userMessage" then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error") && json_obj.contains("error_description")) {
          QString error = json_obj["error"].toString();
          QString error_description = json_obj["error_description"].toString();
          login_errors_ << QString("Authentication failure: %1 (%2)").arg(error).arg(error_description);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      AuthError();
      return;
    }
  }

  QByteArray data(reply->readAll());

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(QString("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_doc.isEmpty()) {
    AuthError("Authentication reply from server has empty Json document.");
    return;
  }

  if (!json_doc.isObject()) {
    AuthError("Authentication reply from server has Json document that is not an object.", json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    AuthError("Authentication reply from server has empty Json object.", json_doc);
    return;
  }

  if (!json_obj.contains("access_token")) {
    AuthError("Authentication reply from server is missing access token.", json_obj);
    return;
  }

  access_token_ = json_obj["access_token"].toString();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.endGroup();

  qLog(Debug) << "Genius: Authentication was successful, got access token" << access_token_;

  emit AuthenticationComplete(true);
  emit AuthenticationSuccess();

}

bool GeniusLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  Q_UNUSED(album);

  if (access_token_.isEmpty()) return false;

  std::shared_ptr<GeniusLyricsSearchContext> search = std::make_shared<GeniusLyricsSearchContext>();

  search->id = id;
  search->artist = artist;
  search->title = title;
  requests_search_.insert(id, search);

  const ParamList params = ParamList() << Param("q", QString(artist + " " + title));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kUrlSearch);
  url.setQuery(url_query);
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setRawHeader("Authorization", "Bearer " + access_token_.toUtf8());
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id); });

  //qLog(Debug) << "GeniusLyrics: Sending request for" << url;

  return true;

}

void GeniusLyricsProvider::CancelSearch(const quint64 id) { Q_UNUSED(id); }

void GeniusLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!requests_search_.contains(id)) return;
  std::shared_ptr<GeniusLyricsSearchContext> search = requests_search_.value(id);

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) {
    EndSearch(search);
    return;
  }

  if (!json_obj.contains("meta")) {
    Error("Json reply is missing meta object.", json_obj);
    EndSearch(search);
    return;
  }
  if (!json_obj["meta"].isObject()) {
    Error("Json reply meta is not an object.", json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_meta = json_obj["meta"].toObject();
  if (!obj_meta.contains("status")) {
    Error("Json reply meta object is missing status.", obj_meta);
    EndSearch(search);
    return;
  }
  int status = obj_meta["status"].toInt();
  if (status != 200) {
    if (obj_meta.contains("message")) {
      Error(QString("Received error %1: %2.").arg(status).arg(obj_meta["message"].toString()));
    }
    else {
      Error(QString("Received error %1.").arg(status));
    }
    EndSearch(search);
    return;
  }

  if (!json_obj.contains("response")) {
    Error("Json reply is missing response.", json_obj);
    EndSearch(search);
    return;
  }
  if (!json_obj["response"].isObject()) {
    Error("Json response is not an object.", json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_response = json_obj["response"].toObject();
  if (!obj_response.contains("hits")) {
    Error("Json response is missing hits.", obj_response);
    EndSearch(search);
    return;
  }
  if (!obj_response["hits"].isArray()) {
    Error("Json hits is not an array.", obj_response);
    EndSearch(search);
    return;
  }
  QJsonArray array_hits = obj_response["hits"].toArray();

  for (QJsonValue value_hit : array_hits) {
    if (!value_hit.isObject()) {
      continue;
    }
    QJsonObject obj_hit = value_hit.toObject();
    if (!obj_hit.contains("result")) {
      continue;
    }
    if (!obj_hit["result"].isObject()) {
      continue;
    }
    QJsonObject obj_result = obj_hit["result"].toObject();
    if (!obj_result.contains("title") || !obj_result.contains("primary_artist") || !obj_result.contains("url") || !obj_result["primary_artist"].isObject()) {
      Error("Missing one or more values in result object", obj_result);
      continue;
    }
    QJsonObject primary_artist = obj_result["primary_artist"].toObject();
    if (!primary_artist.contains("name")) continue;

    QString artist = primary_artist["name"].toString();
    QString title = obj_result["title"].toString();

    // Ignore results where both the artist and title don't match.
    if (artist.toLower() != search->artist.toLower() && title.toLower() != search->title.toLower()) continue;

    QUrl url(obj_result["url"].toString());
    if (!url.isValid()) continue;
    if (search->requests_lyric_.contains(url)) continue;

    GeniusLyricsLyricContext lyric;
    lyric.artist = artist;
    lyric.title = title;
    lyric.url = url;

    search->requests_lyric_.insert(url, lyric);

    QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    QNetworkReply *new_reply = network_->get(req);
    replies_ << new_reply;
    connect(new_reply, &QNetworkReply::finished, [=] { HandleLyricReply(new_reply, search->id, url); });

  }

  EndSearch(search);

}

void GeniusLyricsProvider::HandleLyricReply(QNetworkReply *reply, const int search_id, const QUrl &url) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!requests_search_.contains(search_id)) return;
  std::shared_ptr<GeniusLyricsSearchContext> search = requests_search_.value(search_id);

  if (!search->requests_lyric_.contains(url)) {
    EndSearch(search);
    return;
  }
  const GeniusLyricsLyricContext lyric = search->requests_lyric_.value(url);

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    EndSearch(search, lyric);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search, lyric);
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error("Empty reply received from server.");
    EndSearch(search, lyric);
    return;
  }

  QTextCodec *codec = QTextCodec::codecForName("utf-8");
  if (!codec) {
    EndSearch(search, lyric);
    return;
  }
  QString content = codec->toUnicode(data);

  // Extract the lyrics from HTML.

  QString tag_begin = "<div class=\"lyrics\">";
  QString tag_end = "</div>";
  int begin_idx = content.indexOf(tag_begin);
  QString lyrics;
  if (begin_idx > 0) {
    begin_idx += tag_begin.length();
    int end_idx = content.indexOf(tag_end, begin_idx);
    lyrics = content.mid(begin_idx, end_idx - begin_idx);
    lyrics = lyrics.remove(QRegularExpression("<[^>]*>"));
    lyrics = lyrics.trimmed();
  }

  if (!lyrics.isEmpty()) {
    LyricsSearchResult result;
    result.artist = lyric.artist;
    result.title = lyric.title;
    result.lyrics = lyrics;
    search->results.append(result);
  }

  EndSearch(search, lyric);

}

void GeniusLyricsProvider::AuthError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  for (const QString &e : login_errors_) Error(e);
  if (debug.isValid()) qLog(Debug) << debug;

  emit AuthenticationFailure(login_errors_);
  emit AuthenticationComplete(false, login_errors_);

  login_errors_.clear();

}

void GeniusLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "GeniusLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

void GeniusLyricsProvider::EndSearch(std::shared_ptr<GeniusLyricsSearchContext> search, const GeniusLyricsLyricContext lyric) {

  if (search->requests_lyric_.contains(lyric.url)) {
    search->requests_lyric_.remove(lyric.url);
  }
  if (search->requests_lyric_.count() == 0) {
    requests_search_.remove(search->id);
    if (search->results.isEmpty()) {
      qLog(Debug) << "GeniusLyrics: No lyrics for" << search->artist << search->title;
    }
    else {
      qLog(Debug) << "GeniusLyrics: Got lyrics for" << search->artist << search->title;
    }
    emit SearchFinished(search->id, search->results);
  }

}
