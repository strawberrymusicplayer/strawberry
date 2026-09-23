/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CREDENTIALSBLOCKINGBACKEND_H
#define CREDENTIALSBLOCKINGBACKEND_H

#include <functional>

#include <QObject>
#include <QString>

#include "includes/scoped_ptr.h"
#include "credentialsbackendinterface.h"
#include "credentialsblockingbackendinterface.h"

class QThreadPool;

// Runs a blocking credentials backend with QtConcurrent, and calls the callbacks in the thread of this object.
// Each blocking backend has its own thread pool with a single thread, so calls never run concurrently, and the thread is only started when it is needed.
class CredentialsBlockingBackend : public CredentialsBackendInterface {
  Q_OBJECT

 public:
  // Takes ownership of the backend.
  explicit CredentialsBlockingBackend(CredentialsBlockingBackendInterface *backend, QObject *parent = nullptr);
  ~CredentialsBlockingBackend() override;

  QString name() const override;
  bool secure() const override;
  void CheckAvailable(const AvailableCallback &available_callback) override;
  void ReadPassword(const QString &service, const ResultCallback &callback) override;
  void SavePassword(const QString &service, const QString &password, const ResultCallback &callback) override;
  void DeletePassword(const QString &service, const ResultCallback &callback) override;

 private:
  // Runs the task in the thread pool, and calls the callback with its result in the thread of this object.
  template<typename T>
  void Run(const std::function<T()> &task, const std::function<void(const T&)> &callback);

 private:
  ScopedPtr<CredentialsBlockingBackendInterface> backend_;
  QThreadPool *thread_pool_;
};

#endif  // CREDENTIALSBLOCKINGBACKEND_H
