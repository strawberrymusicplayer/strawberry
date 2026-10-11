/*
 * Strawberry Music Player
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
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

#ifndef LOCALREDIRECTSERVER_H
#define LOCALREDIRECTSERVER_H

#include <QtGlobal>
#include <QTcpServer>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QUrl>

class QTcpSocket;

class LocalRedirectServer : public QTcpServer {
  Q_OBJECT

 public:
  explicit LocalRedirectServer(QObject *parent = nullptr);
  ~LocalRedirectServer() override;

  const QUrl &url() const { return url_; }
  const QUrl &request_url() const { return request_url_; }
  bool success() const { return success_; }
  const QString &error() const { return error_; }

  int port() const { return port_; }
  void set_port(const int port) { port_ = port; }

  bool Listen();

 Q_SIGNALS:
  void Finished();

 public Q_SLOTS:
  void NewConnection();
  void incomingConnection(qintptr socket_descriptor) override;

 private Q_SLOTS:
  void ReadyRead();
  void Disconnected();

 private:
  void WriteTemplate(QTcpSocket *socket) const;
  static void WriteNotFound(QTcpSocket *socket);
  QUrl ParseUrlFromRequest(const QByteArray &request) const;
  static bool IsRedirect(const QUrl &url);

 private:
  int port_;
  QUrl url_;
  QUrl request_url_;
  // The request received so far on each connection, a request can arrive in several parts, and other connections than the redirect must not end it.
  QHash<QTcpSocket*, QByteArray> requests_;
  bool success_;
  QString error_;
};

#endif  // LOCALREDIRECTSERVER_H
