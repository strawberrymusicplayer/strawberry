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

#include <glib.h>
#include <gio/gio.h>
#include <libsecret/secret.h>

#include <memory>

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QMutexLocker>

#include "core/logging.h"
#include "credentialsbackendlibsecret.h"
#include "credentialsbackendlibsecretguard.h"
#include "credentialsresult.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Same schema name and attributes as the GNOME Keyring backend.
constexpr char kSchemaName[] = "org.strawberrymusicplayer.Password";
constexpr char kApplication[] = "strawberry";

const SecretSchema *Schema() {

  static const SecretSchema schema = []() {
    SecretSchema s{};
    s.name = kSchemaName;
    s.flags = SECRET_SCHEMA_NONE;
    s.attributes[0] = { "application", SECRET_SCHEMA_ATTRIBUTE_STRING };
    s.attributes[1] = { "service", SECRET_SCHEMA_ATTRIBUTE_STRING };
    s.attributes[2] = { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING };
    return s;
  }();

  return &schema;

}

QString ErrorString(GError *error) {

  const QString error_string = QString::fromUtf8(error->message);
  g_error_free(error);

  return error_string;

}

struct AvailableCallData {
  std::shared_ptr<CredentialsBackendLibSecretGuard> guard;
  CredentialsBackendInterface::AvailableCallback available_callback;
};

struct ResultCallData {
  std::shared_ptr<CredentialsBackendLibSecretGuard> guard;
  CredentialsBackendInterface::ResultCallback callback;
};

// Calls the callback in the thread of the backend, unless the backend is deleted.
template<typename Callback, typename Value>
void Deliver(const std::shared_ptr<CredentialsBackendLibSecretGuard> &guard, const Callback &callback, const Value &value) {

  // The backend destructor locks the mutex before clearing the pointer, so the backend is valid while the mutex is locked.
  QMutexLocker l(&guard->mutex);
  if (!guard->backend) return;
  QMetaObject::invokeMethod(guard->backend, [callback, value]() { callback(value); }, Qt::QueuedConnection);

}

void AvailableLookupFinished(GObject *source, GAsyncResult *result, gpointer user_data) {

  Q_UNUSED(source)

  AvailableCallData *data = static_cast<AvailableCallData*>(user_data);

  // A lookup that finds nothing returns nullptr without an error, so only an error means that libsecret is not available.
  GError *error = nullptr;
  gchar *password = secret_password_lookup_finish(result, &error);
  const bool available = error == nullptr;
  if (password) {
    secret_password_free(password);
  }
  if (error) {
    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      qLog(Debug) << "libsecret is not available:" << QString::fromUtf8(error->message);
    }
    g_error_free(error);
  }

  Deliver(data->guard, data->available_callback, available);
  delete data;

}

void LookupFinished(GObject *source, GAsyncResult *result, gpointer user_data) {

  Q_UNUSED(source)

  ResultCallData *data = static_cast<ResultCallData*>(user_data);

  GError *error = nullptr;
  gchar *password = secret_password_lookup_finish(result, &error);
  CredentialsResult credentials_result;
  if (error) {
    credentials_result = CredentialsResult::Error(ErrorString(error));
  }
  else if (!password) {
    credentials_result = CredentialsResult::NotFound();
  }
  else {
    credentials_result = CredentialsResult::Success(QString::fromUtf8(password));
  }
  if (password) {
    secret_password_free(password);
  }

  Deliver(data->guard, data->callback, credentials_result);
  delete data;

}

void StoreFinished(GObject *source, GAsyncResult *result, gpointer user_data) {

  Q_UNUSED(source)

  ResultCallData *data = static_cast<ResultCallData*>(user_data);

  GError *error = nullptr;
  const gboolean success = secret_password_store_finish(result, &error);
  CredentialsResult credentials_result;
  if (error) {
    credentials_result = CredentialsResult::Error(ErrorString(error));
  }
  else if (!success) {
    credentials_result = CredentialsResult::Error(u"Failed to store password."_s);
  }
  else {
    credentials_result = CredentialsResult::Success();
  }

  Deliver(data->guard, data->callback, credentials_result);
  delete data;

}

void ClearFinished(GObject *source, GAsyncResult *result, gpointer user_data) {

  Q_UNUSED(source)

  ResultCallData *data = static_cast<ResultCallData*>(user_data);

  // Returns false without error if there was no password to delete.
  GError *error = nullptr;
  secret_password_clear_finish(result, &error);
  const CredentialsResult credentials_result = error ? CredentialsResult::Error(ErrorString(error)) : CredentialsResult::Success();

  Deliver(data->guard, data->callback, credentials_result);
  delete data;

}

}  // namespace

CredentialsBackendLibSecret::CredentialsBackendLibSecret(QObject *parent)
    : CredentialsBackendInterface(parent),
      guard_(std::make_shared<CredentialsBackendLibSecretGuard>()),
      cancellable_(g_cancellable_new()) {

  guard_->backend = this;

}

CredentialsBackendLibSecret::~CredentialsBackendLibSecret() {

  {
    QMutexLocker l(&guard_->mutex);
    guard_->backend = nullptr;
  }

  g_cancellable_cancel(cancellable_);
  g_object_unref(cancellable_);

}

QString CredentialsBackendLibSecret::name() const {
  return u"libsecret"_s;
}

void CredentialsBackendLibSecret::CheckAvailable(const AvailableCallback &available_callback) {

  // Use a password lookup instead of secret_service_get(), so that the check goes through the same backend as the actual requests.
  // Inside Flatpak, libsecret uses a local keyring file unlocked through the Secret portal instead of the Secret Service D-Bus API.
  secret_password_lookup(Schema(), cancellable_, AvailableLookupFinished, new AvailableCallData{ guard_, available_callback }, "application", kApplication, "service", "", nullptr);

}

void CredentialsBackendLibSecret::ReadPassword(const QString &service, const ResultCallback &callback) {

  const QByteArray service_utf8 = service.toUtf8();

  secret_password_lookup(Schema(), cancellable_, LookupFinished, new ResultCallData{ guard_, callback }, "application", kApplication, "service", service_utf8.constData(), nullptr);

}

void CredentialsBackendLibSecret::SavePassword(const QString &service, const QString &password, const ResultCallback &callback) {

  const QByteArray service_utf8 = service.toUtf8();
  const QByteArray label = QStringLiteral("Strawberry %1 password").arg(service).toUtf8();
  const QByteArray password_utf8 = password.toUtf8();

  secret_password_store(Schema(), SECRET_COLLECTION_DEFAULT, label.constData(), password_utf8.constData(), cancellable_, StoreFinished, new ResultCallData{ guard_, callback }, "application", kApplication, "service", service_utf8.constData(), nullptr);

}

void CredentialsBackendLibSecret::DeletePassword(const QString &service, const ResultCallback &callback) {

  const QByteArray service_utf8 = service.toUtf8();

  secret_password_clear(Schema(), cancellable_, ClearFinished, new ResultCallData{ guard_, callback }, "application", kApplication, "service", service_utf8.constData(), nullptr);

}
