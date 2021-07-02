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

#include <cstdlib>
#include <limits>

#include <QtGlobal>
#include <QCoreApplication>
#include <QThread>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QtDebug>

#include "singlecoreapplication.h"
#include "singlecoreapplication_p.h"

/**
 * @brief Constructor. Checks and fires up LocalServer or closes the program if another instance already exists
 * @param argc
 * @param argv
 * @param allowSecondary Whether to enable secondary instance support
 * @param options Optional flags to toggle specific behaviour
 * @param timeout Maximum time blocking functions are allowed during app load
 */
SingleCoreApplication::SingleCoreApplication(int &argc, char *argv[], bool allowSecondary, Options options, int timeout)
    : app_t(argc, argv),
      d_ptr(new SingleCoreApplicationPrivate(this)) {

  Q_D(SingleCoreApplication);

  // Store the current mode of the program
  d->options_ = options;

  // Generating an application ID used for identifying the shared memory block and QLocalServer
  d->genBlockServerName();

  // To mitigate QSharedMemory issues with large amount of processes attempting to attach at the same time
  d->randomSleep();

#ifdef Q_OS_UNIX
  // By explicitly attaching it and then deleting it we make sure that the memory is deleted even after the process has crashed on Unix.
  d->memory_ = new QSharedMemory(d->blockServerName_);
  d->memory_->attach();
  delete d->memory_;
#endif

  // Guarantee thread safe behaviour with a shared memory block.
  d->memory_ = new QSharedMemory(d->blockServerName_);

  // Create a shared memory block
  if (d->memory_->create(sizeof(InstancesInfo))) {
    // Initialize the shared memory block
    if (!d->memory_->lock()) {
      qCritical() << "SingleCoreApplication: Unable to lock memory block after create.";
      abortSafely();
    }
    d->initializeMemoryBlock();
  }
  else {
    if (d->memory_->error() == QSharedMemory::AlreadyExists) {
      // Attempt to attach to the memory segment
      if (!d->memory_->attach()) {
        qCritical() << "SingleCoreApplication: Unable to attach to shared memory block.";
        abortSafely();
      }
      if (!d->memory_->lock()) {
        qCritical() << "SingleCoreApplication: Unable to lock memory block after attach.";
        abortSafely();
      }
    }
    else {
      qCritical() << "SingleCoreApplication: Unable to create block.";
      abortSafely();
    }
  }

  InstancesInfo *inst = static_cast<InstancesInfo*>(d->memory_->data());
  QElapsedTimer time;
  time.start();

  // Make sure the shared memory block is initialised and in consistent state
  forever {
    // If the shared memory block's checksum is valid continue
    if (d->blockChecksum() == inst->checksum) break;

    // If more than 5s have elapsed, assume the primary instance crashed and assume it's position
    if (time.elapsed() > 5000) {
      qWarning() << "SingleCoreApplication: Shared memory block has been in an inconsistent state from more than 5s. Assuming primary instance failure.";
      d->initializeMemoryBlock();
    }

    // Otherwise wait for a random period and try again.
    // The random sleep here limits the probability of a collision between two racing apps and allows the app to initialise faster
    if (!d->memory_->unlock()) {
      qDebug() << "SingleCoreApplication: Unable to unlock memory for random wait.";
      qDebug() << d->memory_->errorString();
    }
    d->randomSleep();
    if (!d->memory_->lock()) {
      qCritical() << "SingleCoreApplication: Unable to lock memory after random wait.";
      abortSafely();
    }
  }

  if (!inst->primary) {
    d->startPrimary();
    if (!d->memory_->unlock()) {
      qDebug() << "SingleCoreApplication: Unable to unlock memory after primary start.";
      qDebug() << d->memory_->errorString();
    }
    return;
  }

  // Check if another instance can be started
  if (allowSecondary) {
    d->startSecondary();
    if (d->options_ & Mode::SecondaryNotification) {
      d->connectToPrimary(timeout, SingleCoreApplicationPrivate::SecondaryInstance);
    }
    if (!d->memory_->unlock()) {
      qDebug() << "SingleCoreApplication: Unable to unlock memory after secondary start.";
      qDebug() << d->memory_->errorString();
    }
    return;
  }

  if (!d->memory_->unlock()) {
    qDebug() << "SingleCoreApplication: Unable to unlock memory at end of execution.";
    qDebug() << d->memory_->errorString();
  }

  d->connectToPrimary(timeout, SingleCoreApplicationPrivate::NewInstance);

  delete d;

  ::exit(EXIT_SUCCESS);

}

SingleCoreApplication::~SingleCoreApplication() {
  Q_D(SingleCoreApplication);
  delete d;
}

/**
 * Checks if the current application instance is primary.
 * @return Returns true if the instance is primary, false otherwise.
 */
bool SingleCoreApplication::isPrimary() {
  Q_D(SingleCoreApplication);
  return d->server_ != nullptr;
}

/**
 * Checks if the current application instance is secondary.
 * @return Returns true if the instance is secondary, false otherwise.
 */
bool SingleCoreApplication::isSecondary() {
  Q_D(SingleCoreApplication);
  return d->server_ == nullptr;
}

/**
 * Allows you to identify an instance by returning unique consecutive instance ids.
 * It is reset when the first (primary) instance of your app starts and only incremented afterwards.
 * @return Returns a unique instance id.
 */
quint32 SingleCoreApplication::instanceId() {
  Q_D(SingleCoreApplication);
  return d->instanceNumber_;
}

/**
 * Returns the OS PID (Process Identifier) of the process running the primary instance.
 * Especially useful when SingleCoreApplication is coupled with OS. specific APIs.
 * @return Returns the primary instance PID.
 */
qint64 SingleCoreApplication::primaryPid() {
  Q_D(SingleCoreApplication);
  return d->primaryPid();
}

/**
 * Returns the username the primary instance is running as.
 * @return Returns the username the primary instance is running as.
 */
QString SingleCoreApplication::primaryUser() {
  Q_D(SingleCoreApplication);
  return d->primaryUser();
}

/**
 * Returns the username the current instance is running as.
 * @return Returns the username the current instance is running as.
 */
QString SingleCoreApplication::currentUser() {
  Q_D(SingleCoreApplication);
  return d->getUsername();
}

/**
 * Sends message to the Primary Instance.
 * @param message The message to send.
 * @param timeout the maximum timeout in milliseconds for blocking functions.
 * @return true if the message was sent successfully, false otherwise.
 */
bool SingleCoreApplication::sendMessage(const QByteArray &message, const int timeout) {

  Q_D(SingleCoreApplication);

  // Nobody to connect to
  if (isPrimary()) return false;

  // Make sure the socket is connected
  if (!d->connectToPrimary(timeout, SingleCoreApplicationPrivate::Reconnect))
    return false;

  d->socket_->write(message);
  const bool dataWritten = d->socket_->waitForBytesWritten(timeout);
  d->socket_->flush();
  return dataWritten;

}

/**
 * Cleans up the shared memory block and exits with a failure.
 * This function halts program execution.
 */
void SingleCoreApplication::abortSafely() {

  Q_D(SingleCoreApplication);

  qCritical() << "SingleCoreApplication: " << d->memory_->error() << d->memory_->errorString();
  delete d;
  ::exit(EXIT_FAILURE);

}
