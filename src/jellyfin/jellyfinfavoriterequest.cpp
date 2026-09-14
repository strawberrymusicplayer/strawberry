/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>

#include "core/logging.h"
#include "utilities/strutils.h"
#include "jellyfinservice.h"
#include "jellyfinbaserequest.h"
#include "jellyfinfavoriterequest.h"

using namespace Qt::Literals::StringLiterals;

JellyfinFavoriteRequest::JellyfinFavoriteRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JellyfinBaseRequest(service, network, parent) {}

void JellyfinFavoriteRequest::SendChanges(const JellyfinFavoriteChangeList &changes) {

  for (const JellyfinFavoriteChange &change : changes) {
    // A newer change of an item replaces a rejected change of it which is sent again after logging in.
    if (!change.retry) {
      unauthorized_changes_.erase(std::remove_if(unauthorized_changes_.begin(), unauthorized_changes_.end(), [&change](const JellyfinFavoriteChange &unauthorized_change) { return unauthorized_change.item_id == change.item_id; }), unauthorized_changes_.end());
    }
    ++requests_active_[change.catalog];
    // Send changes of the same item in order, adding and removing it at the same time could end in either state on the server.
    if (items_in_flight_.contains(change.item_id)) {
      waiting_changes_ << change;
    }
    else {
      SendChange(change);
    }
  }

}

void JellyfinFavoriteRequest::Clear() {

  AbortNetworkReplies();
  requests_active_.clear();
  errors_.clear();
  items_in_flight_.clear();
  waiting_changes_.clear();
  unauthorized_access_token_.clear();
  unauthorized_changes_.clear();

}

void JellyfinFavoriteRequest::SendChange(const JellyfinFavoriteChange &change) {

  items_in_flight_.insert(change.item_id);

  const QString ressource_path = u"Users/%1/FavoriteItems/%2"_s.arg(service()->user_id(), change.item_id);
  QNetworkReply *reply = change.add ? CreatePostRequest(ressource_path, QJsonObject()) : CreateDeleteRequest(ressource_path);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, change]() { ReplyReceived(reply, change); });

}

void JellyfinFavoriteRequest::ReplyReceived(QNetworkReply *reply, const JellyfinFavoriteChange &change) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  items_in_flight_.remove(change.item_id);

  const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (http_status == 401 && !change.retry) {
    // Send the change again after logging in.
    unauthorized_access_token_ = QString::fromUtf8(reply->request().rawHeader("X-Emby-Token"));
    JellyfinFavoriteChange retry_change = change;
    retry_change.retry = true;
    unauthorized_changes_ << retry_change;
  }
  else if (reply->error() != QNetworkReply::NoError || http_status < 200 || http_status >= 300) {
    // The error of the reply contains the URL, so report the HTTP status instead, the same error for many items is only reported once.
    QString error = JellyfinService::RedirectRefusedError(reply);
    if (error.isEmpty()) {
      error = http_status > 0 ? QStringLiteral("%1 %2").arg(QString::number(http_status), reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()).trimmed() : reply->errorString();
    }
    qLog(Error) << "Jellyfin:" << "Changing favorite" << Utilities::UrlForLog(reply->url()) << "failed:" << error;
    if (!errors_[change.catalog].contains(error)) errors_[change.catalog] << error;
  }
  else {
    // The server accepted the access token.
    service()->TokenValidated();
  }

  // Send the next change of the same item.
  for (qsizetype i = 0; i < waiting_changes_.count(); ++i) {
    if (waiting_changes_.at(i).item_id == change.item_id) {
      SendChange(waiting_changes_.takeAt(i));
      break;
    }
  }

  if (--requests_active_[change.catalog] > 0) return;
  requests_active_.remove(change.catalog);

  // A catalog with rejected changes which are sent again after logging in is loaded again when they finished, a newer change can have replaced them.
  const bool retry_pending = std::any_of(unauthorized_changes_.cbegin(), unauthorized_changes_.cend(), [&change](const JellyfinFavoriteChange &unauthorized_change) { return unauthorized_change.catalog == change.catalog; });

  // Changes of other catalogs can still be in flight, report the rejected changes when all finished.
  if (requests_active_.isEmpty() && !unauthorized_changes_.isEmpty()) {
    const QString access_token = unauthorized_access_token_;
    const JellyfinFavoriteChangeList changes = unauthorized_changes_;
    unauthorized_access_token_.clear();
    unauthorized_changes_.clear();
    Q_EMIT FavoritesUnauthorized(access_token, changes);
  }

  const QStringList errors = errors_.take(change.catalog);
  if (retry_pending && errors.isEmpty()) return;
  Q_EMIT FavoritesChanged(change.catalog, errors.isEmpty() ? QString() : tr("Changing Jellyfin favorites failed: %1").arg(errors.join(u"; "_s)));

}
