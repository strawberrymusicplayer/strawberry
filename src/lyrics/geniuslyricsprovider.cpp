/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"
#include "core/localredirectserver.h"
#include "utilities/randutils.h"
#include "jsonlyricsprovider.h"
#include "htmllyricsprovider.h"
#include "geniuslyricsprovider.h"

using namespace Qt::StringLiterals;
using std::make_shared;

namespace {
constexpr char kSettingsGroup[] = "GeniusLyrics";
constexpr char kOAuthAuthorizeUrl[] = "https://api.genius.com/oauth/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://api.genius.com/oauth/token";
constexpr char kOAuthRedirectUrl[] = "http://localhost:63111/";  // Genius does not accept a random port number. This port must match the URL of the ClientID.
constexpr char kUrlSearch[] = "https://api.genius.com/search/";
constexpr char kClientIDB64[] = "RUNTNXU4U1VyMU1KUU5hdTZySEZteUxXY2hkanFiY3lfc2JjdXBpNG5WMU9SNUg4dTBZelEtZTZCdFg2dl91SQ==";
constexpr char kClientSecretB64[] = "VE9pMU9vUjNtTXZ3eFR3YVN0QVRyUjVoUlhVWDI1Ylp5X240eEt1M0ZkYlNwRG5JUnd0LXFFbHdGZkZkRWY2VzJ1S011UnQzM3c2Y3hqY0tVZ3NGN2c=";
}  // namespace

GeniusLyricsProvider::GeniusLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(QStringLiteral("Genius"), true, true, network, parent), server_(nullptr) {

  Settings s;
  s.beginGroup(kSettingsGroup);
  if (s.contains("access_token")) {
    set_access_token(s.value("access_token").toString());
  }
  s.endGroup();

}

GeniusLyricsProvider::~GeniusLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

QString GeniusLyricsProvider::access_token() const {

  QMutexLocker l(&mutex_access_token_);
  return access_token_;

}

void GeniusLyricsProvider::clear_access_token() {

  QMutexLocker l(&mutex_access_token_);
  access_token_.clear();

}

void GeniusLyricsProvider::set_access_token(const QString &access_token) {

  QMutexLocker l(&mutex_access_token_);
  access_token_ = access_token;

}

void GeniusLyricsProvider::Authenticate() {

  QUrl redirect_url(QString::fromLatin1(kOAuthRedirectUrl));

  if (!server_) {
    server_ = new LocalRedirectServer(this);
    server_->set_port(redirect_url.port());
    if (!server_->Listen()) {
      AuthError(server_->error());
      server_->deleteLater();
      server_ = nullptr;
      return;
    }
    QObject::connect(server_, &LocalRedirectServer::Finished, this, &GeniusLyricsProvider::RedirectArrived);
  }

  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString::fromLatin1(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));
  if (code_challenge_.lastIndexOf(u'=') == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  QUrlQuery url_query;
  url_query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(QUrl::toPercentEncoding(QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)))));
  url_query.addQueryItem(QStringLiteral("redirect_uri"), QString::fromLatin1(QUrl::toPercentEncoding(redirect_url.toString())));
  url_query.addQueryItem(QStringLiteral("scope"), QStringLiteral("me"));
  url_query.addQueryItem(QStringLiteral("state"), QString::fromLatin1(QUrl::toPercentEncoding(code_challenge_)));
  url_query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));

  QUrl url(QString::fromLatin1(kOAuthAuthorizeUrl));
  url.setQuery(url_query);

  const bool result = QDesktopServices::openUrl(url);
  if (!result) {
    QMessageBox messagebox(QMessageBox::Information, tr("Genius Authentication"), tr("Please open this URL in your browser") + QStringLiteral(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
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
      if (url_query.hasQueryItem(QStringLiteral("error"))) {
        AuthError(QUrlQuery(url).queryItemValue(QStringLiteral("error")));
      }
      else if (url_query.hasQueryItem(QStringLiteral("code"))) {
        QUrl redirect_url(QString::fromLatin1(kOAuthRedirectUrl));
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

  if (url.hasQuery() && url_query.hasQueryItem(QStringLiteral("code")) && url_query.hasQueryItem(QStringLiteral("state"))) {

    const QString code = url_query.queryItemValue(QStringLiteral("code"));

    QUrlQuery new_url_query;
    new_url_query.addQueryItem(QStringLiteral("code"), QString::fromLatin1(QUrl::toPercentEncoding(code)));
    new_url_query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(QUrl::toPercentEncoding(QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)))));
    new_url_query.addQueryItem(QStringLiteral("client_secret"), QString::fromLatin1(QUrl::toPercentEncoding(QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64)))));
    new_url_query.addQueryItem(QStringLiteral("redirect_uri"), QString::fromLatin1(QUrl::toPercentEncoding(redirect_url.toString())));
    new_url_query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    new_url_query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));

    QUrl new_url(QString::fromLatin1(kOAuthAccessTokenUrl));
    QNetworkRequest req(new_url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QByteArray query = new_url_query.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkReply *reply = network_->post(req, query);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::sslErrors, this, &GeniusLyricsProvider::HandleLoginSSLErrors);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AccessTokenRequestFinished(reply); });

  }

  else {
    AuthError(tr("Redirect from Genius is missing query items code or state."));
    return;
  }

}

void GeniusLyricsProvider::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void GeniusLyricsProvider::AccessTokenRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      AuthError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status" and "userMessage" then use that instead.
      const QByteArray data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1) && json_obj.contains("error_description"_L1)) {
          QString error = json_obj["error"_L1].toString();
          QString error_description = json_obj["error_description"_L1].toString();
          login_errors_ << QStringLiteral("Authentication failure: %1 (%2)").arg(error, error_description);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      AuthError();
      return;
    }
  }

  const QByteArray data = reply->readAll();

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(QStringLiteral("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_doc.isEmpty()) {
    AuthError(QStringLiteral("Authentication reply from server has empty Json document."));
    return;
  }

  if (!json_doc.isObject()) {
    AuthError(QStringLiteral("Authentication reply from server has Json document that is not an object."), json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    AuthError(QStringLiteral("Authentication reply from server has empty Json object."), json_doc);
    return;
  }

  if (!json_obj.contains("access_token"_L1)) {
    AuthError(QStringLiteral("Authentication reply from server is missing access token."), json_obj);
    return;
  }

  const QString access_token = json_obj["access_token"_L1].toString();

  set_access_token(access_token);

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("access_token", access_token);
  s.endGroup();

  qLog(Debug) << "Genius: Authentication was successful.";

  Q_EMIT AuthenticationComplete(true);
  Q_EMIT AuthenticationSuccess();

}

void GeniusLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  GeniusLyricsSearchContextPtr search = make_shared<GeniusLyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.insert(id, search);

  if (access_token().isEmpty()) {
    EndSearch(search);
    return;
  }

  QUrlQuery url_query;
  url_query.addQueryItem(QStringLiteral("q"), QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("%1 %2").arg(request.artist, request.title))));

  QUrl url(QString::fromLatin1(kUrlSearch));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setRawHeader("Authorization", "Bearer " + access_token().toUtf8());
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
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

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) {
    EndSearch(search);
    return;
  }

  if (!json_obj.contains("meta"_L1)) {
    Error(QStringLiteral("Json reply is missing meta object."), json_obj);
    EndSearch(search);
    return;
  }
  if (!json_obj["meta"_L1].isObject()) {
    Error(QStringLiteral("Json reply meta is not an object."), json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_meta = json_obj["meta"_L1].toObject();
  if (!obj_meta.contains("status"_L1)) {
    Error(QStringLiteral("Json reply meta object is missing status."), obj_meta);
    EndSearch(search);
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
    EndSearch(search);
    return;
  }

  if (!json_obj.contains("response"_L1)) {
    Error(QStringLiteral("Json reply is missing response."), json_obj);
    EndSearch(search);
    return;
  }
  if (!json_obj["response"_L1].isObject()) {
    Error(QStringLiteral("Json response is not an object."), json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_response = json_obj["response"_L1].toObject();
  if (!obj_response.contains("hits"_L1)) {
    Error(QStringLiteral("Json response is missing hits."), obj_response);
    EndSearch(search);
    return;
  }
  if (!obj_response["hits"_L1].isArray()) {
    Error(QStringLiteral("Json hits is not an array."), obj_response);
    EndSearch(search);
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
      Error(QStringLiteral("Missing one or more values in result object"), obj_result);
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

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *new_reply = network_->get(req);
    replies_ << new_reply;
    QObject::connect(new_reply, &QNetworkReply::finished, this, [this, new_reply, search, url]() { HandleLyricReply(new_reply, search->id, url); });

  }

  EndSearch(search);

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
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search, lyric);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Empty reply received from server."));
    EndSearch(search, lyric);
    return;
  }

  const QString content = QString::fromUtf8(data);
  QString lyrics = HtmlLyricsProvider::ParseLyricsFromHTML(content, QRegularExpression(QStringLiteral("<div[^>]*>")), QRegularExpression(QStringLiteral("<\\/div>")), QRegularExpression(QStringLiteral("<div data-lyrics-container=[^>]+>")), true);
  if (lyrics.isEmpty()) {
    lyrics = HtmlLyricsProvider::ParseLyricsFromHTML(content, QRegularExpression(QStringLiteral("<div[^>]*>")), QRegularExpression(QStringLiteral("<\\/div>")), QRegularExpression(QStringLiteral("<div class=\"lyrics\">")), true);
  }

  if (!lyrics.isEmpty()) {
    LyricsSearchResult result(lyrics);
    result.artist = lyric.artist;
    result.title = lyric.title;
    search->results.append(result);
  }

  EndSearch(search, lyric);

}

void GeniusLyricsProvider::AuthError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  for (const QString &e : std::as_const(login_errors_)) Error(e);
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT AuthenticationFailure(login_errors_);
  Q_EMIT AuthenticationComplete(false, login_errors_);

  login_errors_.clear();

}

void GeniusLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "GeniusLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

void GeniusLyricsProvider::EndSearch(GeniusLyricsSearchContextPtr search, const GeniusLyricsLyricContext &lyric) {

  if (search->requests_lyric_.contains(lyric.url)) {
    search->requests_lyric_.remove(lyric.url);
  }
  if (search->requests_lyric_.count() == 0) {
    requests_search_.remove(search->id);
    if (search->results.isEmpty()) {
      qLog(Debug) << "GeniusLyrics: No lyrics for" << search->request.artist << search->request.title;
    }
    else {
      qLog(Debug) << "GeniusLyrics: Got lyrics for" << search->request.artist << search->request.title;
    }
    Q_EMIT SearchFinished(search->id, search->results);
  }

}
