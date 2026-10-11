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

#include <QApplication>
#include <QIODevice>
#include <QBuffer>
#include <QFile>
#include <QList>
#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QHash>
#include <QRegularExpression>
#include <QStyle>
#include <QHostAddress>
#include <QTcpServer>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QDateTime>
#include <QRandomGenerator>

#include "core/logging.h"
#include "localredirectserver.h"

using namespace Qt::Literals::StringLiterals;

namespace {
// Limits the request received on a connection before the end of the headers, so a connection can't make it grow without limit.
constexpr qsizetype kMaxRequestSize = 64LL * 1024LL;
}  // namespace

LocalRedirectServer::LocalRedirectServer(QObject *parent)
    : QTcpServer(parent),
      port_(0),
      success_(false) {}

LocalRedirectServer::~LocalRedirectServer() {
  if (isListening()) close();
}

bool LocalRedirectServer::Listen() {

  if (!listen(QHostAddress::LocalHost, static_cast<quint16>(port_))) {
    success_ = false;
    error_ = errorString();
    return false;
  }

  url_.setScheme(u"http"_s);
  url_.setHost(u"localhost"_s);
  url_.setPort(serverPort());
  url_.setPath(u"/"_s);
  port_ = serverPort();
  QObject::connect(this, &QTcpServer::newConnection, this, &LocalRedirectServer::NewConnection);

  return true;

}

void LocalRedirectServer::NewConnection() {

  while (hasPendingConnections()) {
    incomingConnection(nextPendingConnection()->socketDescriptor());
  }

}

void LocalRedirectServer::incomingConnection(qintptr socket_descriptor) {

  QTcpSocket *socket = new QTcpSocket(this);
  if (!socket->setSocketDescriptor(socket_descriptor)) {
    // Only this connection failed, keep waiting for the redirect.
    qLog(Error) << "Unable to set socket descriptor:" << socket->errorString();
    delete socket;
    return;
  }

  requests_.insert(socket, QByteArray());
  QObject::connect(socket, &QAbstractSocket::readyRead, this, &LocalRedirectServer::ReadyRead);
  QObject::connect(socket, &QAbstractSocket::disconnected, this, &LocalRedirectServer::Disconnected);

}

void LocalRedirectServer::Disconnected() {

  QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
  if (!socket) return;

  requests_.remove(socket);
  socket->deleteLater();

}

void LocalRedirectServer::ReadyRead() {

  QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
  if (!socket || !requests_.contains(socket)) return;

  const QByteArray data = socket->readAll();
  QByteArray &request = requests_[socket];
  request.append(data);

  // The request can arrive in several parts, wait for the end of the headers.
  const qsizetype headers_end = request.indexOf("\r\n\r\n");
  if (headers_end < 0) {
    if (request.size() > kMaxRequestSize) {
      requests_.remove(socket);
      socket->abort();
      socket->deleteLater();
    }
    return;
  }

  const QUrl request_url = ParseUrlFromRequest(request.left(headers_end));
  requests_.remove(socket);

  if (!IsRedirect(request_url)) {
    // Not the redirect, e.g. the web browser asking for favicon.ico, or another local connection, keep waiting for the redirect.
    WriteNotFound(socket);
    socket->disconnectFromHost();
    return;
  }

  WriteTemplate(socket);
  socket->disconnectFromHost();

  request_url_ = request_url;
  success_ = true;

  // Stop listening, and close the other connections, the redirect arrived.
  close();
  const QList<QTcpSocket*> other_sockets = requests_.keys();
  requests_.clear();
  for (QTcpSocket *other_socket : other_sockets) {
    other_socket->abort();
    other_socket->deleteLater();
  }

  Q_EMIT Finished();

}

bool LocalRedirectServer::IsRedirect(const QUrl &url) {

  if (!url.isValid()) return false;

  // OAuth redirects have the code or an error (OAuthenticator), Qobuz redirects have code_autorisation (QobuzService), and Last.fm redirects have the token (LastFMScrobbler).
  // Keep this in line with the parameters the users of this server read.
  const QUrlQuery url_query(url);
  return url_query.hasQueryItem(u"code"_s) ||
         url_query.hasQueryItem(u"code_autorisation"_s) ||
         url_query.hasQueryItem(u"token"_s) ||
         url_query.hasQueryItem(u"error"_s) ||
         url_query.hasQueryItem(u"error_description"_s);

}

void LocalRedirectServer::WriteNotFound(QTcpSocket *socket) {

  socket->write("HTTP/1.0 404 Not Found\r\n");
  socket->write("Content-Length: 0\r\n");
  socket->write("\r\n");
  socket->flush();

}

void LocalRedirectServer::WriteTemplate(QTcpSocket *socket) const {

  QFile page_file(u":/html/oauthsuccess.html"_s);
  if (!page_file.open(QIODevice::ReadOnly)) return;
  QString page_data = QString::fromUtf8(page_file.readAll());
  page_file.close();

  static const QRegularExpression tr_regexp(u"tr\\(\"([^\"]+)\"\\)"_s);
  qint64 offset = 0;
  Q_FOREVER {
    QRegularExpressionMatch re_match = tr_regexp.match(page_data, offset);
    if (!re_match.hasMatch()) break;
    offset = re_match.capturedStart();
    if (offset == -1) {
      break;
    }

    const QByteArray captured_data = re_match.captured(1).toUtf8();
    page_data.replace(offset, re_match.capturedLength(), tr(captured_data.constData()));
    offset += re_match.capturedLength();
  }

  QBuffer image_buffer;
  if (image_buffer.open(QIODevice::ReadWrite)) {
    QApplication::style()->standardIcon(QStyle::SP_DialogOkButton).pixmap(16).toImage().save(&image_buffer, "PNG");
    page_data.replace("@IMAGE_DATA@"_L1, QString::fromUtf8(image_buffer.data().toBase64()));
    image_buffer.close();
  }

  socket->write("HTTP/1.0 200 OK\r\n");
  socket->write("Content-type: text/html;charset=UTF-8\r\n");
  socket->write("\r\n\r\n");
  socket->write(page_data.toUtf8());
  socket->flush();

}

QUrl LocalRedirectServer::ParseUrlFromRequest(const QByteArray &request) const {

  const QByteArrayList lines = request.split('\r');
  if (lines.isEmpty()) return QUrl();
  const QByteArray &request_line = lines[0];
  const QByteArrayList request_line_parts = request_line.split(' ');
  if (request_line_parts.size() < 2) return QUrl();
  const QByteArray path = request_line_parts[1];
  const QUrl base_url = url_;
  const QUrl request_url(base_url.toString() + QString::fromLatin1(path.mid(1)), QUrl::StrictMode);

  return request_url;

}
