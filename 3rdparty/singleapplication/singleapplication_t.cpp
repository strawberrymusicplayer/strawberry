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
#include <memory>

#include <boost/scope_exit.hpp>

#include <QtGlobal>
#include <QThread>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QtDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#  include <QNativeIpcKey>
#endif

#include "singleapplication_t.h"
#include "singleapplication_p.h"

/**
 * @brief Constructor. Checks and fires up LocalServer or closes the program if another instance already exists
 * @param argc
 * @param argv
 * @param allowSecondary Whether to enable secondary instance support
 * @param options Optional flags to toggle specific behaviour
 * @param timeout Maximum time blocking functions are allowed during app load
 */
SingleApplicationClass::SingleApplicationClass(int &argc, char *argv[], const bool allowSecondary, const Options options, const int timeout)
    : ApplicationClass(argc, argv),
      d_ptr(new SingleApplicationPrivateClass(this)) {

#if defined(SINGLEAPPLICATION)
  Q_D(SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(SingleCoreApplication);
#endif

  // Store the current mode of the program
  d->options_ = options;

  // Generating an application ID used for identifying the shared memory block and QLocalServer
  d->genBlockServerName();

  // To mitigate QSharedMemory issues with large amount of processes attempting to attach at the same time
  SingleApplicationPrivateClass::randomSleep();

#ifdef Q_OS_UNIX
  // By explicitly attaching it and then deleting it we make sure that the memory is deleted even after the process has crashed on Unix.
  {
#  if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    std::unique_ptr<QSharedMemory> memory = std::make_unique<QSharedMemory>(QNativeIpcKey(d->blockServerName_));
#  else
    std::unique_ptr<QSharedMemory> memory = std::make_unique<QSharedMemory>(d->blockServerName_);
#  endif
    if (memory->attach()) {
      memory->detach();
    }
  }
#endif

  // Guarantee thread safe behaviour with a shared memory block.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  QSharedMemory *memory = new QSharedMemory(QNativeIpcKey(d->blockServerName_), this);
#else
  QSharedMemory *memory = new QSharedMemory(d->blockServerName_, this);
#endif
  d->memory_ = memory;

  bool primary = false;

  // Create a shared memory block
  if (d->memory_->create(sizeof(SingleApplicationPrivateClass::InstancesInfo))) {
    primary = true;
  }
  else if (d->memory_->error() == QSharedMemory::AlreadyExists) {
    if (!d->memory_->attach()) {
      qCritical() << "SingleApplication: Unable to attach to shared memory block:" << d->memory_->error() << d->memory_->errorString();
      return;
    }
  }
  else {
    qCritical() << "SingleApplication: Unable to create shared memory block:" << d->memory_->error() << d->memory_->errorString();
    return;
  }

  bool locked = false;

  BOOST_SCOPE_EXIT((memory)(&locked)) {
    if (locked && !memory->unlock()) {
      qWarning() << "SingleApplication: Unable to unlock shared memory block:" << memory->error() << memory->errorString();
      return;
    }
  }BOOST_SCOPE_EXIT_END

  if (!d->memory_->lock()) {
    qCritical() << "SingleApplication: Unable to lock shared memory block:" << d->memory_->error() << d->memory_->errorString();
    return;
  }
  locked = true;

  if (primary) {
    // Initialize the shared memory block
    d->initializeMemoryBlock();
  }

  SingleApplicationPrivateClass::InstancesInfo *instance = static_cast<SingleApplicationPrivateClass::InstancesInfo*>(d->memory_->data());
  QElapsedTimer time;
  time.start();

  // Make sure the shared memory block is initialized and in a consistent state
  while (d->blockChecksum() != instance->checksum) {

    // If more than 5 seconds have elapsed, assume the primary instance crashed and assume its position
    if (time.elapsed() > 5000) {
      qWarning() << "SingleApplication: Shared memory block has been in an inconsistent state from more than 5 seconds. Assuming primary instance failure.";
      d->initializeMemoryBlock();
    }

    // Otherwise wait for a random period and try again.
    // The random sleep here limits the probability of a collision between two racing apps and allows the app to initialize faster
    if (locked) {
      if (d->memory_->unlock()) {
        locked = false;
      }
      else {
        qCritical() << "SingleApplication: Unable to unlock shared memory block for random wait:" << memory->error() << memory->errorString();
        return;
      }
    }

    SingleApplicationPrivateClass::randomSleep();

    if (!d->memory_->lock()) {
      qCritical() << "SingleApplication: Unable to lock shared memory block after random wait:" << memory->error() << memory->errorString();
      return;
    }
    locked = true;

  }

  if (instance->primary) {
    // Check if another instance can be started
    if (allowSecondary) {
      d->startSecondary();
      if (d->options_ & Mode::SecondaryNotification) {
        d->connectToPrimary(timeout, SingleApplicationPrivateClass::SecondaryInstance);
      }
    }
  }
  else {
    d->startPrimary();
    primary = true;
  }

  if (locked) {
    if (d->memory_->unlock()) {
      locked = false;
    }
    else {
      qWarning() << "SingleApplication: Unable to unlock shared memory block:" << memory->error() << memory->errorString();
    }
  }

  if (!primary && !allowSecondary) {
    d->connectToPrimary(timeout, SingleApplicationPrivateClass::NewInstance);
  }

}

SingleApplicationClass::~SingleApplicationClass() {

#if defined(SINGLEAPPLICATION)
  Q_D(SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(SingleCoreApplication);
#endif

  delete d;

}

/**
 * Checks if the current application instance is primary.
 * @return Returns true if the instance is primary, false otherwise.
 */
bool SingleApplicationClass::isPrimary() const {

#if defined(SINGLEAPPLICATION)
  Q_D(const SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(const SingleCoreApplication);
#endif

  return d->server_ != nullptr;

}

/**
 * Checks if the current application instance is secondary.
 * @return Returns true if the instance is secondary, false otherwise.
 */
bool SingleApplicationClass::isSecondary() const {

#if defined(SINGLEAPPLICATION)
  Q_D(const SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(const SingleCoreApplication);
#endif

  return d->server_ == nullptr;

}

/**
 * Allows you to identify an instance by returning unique consecutive instance ids.
 * It is reset when the first (primary) instance of your app starts and only incremented afterwards.
 * @return Returns a unique instance id.
 */
quint32 SingleApplicationClass::instanceId() const {

#if defined(SINGLEAPPLICATION)
  Q_D(const SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(const SingleCoreApplication);
#endif

  return d->instanceNumber_;

}

/**
 * Returns the OS PID (Process Identifier) of the process running the primary instance.
 * Especially useful when SingleApplication is coupled with OS. specific APIs.
 * @return Returns the primary instance PID.
 */
qint64 SingleApplicationClass::primaryPid() const {

#if defined(SINGLEAPPLICATION)
  Q_D(const SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(const SingleCoreApplication);
#endif

  return d->primaryPid();

}

/**
 * Returns the username the primary instance is running as.
 * @return Returns the username the primary instance is running as.
 */
QString SingleApplicationClass::primaryUser() const {

#if defined(SINGLEAPPLICATION)
  Q_D(const SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(const SingleCoreApplication);
#endif

  return d->primaryUser();

}

/**
 * Returns the username the current instance is running as.
 * @return Returns the username the current instance is running as.
 */
QString SingleApplicationClass::currentUser() const {
  return SingleApplicationPrivateClass::getUsername();
}

/**
 * Sends message to the Primary Instance.
 * @param message The message to send.
 * @param timeout the maximum timeout in milliseconds for blocking functions.
 * @return true if the message was sent successfully, false otherwise.
 */
bool SingleApplicationClass::sendMessage(const QByteArray &message, const int timeout) {

#if defined(SINGLEAPPLICATION)
  Q_D(SingleApplication);
#elif defined(SINGLECOREAPPLICATION)
  Q_D(SingleCoreApplication);
#endif

  // Nobody to connect to
  if (isPrimary()) return false;

  // Make sure the socket is connected
  if (!d->connectToPrimary(timeout, SingleApplicationPrivateClass::Reconnect)) {
    return false;
  }

  return d->writeConfirmedMessage(timeout, message);

}
