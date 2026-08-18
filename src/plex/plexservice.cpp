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

#include <algorithm>
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
#include <QHostAddress>
#include <QNetworkInterface>
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
using std::make_shared;

const Song::Source PlexService::kSource = Song::Source::Plex;

namespace {
constexpr char kSongsTable[] = "plex_songs";
constexpr char kPlexTvUrl[] = "https://plex.tv";
constexpr char kClientProduct[] = "Strawberry";
constexpr int kPinPollIntervalMsec = 2000;
constexpr int kPinMaxPolls = 150;  // 5 minutes
constexpr int kConnectionTestTimeoutMsec = 3000;  // Keep this short: we may need to try several candidates in sequence.

// Tailscale (and other CGNAT-based overlay networks) use 100.64.0.0/10.
bool IsTailscaleAddress(const QHostAddress &address) {
  static const QHostAddress kCgnatBase(u"100.64.0.0"_s);
  return address.protocol() == QAbstractSocket::IPv4Protocol && address.isInSubnet(kCgnatBase, 10);
}

// RFC1918 private address ranges.
bool IsPrivateAddress(const QHostAddress &address) {
  if (address.protocol() != QAbstractSocket::IPv4Protocol) return false;
  static const QHostAddress k10(u"10.0.0.0"_s);
  static const QHostAddress k172_16(u"172.16.0.0"_s);
  static const QHostAddress k192_168(u"192.168.0.0"_s);
  return address.isInSubnet(k10, 8) || address.isInSubnet(k172_16, 12) || address.isInSubnet(k192_168, 16);
}
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
      pin_poll_reply_(nullptr),
      connection_test_index_(-1),
      connection_test_reply_(nullptr) {

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

SharedPtr<QNetworkAccessManager> PlexService::network() {

  if (!network_) {
    network_ = make_shared<QNetworkAccessManager>();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
  }

  return network_;

}

void PlexService::Authenticate() {

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
  QNetworkReply *reply = network()->post(network_request, QByteArray());
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
  QNetworkReply *reply = network()->get(CreatePlexTvRequest(url));
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

  QUrl url(QLatin1String(kPlexTvUrl) + "/api/v2/resources"_L1);
  QUrlQuery url_query;
  url_query.addQueryItem(u"includeHttps"_s, u"1"_s);
  // Relay is only ever used as a last-resort fallback (see OrderConnectionCandidates), but we still want it available in case no direct connection is reachable.
  url_query.addQueryItem(u"includeRelay"_s, u"1"_s);
  url.setQuery(url_query);

  QNetworkReply *reply = network()->get(CreatePlexTvRequest(url));
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
    const QString machine_identifier = object_device["clientIdentifier"_L1].toString();
    const QJsonArray array_connections = object_device["connections"_L1].toArray();

    // Parse *all* advertised connections for this server; do not assume the first one returned is reachable or correct.
    ConnectionList connections;
    for (const QJsonValue &value_connection : array_connections) {
      const QJsonObject object_connection = value_connection.toObject();
      const QUrl uri(object_connection["uri"_L1].toString());
      if (!uri.isValid() || uri.host().isEmpty()) continue;
      Connection connection;
      connection.uri = uri;
      connection.protocol = object_connection["protocol"_L1].toString();
      connection.local = object_connection["local"_L1].toBool();
      connection.relay = object_connection["relay"_L1].toBool();
      connections << connection;
    }
    if (connections.isEmpty()) continue;

    Server server;
    server.name = name;
    server.access_token = access_token;
    server.owned = owned;
    server.machine_identifier = machine_identifier;
    server.connections = OrderConnectionCandidates(connections);
    server.url = server.connections.first().uri;  // Best guess only, for display purposes (e.g. the settings page server list); see BeginConnectionSelection() for the actual reachability-tested selection.
    servers << server;
  }

  // Figure out which of the returned servers we should (re-)select a working connection for: prefer matching by
  // the machine identifier we last connected to (stable even if all connection URIs changed), then fall back to
  // matching the currently configured URL, and finally default to the first server if nothing is configured yet.
  const Server *target_server = nullptr;

  Settings s;
  s.beginGroup(PlexSettings::kSettingsGroup);
  const QString configured_machine_identifier = s.value(PlexSettings::kServerMachineIdentifier).toString();
  s.endGroup();

  if (!configured_machine_identifier.isEmpty()) {
    for (const Server &server : std::as_const(servers)) {
      if (server.machine_identifier == configured_machine_identifier) {
        target_server = &server;
        break;
      }
    }
  }

  if (!target_server && server_url_.isValid() && !server_url_.host().isEmpty()) {
    const QUrl server_url_normalized = server_url_.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
    for (const Server &server : std::as_const(servers)) {
      for (const Connection &connection : std::as_const(server.connections)) {
        if (connection.uri.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments) == server_url_normalized) {
          target_server = &server;
          break;
        }
      }
      if (target_server) break;
    }
  }

  if (!target_server && (server_url_.isEmpty() || !server_url_.isValid() || server_url_.host().isEmpty()) && !servers.isEmpty()) {
    target_server = &servers.first();
  }

  if (target_server) {
    BeginConnectionSelection(*target_server);
  }

  Q_EMIT ServersFound(servers);

}

bool PlexService::HostLikelyReachableOnLan(const QString &host) {

  const QHostAddress address(host);
  if (address.isNull()) return false;  // Not a literal IP; skip the heuristic rather than triggering a DNS lookup here.

  const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
  for (const QNetworkInterface &iface : interfaces) {
    if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
    const QList<QNetworkAddressEntry> address_entries = iface.addressEntries();
    for (const QNetworkAddressEntry &entry : address_entries) {
      if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
      if (address.isInSubnet(entry.ip(), entry.prefixLength())) return true;
      // Tailscale (and similar overlay networks) often report a /32 prefix on the local interface, so fall back to a broad CGNAT range match.
      if (IsTailscaleAddress(address) && IsTailscaleAddress(entry.ip())) return true;
    }
  }

  return false;

}

PlexService::ConnectionList PlexService::OrderConnectionCandidates(const ConnectionList &connections) {

  struct ScoredConnection {
    Connection connection;
    int score;
  };

  QList<ScoredConnection> scored;
  scored.reserve(connections.count());

  for (const Connection &connection : connections) {
    const QString host = connection.uri.host();
    const QHostAddress address(host);
    const bool is_private = !address.isNull() && IsPrivateAddress(address);
    const bool is_tailscale = !address.isNull() && IsTailscaleAddress(address);
    const bool on_lan = (is_private || is_tailscale) && HostLikelyReachableOnLan(host);

    int score = 0;
    if (connection.relay) {
      score = 4;  // Relay is only ever a last resort.
    }
    else if (connection.local && on_lan) {
      score = 0;  // Advertised as local, and it matches one of our own interfaces: try this first.
    }
    else if (!is_private && !is_tailscale) {
      score = 1;  // Public/direct address: try before unverified private or Tailscale addresses.
    }
    else if (on_lan) {
      score = 2;  // Private/Tailscale address that matches one of our interfaces even though not flagged "local".
    }
    else {
      score = 3;  // Private, RFC1918 or Tailscale address we have no evidence we can reach: try after the public address, before relay.
    }

    scored << ScoredConnection{connection, score};
  }

  std::stable_sort(scored.begin(), scored.end(), [](const ScoredConnection &a, const ScoredConnection &b) { return a.score < b.score; });

  ConnectionList ordered;
  ordered.reserve(scored.count());
  for (const ScoredConnection &scored_connection : std::as_const(scored)) {
    ordered << scored_connection.connection;
  }

  return ordered;

}

void PlexService::BeginConnectionSelection(const Server &server) {

  if (connection_test_reply_) {
    QNetworkReply *old_reply = connection_test_reply_;
    connection_test_reply_ = nullptr;
    replies_.removeAll(old_reply);
    QObject::disconnect(old_reply, nullptr, this, nullptr);
    if (old_reply->isRunning()) old_reply->abort();
    old_reply->deleteLater();
  }

  connection_test_server_ = server;
  connection_test_candidates_ = server.connections;
  connection_test_index_ = -1;

  qLog(Debug) << "Plex: Selecting a reachable connection for server" << server.name << "from" << connection_test_candidates_.count() << "candidate(s):";
  for (const Connection &connection : std::as_const(connection_test_candidates_)) {
    qLog(Debug) << "Plex:  -" << connection.uri.toString()
                << "protocol:" << (connection.protocol.isEmpty() ? connection.uri.scheme() : connection.protocol)
                << "local:" << connection.local
                << "relay:" << connection.relay;
  }

  TryNextConnectionCandidate();

}

void PlexService::TryNextConnectionCandidate() {

  ++connection_test_index_;
  if (connection_test_index_ >= connection_test_candidates_.count()) {
    qLog(Error) << "Plex: Could not reach any advertised connection for server" << connection_test_server_.name << "- keeping previous configuration, if any.";
    connection_test_candidates_.clear();
    connection_test_index_ = -1;
    return;
  }

  const Connection candidate = connection_test_candidates_.at(connection_test_index_);

  QUrl identity_url(candidate.uri);
  identity_url.setPath(identity_url.path() + "/identity"_L1);

  QNetworkRequest network_request(identity_url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setTransferTimeout(kConnectionTestTimeoutMsec);
  network_request.setRawHeader("Accept", "application/json");
  network_request.setRawHeader("X-Plex-Client-Identifier", client_id_.toUtf8());
  const QString test_token = connection_test_server_.owned ? token_ : connection_test_server_.access_token;
  if (!test_token.isEmpty()) {
    network_request.setRawHeader("X-Plex-Token", test_token.toUtf8());
  }

  if (identity_url.scheme() == "https"_L1 && !verify_certificate_) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

  qLog(Debug) << "Plex: Trying connection" << identity_url.toString();

  QNetworkReply *reply = network()->get(network_request);
  replies_ << reply;
  connection_test_reply_ = reply;
  const QUrl candidate_uri = candidate.uri;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, candidate_uri]() { HandleConnectionTestReply(reply, candidate_uri); });

}

void PlexService::HandleConnectionTestReply(QNetworkReply *reply, const QUrl &candidate_uri) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  if (reply == connection_test_reply_) connection_test_reply_ = nullptr;
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  bool reachable = false;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    const QJsonDocument json_doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject media_container = json_doc.object()["MediaContainer"_L1].toObject();
    const QString returned_machine_identifier = media_container["machineIdentifier"_L1].toString();
    if (!connection_test_server_.machine_identifier.isEmpty() && !returned_machine_identifier.isEmpty() && returned_machine_identifier != connection_test_server_.machine_identifier) {
      qLog(Warning) << "Plex: Connection" << candidate_uri.toString() << "responded, but machineIdentifier" << returned_machine_identifier << "does not match expected" << connection_test_server_.machine_identifier << "- skipping.";
    }
    else {
      reachable = true;
    }
  }
  else {
    qLog(Debug) << "Plex: Connection" << candidate_uri.toString() << "is not reachable:" << reply->errorString();
  }

  if (!reachable) {
    // Do not cache this failed URI; simply move on to the next candidate.
    TryNextConnectionCandidate();
    return;
  }

  qLog(Debug) << "Plex: Selected reachable connection" << candidate_uri.toString() << "for server" << connection_test_server_.name;

  server_url_ = candidate_uri;
  server_token_ = connection_test_server_.owned ? QString() : connection_test_server_.access_token;

  Settings s;
  s.beginGroup(PlexSettings::kSettingsGroup);
  s.setValue(PlexSettings::kServerUrl, server_url_);
  s.setValue(PlexSettings::kServerName, connection_test_server_.name);
  s.setValue(PlexSettings::kServerToken, server_token_.toUtf8().toBase64());
  if (!connection_test_server_.machine_identifier.isEmpty()) {
    s.setValue(PlexSettings::kServerMachineIdentifier, connection_test_server_.machine_identifier);
  }
  s.endGroup();

  connection_test_candidates_.clear();
  connection_test_index_ = -1;

}

void PlexService::SendPing() {
  SendPingWithSettings(server_url_, server_token());
}

void PlexService::SendPingWithSettings(const QUrl &url, const QString &token) {

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

  QNetworkReply *reply = network()->get(network_request);
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
