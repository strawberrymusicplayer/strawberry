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

#include "localredirectserver.h"

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#include <QApplication>
#include <QIODevice>
#include <QBuffer>
#include <QFile>
#include <QRegExp>
#include <QStyle>
#include <QSslKey>
#include <QSslCertificate>
#include <QTcpServer>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QSslSocket>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>

#include "core/logging.h"
#include "core/closure.h"

LocalRedirectServer::LocalRedirectServer(const bool https, QObject *parent)
    : QTcpServer(parent),
      https_(https),
      socket_(nullptr)
      {}

LocalRedirectServer::~LocalRedirectServer() {
  if (isListening()) close();
}

bool LocalRedirectServer::GenerateCertificate() {

  if (int result = gnutls_global_init() != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to initialize GnuTLS: %1").arg(gnutls_strerror(result));
    return false;
  }

  gnutls_x509_privkey_t key;
  if (int result = gnutls_x509_privkey_init(&key) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to initialize the private key structure: %1").arg(gnutls_strerror(result));
    gnutls_global_deinit();
    return false;
  }

  unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_RSA, GNUTLS_SEC_PARAM_MEDIUM);

  if (int result = gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, bits, 0) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to generate random private key: %1").arg(gnutls_strerror(result));
    gnutls_global_deinit();
    return false;
  }

  char buffer[4096] = "";
  size_t buffer_size = sizeof(buffer);

  if (int result = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM, buffer, &buffer_size) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed export private key: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_global_deinit();
    return false;
  }

  QSslKey ssl_key(QByteArray(buffer, buffer_size), QSsl::Rsa);
  if (ssl_key.isNull()) {
    error_ = QString("Failed to generate random private key.");
    gnutls_x509_privkey_deinit(key);
    gnutls_global_deinit();
    return false;
  }

  gnutls_x509_crt_t crt;
  if (int result = gnutls_x509_crt_init(&crt) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to initialize an X.509 certificate structure: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_global_deinit();
    return false;
  }
  if (int result = gnutls_x509_crt_set_version(crt, 1) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set the version of the certificate: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }
  if (int result = gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COUNTRY_NAME, 0, "US", 2) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set part of the name of the certificate subject: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }
  if (int result = gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, "Strawberry Music Player", strlen("Strawberry Music Player")) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set part of the name of the certificate subject: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }
  if (int result = gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COMMON_NAME, 0, "localhost", strlen("localhost")) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set part of the name of the certificate subject: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }
  if (int result = gnutls_x509_crt_set_key(crt, key) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set the public parameters from the given private key to the certificate: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }
  quint64 time = QDateTime::currentDateTime().toTime_t();
  gnutls_x509_crt_set_activation_time(crt, time);
  if (int result = gnutls_x509_crt_set_expiration_time(crt, time + 31536000L) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set the activation time of the certificate: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }

  quint64 serial = (9999999 + qrand() % 1000000);
  QByteArray q_serial;
  q_serial.setNum(serial);

  if (int result = gnutls_x509_crt_set_serial(crt, q_serial.constData(), sizeof(q_serial.size())) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to set the serial of the certificate: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }

  gnutls_privkey_t pkey;
  if (int result = gnutls_privkey_init(&pkey) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to initialize a private key object: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_global_deinit();
    return false;
  }

  if (int result = gnutls_privkey_import_x509(pkey, key, 0) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to import the given private key to the abstract private key object: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_privkey_deinit(pkey);
    gnutls_global_deinit();
    return false;
  }

  if (int result = gnutls_x509_crt_privkey_sign(crt, crt, pkey, GNUTLS_DIG_SHA256, 0) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to sign the certificate with the issuer's private key: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_privkey_deinit(pkey);
    gnutls_global_deinit();
    return false;
  }

  if (int result = gnutls_x509_crt_sign2(crt, crt, key, GNUTLS_DIG_SHA256, 0) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to sign the certificate: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_privkey_deinit(pkey);
    gnutls_global_deinit();
    return false;
  }

  if (int result = gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_PEM, buffer, &buffer_size) != GNUTLS_E_SUCCESS) {
    error_ = QString("Failed to export the certificate to PEM format: %1").arg(gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    gnutls_x509_crt_deinit(crt);
    gnutls_privkey_deinit(pkey);
    gnutls_global_deinit();
    return false;
  }
  gnutls_x509_crt_deinit(crt);
  gnutls_x509_privkey_deinit(key);
  gnutls_privkey_deinit(pkey);

  QSslCertificate ssl_certificate(QByteArray(buffer, buffer_size));
  if (ssl_certificate.isNull()) {
    error_ = "Failed to generate random client certificate.";
    gnutls_global_deinit();
    return false;
  }

  gnutls_global_deinit();

  ssl_certificate_ = ssl_certificate;
  ssl_key_ = ssl_key;

  return true;

}

bool LocalRedirectServer::Listen() {

  if (https_) {
    if (!GenerateCertificate()) return false;
  }
  if (!listen(QHostAddress::LocalHost)) {
    error_ = errorString();
    return false;
  }

  if (https_) url_.setScheme("https");
  else url_.setScheme("http");
  url_.setHost("localhost");
  url_.setPort(serverPort());
  url_.setPath("/");
  connect(this, SIGNAL(newConnection()), this, SLOT(NewConnection()));

  return true;

}

void LocalRedirectServer::NewConnection() {

  while (hasPendingConnections()) {
    incomingConnection(nextPendingConnection()->socketDescriptor());
  }

}

void LocalRedirectServer::incomingConnection(qintptr socket_descriptor) {

  if (socket_) {
    if (socket_->state() == QAbstractSocket::ConnectedState) socket_->close();
    socket_->deleteLater();
    socket_ = nullptr;
  }
  buffer_.clear();

  if (https_) {
    QSslSocket *ssl_socket = new QSslSocket(this);
    if (!ssl_socket->setSocketDescriptor(socket_descriptor)) {
      delete ssl_socket;
      close();
      error_ = "Unable to set socket descriptor";
      emit Finished();
      return;
    }
    ssl_socket->ignoreSslErrors({QSslError::SelfSignedCertificate});
    ssl_socket->setPrivateKey(ssl_key_);
    ssl_socket->setLocalCertificate(ssl_certificate_);
    ssl_socket->setProtocol(QSsl::TlsV1_2);
    ssl_socket->startServerEncryption();

    connect(ssl_socket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(SSLErrors(QList<QSslError>)));
    connect(ssl_socket, SIGNAL(encrypted()), this, SLOT(Encrypted()));

    socket_ = ssl_socket;
  }
  else {
    QTcpSocket *tcp_socket = new QTcpSocket(this);
    if (!tcp_socket->setSocketDescriptor(socket_descriptor)) {
      delete tcp_socket;
      close();
      error_ = "Unable to set socket descriptor";
      emit Finished();
      return;
    }
    socket_ = tcp_socket;
  }

  connect(socket_, SIGNAL(connected()), this, SLOT(Connected()));
  connect(socket_, SIGNAL(disconnected()), this, SLOT(Disconnected()));
  connect(socket_, SIGNAL(readyRead()), this, SLOT(ReadyRead()));

}

void LocalRedirectServer::SSLErrors(const QList<QSslError> &errors) { Q_UNUSED(errors); }

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
    close();
    emit Finished();
  }
  else {
    connect(socket_, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
  }

}

void LocalRedirectServer::WriteTemplate() const {

  QFile page_file(":/html/oauthsuccess.html");
  page_file.open(QIODevice::ReadOnly);
  QString page_data = QString::fromUtf8(page_file.readAll());

  QRegExp tr_regexp("tr\\(\"([^\"]+)\"\\)");
  int offset = 0;
  forever {
    offset = tr_regexp.indexIn(page_data, offset);
    if (offset == -1) {
      break;
    }

    page_data.replace(offset, tr_regexp.matchedLength(), tr(tr_regexp.cap(1).toUtf8()));
    offset += tr_regexp.matchedLength();
  }

  QBuffer image_buffer;
  image_buffer.open(QIODevice::ReadWrite);
  QApplication::style()
      ->standardIcon(QStyle::SP_DialogOkButton)
      .pixmap(16)
      .toImage()
      .save(&image_buffer, "PNG");
  page_data.replace("@IMAGE_DATA@", image_buffer.data().toBase64());

  socket_->write("HTTP/1.0 200 OK\r\n");
  socket_->write("Content-type: text/html;charset=UTF-8\r\n");
  socket_->write("\r\n\r\n");
  socket_->write(page_data.toUtf8());
  socket_->flush();

}

QUrl LocalRedirectServer::ParseUrlFromRequest(const QByteArray &request) const {

  QList<QByteArray> lines = request.split('\r');
  const QByteArray &request_line = lines[0];
  QByteArray path = request_line.split(' ')[1];
  QUrl base_url = url_;
  QUrl request_url(base_url.toString() + path.mid(1), QUrl::StrictMode);
  return request_url;

}
