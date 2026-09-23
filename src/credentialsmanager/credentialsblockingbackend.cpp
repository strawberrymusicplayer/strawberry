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

#include <functional>

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include "credentialsblockingbackend.h"
#include "credentialsblockingbackendinterface.h"
#include "credentialsresult.h"

CredentialsBlockingBackend::CredentialsBlockingBackend(CredentialsBlockingBackendInterface *backend, QObject *parent)
    : CredentialsBackendInterface(parent),
      backend_(backend),
      thread_pool_(new QThreadPool(this)) {

  thread_pool_->setMaxThreadCount(1);

}

CredentialsBlockingBackend::~CredentialsBlockingBackend() {

  // Wait for a running task, since it uses the backend.
  thread_pool_->waitForDone();

}

QString CredentialsBlockingBackend::name() const {
  return backend_->name();
}

bool CredentialsBlockingBackend::secure() const {
  return backend_->secure();
}

template<typename T>
void CredentialsBlockingBackend::Run(const std::function<T()> &task, const std::function<void(const T&)> &callback) {

  // The watcher is deleted with this object, so the callback is not called after this object is deleted.
  QFutureWatcher<T> *watcher = new QFutureWatcher<T>(this);
  QObject::connect(watcher, &QFutureWatcher<T>::finished, this, [watcher, callback]() {
    watcher->deleteLater();
    callback(watcher->result());
  });
  watcher->setFuture(QtConcurrent::run(thread_pool_, task));

}

void CredentialsBlockingBackend::CheckAvailable(const AvailableCallback &available_callback) {

  CredentialsBlockingBackendInterface *backend = &*backend_;
  Run<bool>([backend]() { return backend->IsAvailable(); }, available_callback);

}

void CredentialsBlockingBackend::ReadPassword(const QString &service, const ResultCallback &callback) {

  CredentialsBlockingBackendInterface *backend = &*backend_;
  Run<CredentialsResult>([backend, service]() { return backend->ReadPassword(service); }, callback);

}

void CredentialsBlockingBackend::SavePassword(const QString &service, const QString &password, const ResultCallback &callback) {

  CredentialsBlockingBackendInterface *backend = &*backend_;
  Run<CredentialsResult>([backend, service, password]() { return backend->SavePassword(service, password); }, callback);

}

void CredentialsBlockingBackend::DeletePassword(const QString &service, const ResultCallback &callback) {

  CredentialsBlockingBackendInterface *backend = &*backend_;
  Run<CredentialsResult>([backend, service]() { return backend->DeletePassword(service); }, callback);

}
