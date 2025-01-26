/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "dropboxservice.h"
#include "dropboxbaserequest.h"
#include "dropboxstreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;

DropboxStreamURLRequest::DropboxStreamURLRequest(const SharedPtr<NetworkAccessManager> network, DropboxService *service, const uint id, const QUrl &media_url, QObject *parent)
    : DropboxBaseRequest(network, service, parent),
      network_(network),
      service_(service),
      id_(id),
      media_url_(media_url),
      reply_(nullptr) {}

DropboxStreamURLRequest::~DropboxStreamURLRequest() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
    reply_ = nullptr;
  }

}

void DropboxStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }

}

void DropboxStreamURLRequest::Process() {

  GetStreamURL();

}

void DropboxStreamURLRequest::GetStreamURL() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  reply_ = GetTemporaryLink(media_url_);
  QObject::connect(reply_, &QNetworkReply::finished, this, &DropboxStreamURLRequest::StreamURLReceived);

}

void DropboxStreamURLRequest::StreamURLReceived() {

  const QScopeGuard finish = qScopeGuard([this]() { Finish(); });

  if (!reply_) return;

  Q_ASSERT(replies_.contains(reply_));
  replies_.removeAll(reply_);

  const JsonObjectResult json_object_result = ParseJsonObject(reply_).json_object;

  QObject::disconnect(reply_, nullptr, this, nullptr);
  reply_->deleteLater();
  reply_ = nullptr;

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty() || !json_object.contains("link"_L1)) {
    Error(u"Could not parse stream URL"_s);
    return;
  }

  stream_url_ = QUrl::fromEncoded(json_object["link"_L1].toVariant().toByteArray());
  success_ = stream_url_.isValid();

}

void DropboxStreamURLRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << service_name() << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  error_ = error_message;

}

void DropboxStreamURLRequest::Finish() {

  Q_EMIT StreamURLRequestFinished(id_, media_url_, success_, stream_url_, error_);

}
