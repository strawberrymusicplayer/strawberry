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

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

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

#include "core/logging.h"
#include "core/closure.h"

LocalRedirectServer::LocalRedirectServer(const bool https, QObject *parent)
    : QTcpServer(parent),
      https_(https),
      socket_(nullptr)
      {}

LocalRedirectServer::~LocalRedirectServer() {}

bool LocalRedirectServer::GenerateCertificate() {

  EVP_PKEY *pkey = EVP_PKEY_new();
  q_check_ptr(pkey);

  RSA *rsa = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);
  q_check_ptr(rsa);

  EVP_PKEY_assign_RSA(pkey, rsa);

  X509 *x509 = X509_new();
  q_check_ptr(x509);

  ASN1_INTEGER_set(X509_get_serialNumber(x509), static_cast<uint64_t>(9999999 + qrand() % 1000000));

  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
  X509_set_pubkey(x509, pkey);

  X509_NAME *name = X509_get_subject_name(x509);
  q_check_ptr(name);

  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *) "US", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *) "Strawberry Music Player", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *) "localhost", -1, -1, 0);
  X509_set_issuer_name(x509, name);
  X509_sign(x509, pkey, EVP_sha1());

  BIO *bp_private = BIO_new(BIO_s_mem());
  q_check_ptr(bp_private);

  if (PEM_write_bio_PrivateKey(bp_private, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
    EVP_PKEY_free(pkey);
    X509_free(x509);
    BIO_free_all(bp_private);
    error_ = "PEM_write_bio_PrivateKey() failed.";
    return false;
  }

  BIO *bp_public = BIO_new(BIO_s_mem());
  q_check_ptr(bp_public);

  if (PEM_write_bio_X509(bp_public, x509) != 1) {
    EVP_PKEY_free(pkey);
    X509_free(x509);
    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    error_ = "PEM_write_bio_X509() failed.";
    return false;
  }

  const char *buffer;

  long size = BIO_get_mem_data(bp_public, &buffer);
  q_check_ptr(buffer);

  QSslCertificate ssl_certificate(QByteArray(buffer, size));
  if (ssl_certificate.isNull()) {
    error_ = "Failed to generate a random client certificate.";
    return false;
  }

  size = BIO_get_mem_data(bp_private, &buffer);
  q_check_ptr(buffer);

  QSslKey ssl_key(QByteArray(buffer, size), QSsl::Rsa);
  if (ssl_key.isNull()) {
    error_ = "Failed to generate a random private key.";
    return false;
  }

  EVP_PKEY_free(pkey);
  X509_free(x509);
  BIO_free_all(bp_public);
  BIO_free_all(bp_private);

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
    connect(ssl_socket, SIGNAL(encrypted()), this, SLOT(Encrypted(QSslSocket*)));

    socket_ = ssl_socket;
  }
  else {
    QTcpSocket *tcp_socket = new QTcpSocket(this);
    if (!tcp_socket->setSocketDescriptor(socket_descriptor)) {
      delete tcp_socket;
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

void LocalRedirectServer::SSLErrors(const QList<QSslError> &errors) {}

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
