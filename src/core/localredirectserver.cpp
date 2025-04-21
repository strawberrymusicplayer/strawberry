/*
 * Strawberry Music Player
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "localredirectserver.h"

#include <QApplication>
#include <QIODevice>
#include <QBuffer>
#include <QFile>
#include <QList>
#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QUrl>
#include <QRegularExpression>
#include <QStyle>
#include <QHostAddress>
#include <QTcpServer>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QDateTime>
#include <QRandomGenerator>

using namespace Qt::Literals::StringLiterals;

LocalRedirectServer::LocalRedirectServer(QObject *parent)
    : QTcpServer(parent),
      port_(0),
      socket_(nullptr),
      success_(false) {}

LocalRedirectServer::~LocalRedirectServer() {
  if (isListening()) close();
}

bool LocalRedirectServer::Listen() {

  if (!listen(QHostAddress::LocalHost, static_cast<quint64>(port_))) {
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

  if (socket_ != nullptr) {
    if (socket_->state() == QAbstractSocket::ConnectedState) socket_->close();
    socket_->deleteLater();
    socket_ = nullptr;
  }
  buffer_.clear();

  QTcpSocket *tcp_socket = new QTcpSocket(this);
  if (!tcp_socket->setSocketDescriptor(socket_descriptor)) {
    delete tcp_socket;
    close();
    success_ = false;
    error_ = "Unable to set socket descriptor"_L1;
    Q_EMIT Finished();
    return;
  }
  socket_ = tcp_socket;

  QObject::connect(socket_, &QAbstractSocket::connected, this, &LocalRedirectServer::Connected);
  QObject::connect(socket_, &QAbstractSocket::disconnected, this, &LocalRedirectServer::Disconnected);
  QObject::connect(socket_, &QAbstractSocket::readyRead, this, &LocalRedirectServer::ReadyRead);

}

void LocalRedirectServer::Encrypted() {}

void LocalRedirectServer::Connected() {}

void LocalRedirectServer::Disconnected() {}

void LocalRedirectServer::ReadyRead() {

  buffer_.append(socket_->readAll());
  if (socket_->atEnd() || buffer_.endsWith("\r\n\r\n")) {
    WriteTemplate();
    socket_->close();
    socket_->deleteLater();
    socket_ = nullptr;
    request_url_ = ParseUrlFromRequest(buffer_);
    success_ = request_url_.isValid();
    if (!request_url_.isValid()) {
      error_ = "Invalid request URL"_L1;
    }
    close();
    Q_EMIT Finished();
  }
  else {
    QObject::connect(socket_, &QAbstractSocket::readyRead, this, &LocalRedirectServer::ReadyRead);
  }

}

void LocalRedirectServer::WriteTemplate() const {

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
    QApplication::style()
        ->standardIcon(QStyle::SP_DialogOkButton)
        .pixmap(16)
        .toImage()
        .save(&image_buffer, "PNG");
    page_data.replace("@IMAGE_DATA@"_L1, QString::fromUtf8(image_buffer.data().toBase64()));
    image_buffer.close();
  }

  socket_->write("HTTP/1.0 200 OK\r\n");
  socket_->write("Content-type: text/html;charset=UTF-8\r\n");
  socket_->write("\r\n\r\n");
  socket_->write(page_data.toUtf8());
  socket_->flush();

}

QUrl LocalRedirectServer::ParseUrlFromRequest(const QByteArray &request) const {

  const QByteArrayList lines = request.split('\r');
  const QByteArray &request_line = lines[0];
  const QByteArray path = request_line.split(' ')[1];
  const QUrl base_url = url_;
  const QUrl request_url(base_url.toString() + QString::fromLatin1(path.mid(1)), QUrl::StrictMode);

  return request_url;

}
