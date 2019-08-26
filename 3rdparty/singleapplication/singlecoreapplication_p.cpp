// The MIT License (MIT)
//
// Copyright (c) Itay Grudev 2015 - 2018
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//
//  W A R N I N G !!!
//  -----------------
//
// This is a modified version of SingleApplication,
// The original version is at:
//
// https://github.com/itay-grudev/SingleApplication
//
//

#include "config.h"

#include <QtGlobal>

#include <cstdlib>
#include <cstddef>

#ifdef Q_OS_UNIX
#  include <unistd.h>
#  include <sys/types.h>
#  include <pwd.h>
#endif

#include <QDir>
#include <QByteArray>
#include <QDataStream>
#include <QCryptographicHash>
#include <QLocalServer>
#include <QLocalSocket>

#include "singlecoreapplication.h"
#include "singlecoreapplication_p.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <lmcons.h>
#endif

SingleCoreApplicationPrivate::SingleCoreApplicationPrivate(SingleCoreApplication *q_ptr)
  : q_ptr(q_ptr),
    memory(nullptr),
    socket(nullptr),
    server(nullptr),
    instanceNumber(-1)
    {}

SingleCoreApplicationPrivate::~SingleCoreApplicationPrivate() {

  if (socket != nullptr) {
    socket->close();
    delete socket;
  }

  memory->lock();
  InstancesInfo* inst = static_cast<InstancesInfo*>(memory->data());
  if (server != nullptr) {
    server->close();
    delete server;
    inst->primary = false;
    inst->primaryPid = -1;
    inst->checksum = blockChecksum();
  }
  memory->unlock();

  delete memory;

}

void SingleCoreApplicationPrivate::genBlockServerName() {

  QCryptographicHash appData(QCryptographicHash::Sha256);
  appData.addData("SingleApplication", 17);
  appData.addData(SingleCoreApplication::app_t::applicationName().toUtf8());
  appData.addData(SingleCoreApplication::app_t::organizationName().toUtf8());
  appData.addData(SingleCoreApplication::app_t::organizationDomain().toUtf8());

  if (!(options & SingleCoreApplication::Mode::ExcludeAppVersion)) {
    appData.addData(SingleCoreApplication::app_t::applicationVersion().toUtf8());
  }

  if (!(options & SingleCoreApplication::Mode::ExcludeAppPath)) {
#ifdef Q_OS_WIN
    appData.addData(SingleCoreApplication::app_t::applicationFilePath().toLower().toUtf8());
#else
    appData.addData(SingleCoreApplication::app_t::applicationFilePath().toUtf8());
#endif
  }

  // User level block requires a user specific data in the hash
  if (options & SingleCoreApplication::Mode::User) {
#ifdef Q_OS_UNIX
    QByteArray username;
#if defined(HAVE_GETEUID) && defined(HAVE_GETPWUID)
    uid_t uid = geteuid();
    if (uid != -1) {
      struct passwd *pw = getpwuid(uid);
      if (pw) {
        username = pw->pw_name;
      }
    }
#endif
    if (username.isEmpty()) username = qgetenv("USER");
    appData.addData(username);
#endif
#ifdef Q_OS_WIN
    wchar_t username [ UNLEN + 1 ];
    // Specifies size of the buffer on input
    DWORD usernameLength = UNLEN + 1;
    if (GetUserNameW(username, &usernameLength)) {
      appData.addData(QString::fromWCharArray(username).toUtf8());
    }
    else {
      appData.addData(qgetenv("USERNAME"));
    }
#endif
  }

  // Replace the backslash in RFC 2045 Base64 [a-zA-Z0-9+/=] to comply with server naming requirements.
  blockServerName = appData.result().toBase64().replace("/", "_");

}

void SingleCoreApplicationPrivate::initializeMemoryBlock() {

  InstancesInfo* inst = static_cast<InstancesInfo*>(memory->data());
  inst->primary = false;
  inst->secondary = 0;
  inst->primaryPid = -1;
  inst->checksum = blockChecksum();

}

void SingleCoreApplicationPrivate::startPrimary() {

  Q_Q(SingleCoreApplication);

  // Successful creation means that no main process exists
  // So we start a QLocalServer to listen for connections
  QLocalServer::removeServer(blockServerName);
  server = new QLocalServer();

  // Restrict access to the socket according to the
  // SingleCoreApplication::Mode::User flag on User level or no restrictions
  if (options & SingleCoreApplication::Mode::User) {
    server->setSocketOptions(QLocalServer::UserAccessOption);
  }
  else {
    server->setSocketOptions(QLocalServer::WorldAccessOption);
  }

  server->listen(blockServerName);
  QObject::connect(server, &QLocalServer::newConnection, this, &SingleCoreApplicationPrivate::slotConnectionEstablished);

  // Reset the number of connections
  InstancesInfo* inst = static_cast <InstancesInfo*>(memory->data());

  inst->primary = true;
  inst->primaryPid = q->applicationPid();
  inst->checksum = blockChecksum();

  instanceNumber = 0;

}

void SingleCoreApplicationPrivate::startSecondary() {}

void SingleCoreApplicationPrivate::connectToPrimary(int msecs, ConnectionType connectionType) {

  // Connect to the Local Server of the Primary Instance if not already connected.
  if (socket == nullptr) {
    socket = new QLocalSocket();
  }

  // If already connected - we are done;
  if (socket->state() == QLocalSocket::ConnectedState)
    return;

  // If not connect
  if (socket->state() == QLocalSocket::UnconnectedState ||
    socket->state() == QLocalSocket::ClosingState) {
    socket->connectToServer(blockServerName);
  }

  // Wait for being connected
  if (socket->state() == QLocalSocket::ConnectingState) {
    socket->waitForConnected(msecs);
  }

  // Initialisation message according to the SingleCoreApplication protocol
  if (socket->state() == QLocalSocket::ConnectedState) {
    // Notify the parent that a new instance had been started;
    QByteArray initMsg;
    QDataStream writeStream(&initMsg, QIODevice::WriteOnly);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    writeStream.setVersion(QDataStream::Qt_5_6);
#endif

    writeStream << blockServerName.toLatin1();
    writeStream << static_cast<quint8>(connectionType);
    writeStream << instanceNumber;
    quint16 checksum = qChecksum(initMsg.constData(), static_cast<quint32>(initMsg.length()));
    writeStream << checksum;

    // The header indicates the message length that follows
    QByteArray header;
    QDataStream headerStream(&header, QIODevice::WriteOnly);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    headerStream.setVersion(QDataStream::Qt_5_6);
#endif
    headerStream << static_cast <quint64>(initMsg.length());

    socket->write(header);
    socket->write(initMsg);
    socket->flush();
    socket->waitForBytesWritten(msecs);
  }

}

quint16 SingleCoreApplicationPrivate::blockChecksum() {

  return qChecksum(static_cast <const char*> (memory->data()), offsetof(InstancesInfo, checksum));

}

qint64 SingleCoreApplicationPrivate::primaryPid() {

  qint64 pid;

  memory->lock();
  InstancesInfo* inst = static_cast<InstancesInfo*>(memory->data());
  pid = inst->primaryPid;
  memory->unlock();

  return pid;

}

/**
 * @brief Executed when a connection has been made to the LocalServer
 */
void SingleCoreApplicationPrivate::slotConnectionEstablished() {

  QLocalSocket *nextConnSocket = server->nextPendingConnection();
  connectionMap.insert(nextConnSocket, ConnectionInfo());

  QObject::connect(nextConnSocket, &QLocalSocket::aboutToClose,
    [nextConnSocket, this]() {
      auto &info = connectionMap[nextConnSocket];
      Q_EMIT this->slotClientConnectionClosed(nextConnSocket, info.instanceId);
    }
  );

  QObject::connect(nextConnSocket, &QLocalSocket::disconnected,
    [nextConnSocket, this](){
      connectionMap.remove(nextConnSocket);
      nextConnSocket->deleteLater();
    }
  );

  QObject::connect(nextConnSocket, &QLocalSocket::readyRead,
    [nextConnSocket, this]() {
      auto &info = connectionMap[nextConnSocket];
      switch(info.stage) {
      case StageHeader:
        readInitMessageHeader(nextConnSocket);
        break;
      case StageBody:
        readInitMessageBody(nextConnSocket);
        break;
      case StageConnected:
        Q_EMIT this->slotDataAvailable(nextConnSocket, info.instanceId);
        break;
      default:
        break;
      };
    }
  );

}

void SingleCoreApplicationPrivate::readInitMessageHeader(QLocalSocket *sock) {

  if (!connectionMap.contains(sock)) {
    return;
  }

  if (sock->bytesAvailable() < (qint64)sizeof(quint64)) {
    return;
  }

  QDataStream headerStream(sock);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  headerStream.setVersion(QDataStream::Qt_5_6);
#endif

  // Read the header to know the message length
  quint64 msgLen = 0;
  headerStream >> msgLen;
  ConnectionInfo &info = connectionMap[sock];
  info.stage = StageBody;
  info.msgLen = msgLen;

  if (sock->bytesAvailable() >= (qint64) msgLen) {
    readInitMessageBody(sock);
  }

}

void SingleCoreApplicationPrivate::readInitMessageBody(QLocalSocket *sock) {

  Q_Q(SingleCoreApplication);

  if (!connectionMap.contains(sock)) {
    return;
  }

  ConnectionInfo &info = connectionMap[sock];
  if (sock->bytesAvailable() < (qint64)info.msgLen) {
    return;
  }

  // Read the message body
  QByteArray msgBytes = sock->read(info.msgLen);
  QDataStream readStream(msgBytes);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  readStream.setVersion(QDataStream::Qt_5_6);
#endif

  // server name
  QByteArray latin1Name;
  readStream >> latin1Name;

  // connection type
  ConnectionType connectionType = InvalidConnection;
  quint8 connTypeVal = InvalidConnection;
  readStream >> connTypeVal;
  connectionType = static_cast <ConnectionType>(connTypeVal);

  // instance id
  quint32 instanceId = 0;
  readStream >> instanceId;

  // checksum
  quint16 msgChecksum = 0;
  readStream >> msgChecksum;

  const quint16 actualChecksum = qChecksum(msgBytes.constData(), static_cast<quint32>(msgBytes.length() - sizeof(quint16)));

  bool isValid = readStream.status() == QDataStream::Ok && QLatin1String(latin1Name) == blockServerName && msgChecksum == actualChecksum;

  if (!isValid) {
    sock->close();
    return;
  }

  info.instanceId = instanceId;
  info.stage = StageConnected;

  if (connectionType == NewInstance || (connectionType == SecondaryInstance && options & SingleCoreApplication::Mode::SecondaryNotification)) {
    Q_EMIT q->instanceStarted();
  }

  if (sock->bytesAvailable() > 0) {
    Q_EMIT this->slotDataAvailable(sock, instanceId);
  }

}

void SingleCoreApplicationPrivate::slotDataAvailable(QLocalSocket *dataSocket, quint32 instanceId) {

  Q_Q(SingleCoreApplication);
  Q_EMIT q->receivedMessage(instanceId, dataSocket->readAll());

}

void SingleCoreApplicationPrivate::slotClientConnectionClosed(QLocalSocket *closedSocket, quint32 instanceId) {

  if (closedSocket->bytesAvailable() > 0)
    Q_EMIT slotDataAvailable(closedSocket, instanceId);

}
