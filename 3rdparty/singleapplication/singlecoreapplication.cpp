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

#include <stdlib.h>
#include <limits>

#include <QtGlobal>
#include <QCoreApplication>
#include <QThread>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QtDebug>

#include "singlecoreapplication.h"
#include "singlecoreapplication_p.h"

/**
 * @brief Constructor. Checks and fires up LocalServer or closes the program
 * if another instance already exists
 * @param argc
 * @param argv
 * @param {bool} allowSecondaryInstances
 */
SingleCoreApplication::SingleCoreApplication(int &argc, char *argv[], bool allowSecondary, Options options, int timeout)
  : app_t(argc, argv), d_ptr(new SingleCoreApplicationPrivate(this)) {

  Q_D(SingleCoreApplication);

  // Store the current mode of the program
  d->options = options;

  // Generating an application ID used for identifying the shared memory
  // block and QLocalServer
  d->genBlockServerName();

#ifdef Q_OS_UNIX
  // By explicitly attaching it and then deleting it we make sure that the
  // memory is deleted even after the process has crashed on Unix.
  d->memory = new QSharedMemory(d->blockServerName);
  d->memory->attach();
  delete d->memory;
#endif
  // Guarantee thread safe behaviour with a shared memory block.
  d->memory = new QSharedMemory(d->blockServerName);

  // Create a shared memory block
  if (d->memory->create(sizeof(InstancesInfo))) {
    // Initialize the shared memory block
    d->memory->lock();
    d->initializeMemoryBlock();
    d->memory->unlock();
  }
  else {
    // Attempt to attach to the memory segment
    if (!d->memory->attach()) {
      qCritical() << "SingleCoreApplication: Unable to attach to shared memory block.";
      qCritical() << d->memory->errorString();
      delete d;
      ::exit(EXIT_FAILURE);
    }
  }

  InstancesInfo* inst = static_cast<InstancesInfo*>(d->memory->data());
  QElapsedTimer time;
  time.start();

  // Make sure the shared memory block is initialised and in consistent state
  while (true) {
    d->memory->lock();

    if(d->blockChecksum() == inst->checksum) break;

    if (time.elapsed() > 5000) {
      qWarning() << "SingleCoreApplication: Shared memory block has been in an inconsistent state from more than 5s. Assuming primary instance failure.";
      d->initializeMemoryBlock();
    }

    d->memory->unlock();

    // Random sleep here limits the probability of a collision between two racing apps
    qsrand(QDateTime::currentMSecsSinceEpoch() % std::numeric_limits<uint>::max());
    QThread::sleep(8 + static_cast <unsigned long>(static_cast <float>(qrand()) / RAND_MAX * 10));
  }

  if (inst->primary == false) {
    d->startPrimary();
    d->memory->unlock();
    return;
  }

  // Check if another instance can be started
  if (allowSecondary) {
    inst->secondary += 1;
    inst->checksum = d->blockChecksum();
    d->instanceNumber = inst->secondary;
    d->startSecondary();
    if(d->options & Mode::SecondaryNotification) {
      d->connectToPrimary(timeout, SingleCoreApplicationPrivate::SecondaryInstance);
    }
    d->memory->unlock();
    return;
  }

  d->memory->unlock();

  d->connectToPrimary(timeout, SingleCoreApplicationPrivate::NewInstance);

  delete d;

  ::exit(EXIT_SUCCESS);

}

/**
 * @brief Destructor
 */
SingleCoreApplication::~SingleCoreApplication() {
  Q_D(SingleCoreApplication);
  delete d;
}

bool SingleCoreApplication::isPrimary() {
  Q_D(SingleCoreApplication);
  return d->server != nullptr;
}

bool SingleCoreApplication::isSecondary() {
  Q_D(SingleCoreApplication);
  return d->server == nullptr;
}

quint32 SingleCoreApplication::instanceId() {
  Q_D(SingleCoreApplication);
  return d->instanceNumber;
}

qint64 SingleCoreApplication::primaryPid() {
  Q_D(SingleCoreApplication);
  return d->primaryPid();
}

bool SingleCoreApplication::sendMessage(QByteArray message, int timeout) {

  Q_D(SingleCoreApplication);

  // Nobody to connect to
  if(isPrimary()) return false;

  // Make sure the socket is connected
  d->connectToPrimary(timeout, SingleCoreApplicationPrivate::Reconnect);

  d->socket->write(message);
  bool dataWritten = d->socket->waitForBytesWritten(timeout);
  d->socket->flush();
  return dataWritten;

}
