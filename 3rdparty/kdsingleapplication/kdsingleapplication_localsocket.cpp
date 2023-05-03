/*
  This file is part of KDSingleApplication.

  SPDX-FileCopyrightText: 2019-2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "kdsingleapplication_localsocket_p.h"

#include <QtCore/QDir>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QTimer>
#include <QtCore/QLockFile>
#include <QtCore/QDataStream>

#include <QtCore/QtDebug>
#include <QtCore/QLoggingCategory>

#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include <chrono>
#include <algorithm>

#if defined(Q_OS_UNIX)
// for ::getuid()
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

static const auto LOCALSOCKET_CONNECTION_TIMEOUT = std::chrono::seconds(5);
static const char LOCALSOCKET_PROTOCOL_VERSION = 2;

Q_LOGGING_CATEGORY(kdsaLocalSocket, "kdsingleapplication.localsocket", QtWarningMsg);

KDSingleApplicationLocalSocket::KDSingleApplicationLocalSocket(const QString &name, QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_UNIX)
    /* cppcheck-suppress useInitializationList */
    m_socketName = QStringLiteral("kdsingleapp-%1-%2-%3")
                       .arg(::getuid())
                       .arg(qEnvironmentVariable("XDG_SESSION_ID"), name);
#elif defined(Q_OS_WIN)
    // I'm not sure of a "global session identifier" on Windows; are
    // multiple logins from the same user a possibility? For now, following this:
    // https://docs.microsoft.com/en-us/windows/desktop/devnotes/getting-the-session-id-of-the-current-process

    DWORD sessionId;
    BOOL haveSessionId = ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

    m_socketName = QString::fromUtf8("kdsingleapp-%1-%2")
                       .arg(haveSessionId ? sessionId : 0)
                       .arg(name);
#else
#error "KDSingleApplication has not been ported to this platform"
#endif

    const QString lockFilePath =
        QDir::tempPath() + QLatin1Char('/') + m_socketName + QLatin1String(".lock");

    qCDebug(kdsaLocalSocket) << "Socket name is" << m_socketName;
    qCDebug(kdsaLocalSocket) << "Lock file path is" << lockFilePath;

    std::unique_ptr<QLockFile> lockFile(new QLockFile(lockFilePath));
    lockFile->setStaleLockTime(0);

    if (!lockFile->tryLock()) {
        // someone else has the lock => we're secondary
        qCDebug(kdsaLocalSocket) << "Secondary instance";
        return;
    }

    qCDebug(kdsaLocalSocket) << "Primary instance";

    std::unique_ptr<QLocalServer> server = std::make_unique<QLocalServer>();
    if (!server->listen(m_socketName)) {
        // maybe the primary crashed, leaving a stale socket; delete it and try again
        QLocalServer::removeServer(m_socketName);
        if (!server->listen(m_socketName)) {
            // TODO: better error handling.
            qWarning("KDSingleApplication: unable to make the primary instance listen on %ls: %ls",
                     qUtf16Printable(m_socketName),
                     qUtf16Printable(server->errorString()));

            return;
        }
    }

    connect(server.get(), &QLocalServer::newConnection,
            this, &KDSingleApplicationLocalSocket::handleNewConnection);

    m_lockFile = std::move(lockFile);
    m_localServer = std::move(server);
}

KDSingleApplicationLocalSocket::~KDSingleApplicationLocalSocket() = default;

bool KDSingleApplicationLocalSocket::isPrimaryInstance() const
{
    return m_localServer != nullptr;
}

bool KDSingleApplicationLocalSocket::sendMessage(const QByteArray &message, int timeout)
{
    Q_ASSERT(!isPrimaryInstance());
    QLocalSocket socket;

    qCDebug(kdsaLocalSocket) << "Preparing to send message" << message << "with timeout" << timeout;

    QDeadlineTimer deadline(timeout);

    // There is an inherent race here with the setup of the server side.
    // Even if the socket lock is held by the server, the server may not
    // be listening yet. So this connection may fail; keep retrying
    // until we hit the timeout.
    do {
        socket.connectToServer(m_socketName);
        if (socket.waitForConnected(deadline.remainingTime()))
            break;
    } while (!deadline.hasExpired());

    qCDebug(kdsaLocalSocket) << "Socket state:" << socket.state() << "Timer remaining" << deadline.remainingTime() << "Expired?" << deadline.hasExpired();

    if (deadline.hasExpired()) {
        qCWarning(kdsaLocalSocket) << "Connection timed out";
        return false;
    }

    socket.write(&LOCALSOCKET_PROTOCOL_VERSION, 1);

    {
        QByteArray encodedMessage;
        QDataStream ds(&encodedMessage, QIODevice::WriteOnly);
        ds << message;
        socket.write(encodedMessage);
    }

    qCDebug(kdsaLocalSocket) << "Wrote message in the socket"
                             << "Timer remaining" << deadline.remainingTime() << "Expired?" << deadline.hasExpired();

    // There is no acknowledgement mechanism here.
    // Should there be one?

    while (socket.bytesToWrite() > 0) {
        if (!socket.waitForBytesWritten(deadline.remainingTime())) {
            qCWarning(kdsaLocalSocket) << "Message to primary timed out";
            return false;
        }
    }

    qCDebug(kdsaLocalSocket) << "Bytes written, now disconnecting"
                             << "Timer remaining" << deadline.remainingTime() << "Expired?" << deadline.hasExpired();

    socket.disconnectFromServer();

    if (socket.state() == QLocalSocket::UnconnectedState) {
        qCDebug(kdsaLocalSocket) << "Disconnected -- success!";
        return true;
    }

    if (!socket.waitForDisconnected(deadline.remainingTime())) {
        qCWarning(kdsaLocalSocket) << "Disconnection from primary timed out";
        return false;
    }

    qCDebug(kdsaLocalSocket) << "Disconnected -- success!";

    return true;
}

void KDSingleApplicationLocalSocket::handleNewConnection()
{
    Q_ASSERT(m_localServer);

    QLocalSocket *socket;
    while ((socket = m_localServer->nextPendingConnection())) {
        qCDebug(kdsaLocalSocket) << "Got new connection on" << m_socketName << "state" << socket->state();

        Connection c(std::move(socket));
        socket = c.socket.get();

        c.readDataConnection = QObjectConnectionHolder(
            connect(socket, &QLocalSocket::readyRead,
                    this, &KDSingleApplicationLocalSocket::readDataFromSecondary));

        c.secondaryDisconnectedConnection = QObjectConnectionHolder(
            connect(socket, &QLocalSocket::disconnected,
                    this, &KDSingleApplicationLocalSocket::secondaryDisconnected));

        c.abortConnection = QObjectConnectionHolder(
            connect(c.timeoutTimer.get(), &QTimer::timeout,
                    this, &KDSingleApplicationLocalSocket::abortConnectionToSecondary));

        m_clients.push_back(std::move(c));

        // Note that by the time we get here, the socket could've already been closed,
        // and no signals emitted (hello, Windows!). Read what's already in the socket.
        if (readDataFromSecondarySocket(socket))
            return;

        if (socket->state() == QLocalSocket::UnconnectedState)
            secondarySocketDisconnected(socket);
    }
}

template<typename Container>
static auto findConnectionBySocket(Container &container, QLocalSocket *socket)
{
    auto i = std::find_if(container.begin(),
                          container.end(),
                          [socket](const auto &c) { return c.socket.get() == socket; });
    Q_ASSERT(i != container.end());
    return i;
}

template<typename Container>
static auto findConnectionByTimer(Container &container, QTimer *timer)
{
    auto i = std::find_if(container.begin(),
                          container.end(),
                          [timer](const auto &c) { return c.timeoutTimer.get() == timer; });
    Q_ASSERT(i != container.end());
    return i;
}

void KDSingleApplicationLocalSocket::readDataFromSecondary()
{
    QLocalSocket *socket = static_cast<QLocalSocket *>(sender());
    readDataFromSecondarySocket(socket);
}

bool KDSingleApplicationLocalSocket::readDataFromSecondarySocket(QLocalSocket *socket)
{
    auto i = findConnectionBySocket(m_clients, socket);
    Connection &c = *i;
    c.readData.append(socket->readAll());

    qCDebug(kdsaLocalSocket) << "Got more data from a secondary. Data read so far:" << c.readData;

    const QByteArray &data = c.readData;

    if (data.size() >= 1) {
        if (data[0] != LOCALSOCKET_PROTOCOL_VERSION) {
            qCDebug(kdsaLocalSocket) << "Got an invalid protocol version";
            m_clients.erase(i);
            return true;
        }
    }

    QDataStream ds(data);
    ds.skipRawData(1);

    ds.startTransaction();
    QByteArray message;
    ds >> message;

    if (ds.commitTransaction()) {
        qCDebug(kdsaLocalSocket) << "Got a complete message:" << message;
        Q_EMIT messageReceived(message);
        m_clients.erase(i);
        return true;
    }

    return false;
}

void KDSingleApplicationLocalSocket::secondaryDisconnected()
{
    QLocalSocket *socket = static_cast<QLocalSocket *>(sender());
    secondarySocketDisconnected(socket);
}

void KDSingleApplicationLocalSocket::secondarySocketDisconnected(QLocalSocket *socket)
{
    auto i = findConnectionBySocket(m_clients, socket);
    Connection c = std::move(*i);
    m_clients.erase(i);

    qCDebug(kdsaLocalSocket) << "Secondary disconnected. Data read:" << c.readData;
}

void KDSingleApplicationLocalSocket::abortConnectionToSecondary()
{
    QTimer *timer = static_cast<QTimer *>(sender());

    auto i = findConnectionByTimer(m_clients, timer);
    Connection c = std::move(*i);
    m_clients.erase(i);

    qCDebug(kdsaLocalSocket) << "Secondary timed out. Data read:" << c.readData;
}

KDSingleApplicationLocalSocket::Connection::Connection(QLocalSocket *_socket)
    : socket(_socket)
    , timeoutTimer(new QTimer)
{
    timeoutTimer->start(LOCALSOCKET_CONNECTION_TIMEOUT);
}
