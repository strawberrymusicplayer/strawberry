/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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
#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/urlhandlers.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "plexservice.h"
#include "plexurlhandler.h"
#include "plexrequest.h"
#include "plexbaserequest.h"
#include "constants/plexsettings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_unique;
using std::make_shared;

const Song::Source PlexService::kSource = Song::Source::Plex;

namespace {
constexpr char kSongsTable[] = "plex_songs";
constexpr char kPlexTvUrl[] = "https://plex.tv";
constexpr char kClientProduct[] = "Strawberry";
constexpr int kPinPollIntervalMsec = 2000;
constexpr int kPinMaxPolls = 150;  // 5 minutes
}  // namespace

PlexService::PlexService(const SharedPtr<TaskManager> task_manager,
                         const SharedPtr<Database> database,
                         const SharedPtr<UrlHandlers> url_handlers,
                         const SharedPtr<AlbumCoverLoader> albumcover_loader,
                         QObject *parent)
    : StreamingService(Song::Source::Plex, u"Plex"_s, u"plex"_s, QLatin1String(PlexSettings::kSettingsGroup), parent),
      url_handler_(new PlexUrlHandler(this)),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      verify_certificate_(PlexSettings::kDefaultVerifyCertificate),
      download_album_covers_(PlexSettings::kDefaultDownloadAlbumCovers),
      last_update_(0),
      pin_id_(0),
      pin_polls_remaining_(0),
      pin_poll_reply_(nullptr) {

  url_handlers->Register(url_handler_);

  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->moveToThread(database->thread());
  collection_backend_->Init(database, task_manager, Song::Source::Plex, QLatin1String(kSongsTable));
  collection_model_ = new CollectionModel(collection_backend_, albumcover_loader, this);

  timer_pin_poll_.setInterval(kPinPollIntervalMsec);
  QObject::connect(&timer_pin_poll_, &QTimer::timeout, this, &PlexService::PollPin);

  PlexService::ReloadSettings();

}

PlexService::~PlexService() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void PlexService::Exit() {

  QObject::connect(&*collection_backend_, &CollectionBackend::ExitFinished, this, &PlexService::ExitFinished);
  collection_backend_->ExitAsync();

}

void PlexService::ReloadSettings() {

  Settings s;
  s.beginGroup(PlexSettings::kSettingsGroup);

  server_url_ = s.value(PlexSettings::kServerUrl).toUrl();
  const QByteArray token = s.value(PlexSettings::kToken).toByteArray();
  if (token.isEmpty()) token_.clear();
  else token_ = QString::fromUtf8(QByteArray::fromBase64(token));
  const QByteArray server_token = s.value(PlexSettings::kServerToken).toByteArray();
  if (server_token.isEmpty()) server_token_.clear();
  else server_token_ = QString::fromUtf8(QByteArray::fromBase64(server_token));
  client_id_ = s.value(PlexSettings::kClientId).toString();
  verify_certificate_ = s.value(PlexSettings::kVerifyCertificate, PlexSettings::kDefaultVerifyCertificate).toBool();
  download_album_covers_ = s.value(PlexSettings::kDownloadAlbumCovers, PlexSettings::kDefaultDownloadAlbumCovers).toBool();
  last_update_ = s.value(PlexSettings::kLastUpdate, 0).toLongLong();

  if (client_id_.isEmpty()) {
    client_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.setValue(PlexSettings::kClientId, client_id_);
  }

  s.endGroup();

}

QNetworkRequest PlexService::CreatePlexTvRequest(const QUrl &url) const {

  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setTransferTimeout(QNetworkRequest::DefaultTransferTimeoutConstant);
  network_request.setRawHeader("Accept", "application/json");
  network_request.setRawHeader("X-Plex-Product", kClientProduct);
  network_request.setRawHeader("X-Plex-Client-Identifier", client_id_.toUtf8());
  if (!token_.isEmpty()) {
    network_request.setRawHeader("X-Plex-Token", token_.toUtf8());
  }

  return network_request;

}

void PlexService::Authenticate() {

  if (!network_) {
    network_ = make_unique<QNetworkAccessManager>();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
  }

  timer_pin_poll_.stop();
  pin_id_ = 0;
  pin_code_.clear();
  errors_.clear();

  QUrl url(QLatin1String(kPlexTvUrl) + "/api/v2/pins"_L1);
  QUrlQuery url_query;
  url_query.addQueryItem(u"strong"_s, u"true"_s);
  url.setQuery(url_query);

  QNetworkRequest network_request = CreatePlexTvRequest(url);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  QNetworkReply *reply = network_->post(network_request, QByteArray());
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandlePinReply(reply); });

}

void PlexService::CancelAuthentication() {

  timer_pin_poll_.stop();
  pin_id_ = 0;
  pin_code_.clear();

}

void PlexService::Deauthenticate() {

  CancelAuthentication();

  token_.clear();
  server_token_.clear();

  Settings s;
  s.beginGroup(PlexSettings::kSettingsGroup);
  s.remove(PlexSettings::kToken);
  s.remove(PlexSettings::kServerToken);
  s.endGroup();

}

void PlexService::HandlePinReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    AuthenticationError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }

  const QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
  const QJsonObject json_obj = json_doc.object();
  if (!json_obj.contains("id"_L1) || !json_obj.contains("code"_L1)) {
    AuthenticationError(u"Pin reply from plex.tv is missing id or code."_s);
    return;
  }

  pin_id_ = json_obj["id"_L1].toVariant().toLongLong();
  pin_code_ = json_obj["code"_L1].toString();
  pin_polls_remaining_ = kPinMaxPolls;

  QUrl auth_url(u"https://app.plex.tv/auth"_s);
  QUrlQuery auth_url_query;
  auth_url_query.addQueryItem(u"clientID"_s, client_id_);
  auth_url_query.addQueryItem(u"code"_s, pin_code_);
  auth_url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(u"context[device][product]"_s)), QLatin1String(kClientProduct));
  auth_url.setFragment(u"?"_s + auth_url_query.toString(QUrl::FullyEncoded));

  if (!QDesktopServices::openUrl(auth_url)) {
    Q_EMIT LoginFailure(u"Could not open a browser. Open this URL manually to log in:\n"_s + auth_url.toString());
  }

  timer_pin_poll_.start();

}

void PlexService::PollPin() {

  if (pin_id_ == 0) {
    timer_pin_poll_.stop();
    return;
  }

  if (--pin_polls_remaining_ <= 0) {
    CancelAuthentication();
    AuthenticationError(u"Plex login timed out."_s);
    return;
  }

  if (pin_poll_reply_) return;  // The previous poll is still in flight.

  const QUrl url(QLatin1String(kPlexTvUrl) + "/api/v2/pins/"_L1 + QString::number(pin_id_));
  QNetworkReply *reply = network_->get(CreatePlexTvRequest(url));
  replies_ << reply;
  pin_poll_reply_ = reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandlePollPinReply(reply); });

}

void PlexService::HandlePollPinReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  if (reply == pin_poll_reply_) pin_poll_reply_ = nullptr;
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (pin_id_ == 0) return;

  if (reply->error() != QNetworkReply::NoError) {
    CancelAuthentication();
    AuthenticationError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }

  const QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
  const QJsonObject json_obj = json_doc.object();
  const QString token = json_obj["authToken"_L1].toString();
  if (token.isEmpty()) return;  // Not authorized yet, keep polling.

  CancelAuthentication();
  FinishAuthentication(token);

}

void PlexService::FinishAuthentication(const QString &token) {

  token_ = token;

  Settings s;
  s.beginGroup(PlexSettings::kSettingsGroup);
  s.setValue(PlexSettings::kToken, token_.toUtf8().toBase64());
  s.endGroup();

  qLog(Debug) << "Plex: Authentication successful";

  Q_EMIT LoginSuccess();
  Q_EMIT LoginFinished(true);

  GetServers();

}

void PlexService::AuthenticationError(const QString &error) {

  qLog(Error) << "Plex:" << error;

  Q_EMIT LoginFailure(error);
  Q_EMIT LoginFinished(false, error);

}

void PlexService::GetServers() {

  if (token_.isEmpty()) return;

  if (!network_) {
    network_ = make_unique<QNetworkAccessManager>();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
  }

  QUrl url(QLatin1String(kPlexTvUrl) + "/api/v2/resources"_L1);
  QUrlQuery url_query;
  url_query.addQueryItem(u"includeHttps"_s, u"1"_s);
  url_query.addQueryItem(u"includeRelay"_s, u"0"_s);
  url.setQuery(url_query);

  QNetworkReply *reply = network_->get(CreatePlexTvRequest(url));
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleResourcesReply(reply); });

}

void PlexService::HandleResourcesReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << "Plex: Could not retrieve servers:" << reply->errorString();
    return;
  }

  const QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
  const QJsonArray array_devices = json_doc.array();

  ServerList servers;
  for (const QJsonValue &value_device : array_devices) {
    if (!value_device.isObject()) continue;
    const QJsonObject object_device = value_device.toObject();
    if (!object_device["provides"_L1].toString().split(u',').contains("server"_L1)) continue;
    const QString name = object_device["name"_L1].toString();
    const bool owned = object_device["owned"_L1].toBool();
    const QString access_token = object_device["accessToken"_L1].toString();
    const QJsonArray array_connections = object_device["connections"_L1].toArray();
    QUrl url_local;
    QUrl url_remote;
    for (const QJsonValue &value_connection : array_connections) {
      const QJsonObject object_connection = value_connection.toObject();
      if (object_connection["relay"_L1].toBool()) continue;
      const QUrl uri(object_connection["uri"_L1].toString());
      if (!uri.isValid()) continue;
      if (object_connection["local"_L1].toBool()) {
        if (url_local.isEmpty()) url_local = uri;
      }
      else {
        if (url_remote.isEmpty()) url_remote = uri;
      }
    }
    // A local address is only reachable for servers on our own network; for shared servers prefer the remote address.
    QUrl url;
    if (owned) {
      url = url_local.isEmpty() ? url_remote : url_local;
    }
    else {
      url = url_remote.isEmpty() ? url_local : url_remote;
    }
    if (url.isEmpty()) continue;
    Server server;
    server.name = name;
    server.url = url;
    server.access_token = access_token;
    server.owned = owned;
    servers << server;
  }

  if ((server_url_.isEmpty() || !server_url_.isValid() || server_url_.scheme().isEmpty() || server_url_.host().isEmpty()) && !servers.isEmpty()) {
    server_url_ = servers.first().url;
    server_token_ = servers.first().owned ? QString() : servers.first().access_token;
    Settings s;
    s.beginGroup(PlexSettings::kSettingsGroup);
    s.setValue(PlexSettings::kServerUrl, server_url_);
    s.setValue(PlexSettings::kServerName, servers.first().name);
    s.setValue(PlexSettings::kServerToken, server_token_.toUtf8().toBase64());
    s.endGroup();
  }
  else {
    // Refresh the access token for the currently configured server.
    const QUrl server_url_normalized = server_url_.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
    for (const Server &server : std::as_const(servers)) {
      if (server.url.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments) == server_url_normalized) {
        server_token_ = server.owned ? QString() : server.access_token;
        Settings s;
        s.beginGroup(PlexSettings::kSettingsGroup);
        s.setValue(PlexSettings::kServerName, server.name);
        s.setValue(PlexSettings::kServerToken, server_token_.toUtf8().toBase64());
        s.endGroup();
        break;
      }
    }
  }

  Q_EMIT ServersFound(servers);

}

void PlexService::SendPing() {
  SendPingWithSettings(server_url_, server_token());
}

void PlexService::SendPingWithSettings(const QUrl &url, const QString &token) {

  if (!network_) {
    network_ = make_unique<QNetworkAccessManager>();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
  }

  errors_.clear();

  QUrl ping_url(url);
  ping_url.setPath(ping_url.path() + "/identity"_L1);

  QNetworkRequest network_request(ping_url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setTransferTimeout(QNetworkRequest::DefaultTransferTimeoutConstant);
  network_request.setRawHeader("Accept", "application/json");
  network_request.setRawHeader("X-Plex-Client-Identifier", client_id_.toUtf8());
  network_request.setRawHeader("X-Plex-Token", token.toUtf8());

  if (ping_url.scheme() == "https"_L1 && !verify_certificate_) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &PlexService::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandlePingReply(reply); });

}

void PlexService::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    errors_ += ssl_error.errorString();
  }

}

void PlexService::HandlePingReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    PingError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    PingError(QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    return;
  }

  const QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
  const QJsonObject json_obj = json_doc.object();
  if (!json_obj.contains("MediaContainer"_L1)) {
    PingError(u"Ping reply from server is missing MediaContainer."_s);
    return;
  }

  errors_.clear();

  Q_EMIT TestComplete(true);
  Q_EMIT TestSuccess();

}

void PlexService::PingError(const QString &error) {

  if (!error.isEmpty()) errors_ << error;

  QString error_html;
  for (const QString &e : std::as_const(errors_)) {
    qLog(Error) << "Plex:" << e;
    error_html += e + "<br />"_L1;
  }

  Q_EMIT TestFailure(error_html);
  Q_EMIT TestComplete(false, error_html);

  errors_.clear();

}

void PlexService::ResetSongsRequest() {

  if (songs_request_) {
    QObject::disconnect(&*songs_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*songs_request_, nullptr);
    songs_request_.reset();
  }

}

void PlexService::GetSongs() {

  if (!server_url_.isValid()) {
    Q_EMIT SongsResults(SongMap(), tr("Server URL is invalid."));
    return;
  }

  if (token_.isEmpty()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Plex."));
    return;
  }

  ResetSongsRequest();

  SongMap existing_songs;
  if (last_update_ > 0) {
    const SongList songs = collection_backend_->GetAllSongs();
    for (const Song &song : songs) {
      existing_songs.insert(song.song_id(), song);
    }
  }

  songs_request_.reset(new PlexRequest(this, url_handler_, existing_songs, last_update_), [](PlexRequest *request) { request->deleteLater(); });
  QObject::connect(&*songs_request_, &PlexRequest::Results, this, &PlexService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &PlexRequest::UpdateStatus, this, &PlexService::SongsUpdateStatus);
  QObject::connect(&*songs_request_, &PlexRequest::ProgressSetMaximum, this, &PlexService::SongsProgressSetMaximum);
  QObject::connect(&*songs_request_, &PlexRequest::UpdateProgress, this, &PlexService::SongsUpdateProgress);

  songs_request_->GetLibrarySections();

}

void PlexService::DeleteSongs() {

  collection_backend_->DeleteAllAsync();

  last_update_ = 0;
  Settings s;
  s.beginGroup(PlexSettings::kSettingsGroup);
  s.remove(PlexSettings::kLastUpdate);
  s.endGroup();

}

void PlexService::SongsResultsReceived(const SongMap &songs, const QString &error) {

  if (error.isEmpty() && songs_request_) {
    const qint64 newest_updated_at = songs_request_->newest_updated_at();
    if (newest_updated_at > last_update_) {
      last_update_ = newest_updated_at;
      Settings s;
      s.beginGroup(PlexSettings::kSettingsGroup);
      s.setValue(PlexSettings::kLastUpdate, last_update_);
      s.endGroup();
    }
  }

  Q_EMIT SongsResults(songs, error);

  ResetSongsRequest();

}
