/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QIODevice>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkInformation>
#include <QObject>
#include <QList>
#include <QMetaObject>
#include <QUrl>
#include <QLibraryInfo>
#include <QVersionNumber>

#include "utilities/useragent.h"
#include "networkaccessmanager.h"
#include "threadsafenetworkdiskcache.h"

using namespace Qt::Literals::StringLiterals;

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent),
      clear_cache_pending_(false),
      cancel_savings_of_finished_replies_(!QtCancelsSavingOfDeletedReply(QLibraryInfo::version())) {

  setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
  setCache(new ThreadSafeNetworkDiskCache(this));

  // Handle network state changes after system suspend/resume
  // QNetworkInformation provides cross-platform network reachability monitoring in Qt 6
  if (QNetworkInformation::loadDefaultBackend()) {
    QNetworkInformation *network_info = QNetworkInformation::instance();
    if (network_info) {
      QObject::connect(network_info, &QNetworkInformation::reachabilityChanged, this, &NetworkAccessManager::ReachabilityChanged);
    }
  }

}

NetworkAccessManager::~NetworkAccessManager() {

  // QNetworkAccessManager's destructor deletes the replies, which would call ReplyDone() after this destructor.
  for (const QList<QMetaObject::Connection> &connections : std::as_const(running_replies_)) {
    for (const QMetaObject::Connection &connection : connections) {
      QObject::disconnect(connection);
    }
  }
  for (const QMetaObject::Connection &connection : std::as_const(replies_)) {
    QObject::disconnect(connection);
  }

}

void NetworkAccessManager::ReachabilityChanged(const QNetworkInformation::Reachability reachability) {

  if (reachability != QNetworkInformation::Reachability::Online) return;

  // Clear the connection cache to force reconnecting after the network becomes available, this fixes issues after system suspend/resume.
  // Clearing it stops the network thread, which also cancels the running requests, so wait until they are finished.
  clear_cache_pending_ = true;
  ClearCacheIfIdle();

}

void NetworkAccessManager::ClearCacheIfIdle() {

  if (!clear_cache_pending_ || !running_replies_.isEmpty()) return;

  clear_cache_pending_ = false;
  clearAccessCache();

}

void NetworkAccessManager::ReplyDone(QObject *reply) {

  if (!running_replies_.contains(reply)) return;

  // A finished reply can still be deleted by QNetworkAccessManager's destructor, which would call this after this class' destructor.
  // Only this class' connections are disconnected, QNetworkAccessManager has connections from the reply to the manager too.
  const QList<QMetaObject::Connection> connections = running_replies_.take(reply);
  for (const QMetaObject::Connection &connection : connections) {
    QObject::disconnect(connection);
  }

  // This is connected before the receivers of the finished signal, so the cache is cleared before they can start new requests, which would postpone it again.
  ClearCacheIfIdle();

}

bool NetworkAccessManager::QtCancelsSavingOfDeletedReply(const QVersionNumber &qt_version) {

  if (qt_version >= QVersionNumber(6, 10, 0)) return true;

  switch (qt_version.minorVersion()) {
    case 5:
      return qt_version.microVersion() >= 10;
    case 8:
      return qt_version.microVersion() >= 5;
    case 9:
      return qt_version.microVersion() >= 2;
    default:
      return false;
  }

}

void NetworkAccessManager::ReplyDeleted(QObject *reply) {

  if (!replies_.remove(reply)) return;

  CancelUnusedSavings();

}

void NetworkAccessManager::ReplyFinished(QNetworkReply *reply) {

  if (SavingFinished(reply)) {
    CancelUnusedSavings();
  }

}

bool NetworkAccessManager::SavingFinished(QNetworkReply *reply) const {

  // With the later Qt versions, the saving is cancelled when the reply is deleted, cancelling it before could cancel another reply's saving of the URL then.
  // A finished reply doesn't use its saving anymore, it has saved the data in the cache when it got all of it, or it has failed, except when the data is compressed, it's written to the saving when it's read then.
  // So a saving of the URL left by a reply which was deleted while it was saving is cancelled when the other replies with the URL have finished, and not kept as long as there are new replies with the URL.
  return cancel_savings_of_finished_replies_ && reply->isFinished() && !reply->hasRawHeader("Content-Encoding");

}

void NetworkAccessManager::CancelUnusedSavings() {

  ThreadSafeNetworkDiskCache *disk_cache = qobject_cast<ThreadSafeNetworkDiskCache*>(cache());
  if (!disk_cache) return;

  // The replies in replies_ are still alive, each one is removed when it is deleted.
  QList<QUrl> reply_urls;
  reply_urls.reserve(replies_.count());
  for (QHash<QObject*, QMetaObject::Connection>::const_iterator it = replies_.constBegin(); it != replies_.constEnd(); ++it) {
    QNetworkReply *reply = static_cast<QNetworkReply*>(it.key());
    if (!SavingFinished(reply)) {
      reply_urls << ThreadSafeNetworkDiskCache::CacheUrl(reply->url());
    }
  }
  disk_cache->CancelUnusedSavings(reply_urls);

}

QNetworkReply *NetworkAccessManager::createRequest(Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) {

  const QVariant user_agent_header = network_request.header(QNetworkRequest::UserAgentHeader);
  const QByteArray user_agent = user_agent_header.isValid() ? user_agent_header.toByteArray() : Utilities::UserAgent();

  QNetworkRequest new_network_request(network_request);
  // Keep a redirect policy set by the caller, for example one that doesn't send credentials to another server.
  if (!network_request.attribute(QNetworkRequest::RedirectPolicyAttribute).isValid()) {
    new_network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  }
  new_network_request.setHeader(QNetworkRequest::UserAgentHeader, user_agent);

  if (op == QNetworkAccessManager::PostOperation && !new_network_request.header(QNetworkRequest::ContentTypeHeader).isValid()) {
    new_network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  }

  // Prefer the cache unless the caller has changed the setting already
  if (!network_request.attribute(QNetworkRequest::CacheLoadControlAttribute).isValid()) {
    new_network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
  }

  if (new_network_request.transferTimeout() == 0) {
    new_network_request.setTransferTimeout(QNetworkRequest::DefaultTransferTimeoutConstant);
  }

  QNetworkReply *reply = QNetworkAccessManager::createRequest(op, new_network_request, outgoing_data);
  // A reply emits finished when it fails or is aborted too, but not when it is deleted while running.
  QMetaObject::Connection finished_connection = QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    ReplyDone(reply);
    ReplyFinished(reply);
  });
  QMetaObject::Connection destroyed_connection = QObject::connect(reply, &QObject::destroyed, this, &NetworkAccessManager::ReplyDone);
  running_replies_.insert(reply, QList<QMetaObject::Connection>{finished_connection, destroyed_connection});
  replies_.insert(reply, QObject::connect(reply, &QObject::destroyed, this, &NetworkAccessManager::ReplyDeleted));

  return reply;

}
