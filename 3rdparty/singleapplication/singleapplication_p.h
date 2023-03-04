// The MIT License (MIT)
//
// Copyright (c) Itay Grudev 2015 - 2020
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

#ifndef SINGLEAPPLICATION_P_H
#define SINGLEAPPLICATION_P_H

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QHash>

#include "singleapplication_t.h"

class QLocalServer;
class QLocalSocket;
class QSharedMemory;

class SingleApplicationPrivateClass : public QObject {
  Q_OBJECT

 public:
  explicit SingleApplicationPrivateClass(SingleApplicationClass *ptr);
  ~SingleApplicationPrivateClass() override;

  enum ConnectionType : quint8 {
    InvalidConnection = 0,
    NewInstance = 1,
    SecondaryInstance = 2,
    Reconnect = 3
  };
  enum ConnectionStage : quint8 {
    StageInitHeader = 0,
    StageInitBody = 1,
    StageConnectedHeader = 2,
    StageConnectedBody = 3
  };
  Q_DECLARE_PUBLIC(SingleApplicationClass)

  struct InstancesInfo {
    explicit InstancesInfo() : primary(false), secondary(0), primaryPid(0), checksum(0) {}
    bool primary;
    quint32 secondary;
    qint64 primaryPid;
    char primaryUser[128];
    quint16 checksum;
  };

  struct ConnectionInfo {
    explicit ConnectionInfo() : msgLen(0), instanceId(0), stage(0) {}
    quint64 msgLen;
    quint32 instanceId;
    quint8 stage;
  };

  static QString getUsername();
  void genBlockServerName();
  void initializeMemoryBlock() const;
  void startPrimary();
  void startSecondary();
  bool connectToPrimary(const int timeout, const ConnectionType connectionType);
  quint16 blockChecksum() const;
  qint64 primaryPid() const;
  QString primaryUser() const;
  bool isFrameComplete(QLocalSocket *sock);
  void readMessageHeader(QLocalSocket *socket, const ConnectionStage nextStage);
  void readInitMessageBody(QLocalSocket *socket);
  void writeAck(QLocalSocket *sock);
  bool writeConfirmedFrame(const int timeout, const QByteArray &msg) const;
  bool writeConfirmedMessage(const int timeout, const QByteArray &msg) const;
  static void randomSleep();

  SingleApplicationClass *q_ptr;
  QSharedMemory *memory_;
  QLocalSocket *socket_;
  QLocalServer *server_;
  quint32 instanceNumber_;
  QString blockServerName_;
  SingleApplicationClass::Options options_;
  QHash<QLocalSocket*, ConnectionInfo> connectionMap_;

 public slots:
  void slotConnectionEstablished();
  void slotDataAvailable(QLocalSocket*, const quint32);
  void slotClientConnectionClosed(QLocalSocket*, const quint32);
};

#endif  // SINGLEAPPLICATION_P_H
