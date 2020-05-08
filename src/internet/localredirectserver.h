/*
 * This file was part of Clementine.
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LOCALREDIRECTSERVER_H
#define LOCALREDIRECTSERVER_H

#include <QtGlobal>
#include <QByteArray>
#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <QTcpServer>
#include <QSslCertificate>
#include <QSslKey>

class QAbstractSocket;

class LocalRedirectServer : public QTcpServer {
  Q_OBJECT

 public:
  explicit LocalRedirectServer(QObject *parent = nullptr);
  ~LocalRedirectServer();

  void set_https(const bool https) { https_ = https; }
  void set_port(const int port) { port_ = port; }
  bool Listen();
  const QUrl &url() const { return url_; }
  const QUrl &request_url() const { return request_url_; }
  const QString &error() const { return error_; }

 signals:
  void Finished();

 public slots:
  void NewConnection();
  void incomingConnection(qintptr socket_descriptor);
  void SSLErrors(const QList<QSslError> &errors);
  void Encrypted();
  void Connected();
  void Disconnected();
  void ReadyRead();

 private:
  bool GenerateCertificate();
  void WriteTemplate() const;
  QUrl ParseUrlFromRequest(const QByteArray &request) const;

 private:
  bool https_;
  int port_;
  QUrl url_;
  QUrl request_url_;
  QSslCertificate ssl_certificate_;
  QSslKey ssl_key_;
  QAbstractSocket *socket_;
  QByteArray buffer_;
  QString error_;

};

#endif
