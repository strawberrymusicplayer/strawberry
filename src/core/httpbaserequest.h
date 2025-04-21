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

#ifndef HTTPBASEREQUEST_H
#define HTTPBASEREQUEST_H

#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>

#include "includes/shared_ptr.h"

class NetworkAccessManager;

class HttpBaseRequest : public QObject {
  Q_OBJECT

 public:
  explicit HttpBaseRequest(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~HttpBaseRequest() override;

  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  enum class ErrorCode {
    Success,
    NetworkError,
    HttpError,
    APIError,
    ParseError,
  };

  class HttpBaseRequestResult {
   public:
    HttpBaseRequestResult(const ErrorCode _error_code, const QString &_error_message = QString())
        : error_code(_error_code),
          network_error(QNetworkReply::NetworkError::UnknownNetworkError),
          http_status_code(200),
          api_error(-1),
          error_message(_error_message) {}
    ErrorCode error_code;
    QNetworkReply::NetworkError network_error;
    int http_status_code;
    int api_error;
    QString error_message;
    bool success() const { return error_code == ErrorCode::Success; }
  };

  class ReplyDataResult : public HttpBaseRequestResult {
   public:
    ReplyDataResult(const ErrorCode _error_code, const QString &_error_message = QString()) : HttpBaseRequestResult(_error_code, _error_message) {}
    ReplyDataResult(const QByteArray &_data) : HttpBaseRequestResult(ErrorCode::Success), data(_data) {}
    QByteArray data;
  };

  static ReplyDataResult GetReplyData(QNetworkReply *reply);

 protected:
  virtual QString service_name() const = 0;
  virtual bool authentication_required() const = 0;
  virtual bool authenticated() const = 0;
  virtual bool use_authorization_header() const = 0;
  virtual QByteArray authorization_header() const = 0;

  virtual QNetworkReply *CreateGetRequest(const QUrl &url, const bool fake_user_agent_header);
  virtual QNetworkReply *CreateGetRequest(const QUrl &url, const ParamList &params = ParamList(), const bool fake_user_agent_header = false);
  virtual QNetworkReply *CreateGetRequest(const QUrl &url, const QUrlQuery &url_query, const bool fake_user_agent_header = false);
  virtual QNetworkReply *CreatePostRequest(const QUrl &url, const QByteArray &content_type_header, const QByteArray &data);
  virtual QNetworkReply *CreatePostRequest(const QUrl &url, const QUrlQuery &url_query);
  virtual QNetworkReply *CreatePostRequest(const QUrl &url, const ParamList &params);
  virtual QNetworkReply *CreatePostRequest(const QUrl &url, const QJsonDocument &json_document);
  virtual QNetworkReply *CreatePostRequest(const QUrl &url, const QJsonObject &json_object);
  virtual void Error(const QString &error_message, const QVariant &debug_output = QVariant());

 public Q_SLOTS:
  void HandleSSLErrors(const QList<QSslError> &ssl_errors);

 Q_SIGNALS:
  void ShowErrorDialog(const QString &error);

 protected:
  const SharedPtr<NetworkAccessManager> network_;
  QList<QNetworkReply*> replies_;
};

#endif  // HTTPBASEREQUEST_H
