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

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/logging.h"
#include "networkaccessmanager.h"
#include "httpbaserequest.h"

using namespace Qt::Literals::StringLiterals;

HttpBaseRequest::HttpBaseRequest(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      network_(network) {}

HttpBaseRequest::~HttpBaseRequest() {

  if (!replies_.isEmpty()) {
    qLog(Debug) << "Aborting" << replies_.count() << "network replies";
    while (!replies_.isEmpty()) {
      QNetworkReply *reply = replies_.takeFirst();
      QObject::disconnect(reply, nullptr, this, nullptr);
      reply->abort();
      reply->deleteLater();
    }
  }

}

QNetworkReply *HttpBaseRequest::CreateGetRequest(const QUrl &url, const bool fake_user_agent_header) {

  return CreateGetRequest(url, QUrlQuery(), fake_user_agent_header);

}

QNetworkReply *HttpBaseRequest::CreateGetRequest(const QUrl &url, const ParamList &params, const bool fake_user_agent_header) {

  QUrlQuery url_query;

  if (!params.isEmpty()) {
    ParamList sorted_params = params;
    std::sort(sorted_params.begin(), sorted_params.end());
    for (const Param &param : sorted_params) {
      url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
    }
  }

  return CreateGetRequest(url, url_query, fake_user_agent_header);

}

QNetworkReply *HttpBaseRequest::CreateGetRequest(const QUrl &url, const QUrlQuery &url_query, const bool fake_user_agent_header) {

  QUrl request_url(url);

  if (!url_query.isEmpty()) {
    request_url.setQuery(url_query);
  }

  QNetworkRequest network_request(request_url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  if (use_authorization_header() && authenticated()) {
    network_request.setRawHeader("Authorization", authorization_header());
  }
  if (fake_user_agent_header) {
    network_request.setHeader(QNetworkRequest::UserAgentHeader, u"Mozilla/5.0 (X11; Linux x86_64; rv:122.0) Gecko/20100101 Firefox/122.0"_s);
  }
  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &HttpBaseRequest::HandleSSLErrors);
  replies_ << reply;

  //qLog(Debug) << service_name() << "Sending get request" << request_url;

  return reply;

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QByteArray &content_type_header, const QByteArray &data) {

  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, content_type_header);
  if (use_authorization_header() && authenticated()) {
    network_request.setRawHeader("Authorization", authorization_header());
  }
  QNetworkReply *reply = network_->post(network_request, data);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &HttpBaseRequest::HandleSSLErrors);
  replies_ << reply;

  //qLog(Debug) << service_name() << "Sending post request" << url << data;

  return reply;

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QUrlQuery &url_query) {

  return CreatePostRequest(url, "application/x-www-form-urlencoded", url_query.toString(QUrl::FullyEncoded).toUtf8());

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const ParamList &params) {

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  return CreatePostRequest(url, url_query);

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QJsonDocument &json_document) {

  return CreatePostRequest(url, "application/json; charset=utf-8", json_document.toJson());

}

QNetworkReply *HttpBaseRequest::CreatePostRequest(const QUrl &url, const QJsonObject &json_object) {

  return CreatePostRequest(url, QJsonDocument(json_object));

}

void HttpBaseRequest::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    Error(ssl_error.errorString());
  }

}

HttpBaseRequest::ReplyDataResult HttpBaseRequest::GetReplyData(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() >= 200) {
      reply->readAll(); // QTBUG-135641
    }
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    const int http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http_status_code < 200 || http_status_code > 207) {
      reply->readAll(); // QTBUG-135641
      return ReplyDataResult(ErrorCode::HttpError, QStringLiteral("Received HTTP code %1").arg(http_status_code));
    }
  }

  return reply->readAll();

}

void HttpBaseRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << service_name() << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

}
