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

#include "config.h"

#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QQueue>
#include <QByteArray>
#include <QString>
#include <QSettings>

#include "includes/scoped_ptr.h"
#include "core/logging.h"
#include "core/settings.h"
#include "credentialsmanager.h"
#include "credentialsreply.h"
#include "credentialsresult.h"
#include "credentialsbackendinterface.h"
#include "credentialsblockingbackend.h"
#include "credentialsbackendsettings.h"

#ifdef HAVE_KWALLET
#  include "credentialsbackendkwallet.h"
#endif
#ifdef HAVE_GNOME_KEYRING
#  include "credentialsbackendgnomekeyring.h"
#endif
#ifdef HAVE_LIBSECRET
#  include "credentialsbackendlibsecret.h"
#endif
#ifdef Q_OS_WIN32
#  include "credentialsbackendwincred.h"
#endif
#ifdef Q_OS_MACOS
#  include "credentialsbackendkeychain.h"
#endif

using namespace Qt::Literals::StringLiterals;

CredentialsManager::CredentialsManager(QObject *parent)
    : QObject(parent),
      backend_type_(BackendType::Auto),
      auto_backend_type_(BackendType::Auto),
      reset_backend_(false),
      backend_state_(BackendState::Uninitialized),
      current_backend_type_(BackendType::Auto),
      candidate_backend_type_(BackendType::Auto),
      candidate_backend_is_selected_(false),
      auto_backend_unavailable_(false),
      request_running_(false) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  ReloadSettings();

}

CredentialsManager::~CredentialsManager() = default;

QString CredentialsManager::BackendToString(const BackendType backend) {

  switch (backend) {
    case BackendType::Auto:
      return u"auto"_s;
    case BackendType::KWallet:
      return u"kwallet"_s;
    case BackendType::GnomeKeyring:
      return u"gnomekeyring"_s;
    case BackendType::LibSecret:
      return u"libsecret"_s;
    case BackendType::WindowsCredentialManager:
      return u"wincred"_s;
    case BackendType::Keychain:
      return u"keychain"_s;
    case BackendType::Settings:
      return u"settings"_s;
  }

  return u"auto"_s;

}

CredentialsManager::BackendType CredentialsManager::BackendFromString(const QString &backend) {

  const QString backend_lower = backend.toLower();

  if (backend_lower == "kwallet"_L1) return BackendType::KWallet;
  if (backend_lower == "gnomekeyring"_L1) return BackendType::GnomeKeyring;
  if (backend_lower == "libsecret"_L1) return BackendType::LibSecret;
  if (backend_lower == "wincred"_L1) return BackendType::WindowsCredentialManager;
  if (backend_lower == "keychain"_L1) return BackendType::Keychain;
  if (backend_lower == "settings"_L1) return BackendType::Settings;

  return BackendType::Auto;

}

QList<CredentialsManager::BackendType> CredentialsManager::SupportedBackends() {

  QList<BackendType> backend_types;

#ifdef HAVE_KWALLET
  backend_types << BackendType::KWallet;
#endif
#ifdef HAVE_GNOME_KEYRING
  backend_types << BackendType::GnomeKeyring;
#endif
#ifdef HAVE_LIBSECRET
  backend_types << BackendType::LibSecret;
#endif
#ifdef Q_OS_WIN32
  backend_types << BackendType::WindowsCredentialManager;
#endif
#ifdef Q_OS_MACOS
  backend_types << BackendType::Keychain;
#endif
  backend_types << BackendType::Settings;

  return backend_types;

}

QList<CredentialsManager::BackendType> CredentialsManager::AutoBackendTypes() const {

  QList<BackendType> backend_types = SupportedBackends();

#ifdef HAVE_KWALLET
  // Prefer KWallet on KDE Plasma, otherwise prefer the Secret Service backends.
  backend_types.removeAll(BackendType::KWallet);
  if (qgetenv("XDG_CURRENT_DESKTOP").toLower().contains("kde")) {
    backend_types.prepend(BackendType::KWallet);
  }
  else {
    backend_types.insert(backend_types.indexOf(BackendType::Settings), BackendType::KWallet);
  }
#endif

#ifdef HAVE_LIBSECRET
  // libsecret handles encryption of the session, so prefer it over our own Secret Service implementation.
  if (backend_types.contains(BackendType::GnomeKeyring)) {
    backend_types.removeAll(BackendType::LibSecret);
    backend_types.insert(backend_types.indexOf(BackendType::GnomeKeyring), BackendType::LibSecret);
  }
#endif

  return backend_types;

}

ScopedPtr<CredentialsBackendInterface> CredentialsManager::CreateBackend(const BackendType backend_type) {

  switch (backend_type) {
#ifdef HAVE_KWALLET
    case BackendType::KWallet:
      return ScopedPtr<CredentialsBackendInterface>(new CredentialsBackendKWallet);
#endif
#ifdef HAVE_GNOME_KEYRING
    case BackendType::GnomeKeyring:
      return ScopedPtr<CredentialsBackendInterface>(new CredentialsBackendGnomeKeyring);
#endif
#ifdef HAVE_LIBSECRET
    case BackendType::LibSecret:
      return ScopedPtr<CredentialsBackendInterface>(new CredentialsBackendLibSecret);
#endif
#ifdef Q_OS_WIN32
    case BackendType::WindowsCredentialManager:
      return ScopedPtr<CredentialsBackendInterface>(new CredentialsBlockingBackend(new CredentialsBackendWinCred));
#endif
#ifdef Q_OS_MACOS
    case BackendType::Keychain:
      return ScopedPtr<CredentialsBackendInterface>(new CredentialsBlockingBackend(new CredentialsBackendKeychain));
#endif
    case BackendType::Settings:
      return ScopedPtr<CredentialsBackendInterface>(new CredentialsBlockingBackend(new CredentialsBackendSettings));
    default:
      break;
  }

  return nullptr;

}

void CredentialsManager::ReloadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  const BackendType backend_type = BackendFromString(s.value(kBackend, BackendToString(BackendType::Auto)).toString());
  const BackendType auto_backend_type = BackendFromString(s.value(kAutoBackend, BackendToString(BackendType::Auto)).toString());
  s.endGroup();

  QMetaObject::invokeMethod(this, [this, backend_type, auto_backend_type]() {
    if (backend_type == backend_type_ && auto_backend_type == auto_backend_type_) return;
    backend_type_ = backend_type;
    auto_backend_type_ = auto_backend_type;
    reset_backend_ = true;
    StartNextRequest();
  }, Qt::QueuedConnection);

}

CredentialsReplyPtr CredentialsManager::ReadPasswordAsync(const QString &service) {

  CredentialsReplyPtr reply = CredentialsReply::Create(service);
  AddRequest(Request{ RequestType::Read, reply, QString() });

  return reply;

}

CredentialsReplyPtr CredentialsManager::SavePasswordAsync(const QString &service, const QString &password) {

  if (password.isEmpty()) {
    return DeletePasswordAsync(service);
  }

  CredentialsReplyPtr reply = CredentialsReply::Create(service);
  AddRequest(Request{ RequestType::Save, reply, password });

  return reply;

}

CredentialsReplyPtr CredentialsManager::DeletePasswordAsync(const QString &service) {

  CredentialsReplyPtr reply = CredentialsReply::Create(service);
  AddRequest(Request{ RequestType::Delete, reply, QString() });

  return reply;

}

void CredentialsManager::AddRequest(const Request &request) {

  if (request.reply->service().isEmpty()) {
    request.reply->Finish(CredentialsResult::Error(u"Missing service name."_s));
    return;
  }

  // Can be called from any thread, the queue is only used in the thread of this object.
  QMetaObject::invokeMethod(this, [this, request]() {
    requests_.enqueue(request);
    StartNextRequest();
  }, Qt::QueuedConnection);

}

void CredentialsManager::StartNextRequest() {

  if (request_running_ || backend_state_ == BackendState::Initializing) return;

  if (reset_backend_) {
    reset_backend_ = false;
    backend_.reset();
    backend_state_ = BackendState::Uninitialized;
  }

  if (requests_.isEmpty()) return;

  if (backend_state_ == BackendState::Uninitialized) {
    InitializeBackend();
    return;
  }

  request_running_ = true;

  const Request request = requests_.dequeue();

  // The remembered backend is checked again before each request while it's not available, since it's usually only temporarily unavailable, for example when the keyring daemon wasn't started yet.
  if (auto_backend_unavailable_) {
    CheckRememberedBackend(request);
    return;
  }

  RunRequest(request);

}

void CredentialsManager::CheckRememberedBackend(const Request &request) {

  other_backend_ = CreateBackend(auto_backend_type_);
  if (!other_backend_) {
    RememberedBackendChecked(request, false);
    return;
  }

  other_backend_->CheckAvailable([this, request](const bool available) {
    // Handle the result after the backend has returned from the callback, since the backend is deleted.
    QMetaObject::invokeMethod(this, [this, request, available]() { RememberedBackendChecked(request, available); }, Qt::QueuedConnection);
  });

}

void CredentialsManager::RememberedBackendChecked(const Request &request, const bool available) {

  if (available) {
    qLog(Info) << "Credentials backend" << other_backend_->name() << "which stored passwords before is available now, using it.";
    backend_ = std::move(other_backend_);
    current_backend_type_ = auto_backend_type_;
    auto_backend_unavailable_ = false;
    RunRequest(request);
    return;
  }

  other_backend_.reset();

  // Passwords are only saved and deleted in the remembered backend, otherwise a newer or deleted password in another backend would be hidden by the password in the remembered backend when it's available again.
  if (request.type != RequestType::Read) {
    const QString error = u"Credentials backend %1 which stored passwords before is not available."_s.arg(BackendToString(auto_backend_type_));
    qLog(Error) << "Failed to" << (request.type == RequestType::Save ? "save" : "delete") << "password for" << request.reply->service() << error;
    FinishRequest(request, CredentialsResult::Error(error), false);
    return;
  }

  RunRequest(request);

}

void CredentialsManager::RunRequest(const Request &request) {

  const CredentialsBackendInterface::ResultCallback callback = [this, request](const CredentialsResult &credentials_result) { RequestFinished(request, credentials_result); };
  const QString service = request.reply->service();

  switch (request.type) {
    case RequestType::Read:
      backend_->ReadPassword(service, callback);
      break;
    case RequestType::Save:
      backend_->SavePassword(service, request.password, callback);
      break;
    case RequestType::Delete:
      backend_->DeletePassword(service, callback);
      break;
  }

}

void CredentialsManager::RequestFinished(const Request &request, const CredentialsResult &credentials_result) {

  if (credentials_result.status == CredentialsResult::Status::Error) {
    switch (request.type) {
      case RequestType::Read:
        qLog(Error) << "Failed to read password for" << request.reply->service() << "from" << backend_->name() << credentials_result.error;
        break;
      case RequestType::Save:
        qLog(Error) << "Failed to save password for" << request.reply->service() << "to" << backend_->name() << credentials_result.error;
        break;
      case RequestType::Delete:
        qLog(Error) << "Failed to delete password for" << request.reply->service() << "from" << backend_->name() << credentials_result.error;
        break;
    }
  }

  // In Auto mode, the settings file is used when no secure backend is available, a password not found in a secure backend which is available later is moved from the settings file to it.
  if (request.type == RequestType::Read && credentials_result.status == CredentialsResult::Status::NotFound && backend_type_ == BackendType::Auto && current_backend_type_ != BackendType::Settings) {
    MovePasswordFromSettings(request);
    return;
  }

  // A password which was not moved from the settings file yet is deleted there too when it's saved or deleted in a secure backend.
  // Otherwise a deleted password would be moved back when it's read, and an old password would be used when no secure backend is available, also after selecting Auto mode later.
  if ((request.type == RequestType::Save || request.type == RequestType::Delete) && credentials_result.success() && current_backend_type_ != BackendType::Settings) {
    DeletePasswordFromSettings(request, credentials_result);
    return;
  }

  FinishRequest(request, credentials_result, credentials_result.success() && request.type != RequestType::Delete);

}

void CredentialsManager::FinishRequest(const Request &request, const CredentialsResult &credentials_result, const bool password_in_backend) {

  // In Auto mode, a different backend can be detected on the next start, for example when the desktop is not detected, then the stored passwords would look missing.
  // So the backend which stored or returned a password is saved, and used first on the next start.
  // The settings file is not saved, so a secure backend is used again when it's available.
  // When the remembered backend was not available, it's kept, since it's usually only temporarily unavailable, and the passwords are stored there.
  if (password_in_backend && backend_type_ == BackendType::Auto && !auto_backend_unavailable_ && current_backend_type_ != BackendType::Settings && current_backend_type_ != auto_backend_type_) {
    auto_backend_type_ = current_backend_type_;
    Settings s;
    s.beginGroup(kSettingsGroup);
    s.setValue(kAutoBackend, BackendToString(auto_backend_type_));
    s.endGroup();
  }

  request.reply->Finish(credentials_result);

  // Start the next request after the backend has returned from the callback.
  request_running_ = false;
  QMetaObject::invokeMethod(this, &CredentialsManager::StartNextRequest, Qt::QueuedConnection);

}

void CredentialsManager::MovePasswordFromSettings(const Request &request) {

  other_backend_ = CreateBackend(BackendType::Settings);
  other_backend_->ReadPassword(request.reply->service(), [this, request](const CredentialsResult &settings_result) {
    // Handle the result after the backend has returned from the callback, since the backend is deleted.
    QMetaObject::invokeMethod(this, [this, request, settings_result]() { SettingsPasswordRead(request, settings_result); }, Qt::QueuedConnection);
  });

}

void CredentialsManager::SettingsPasswordRead(const Request &request, const CredentialsResult &settings_result) {

  if (!settings_result.success()) {
    other_backend_.reset();
    FinishRequest(request, CredentialsResult::NotFound(), false);
    return;
  }

  // The password is not moved to a backend which is only used because the remembered backend is not available.
  if (auto_backend_unavailable_) {
    other_backend_.reset();
    FinishRequest(request, settings_result, false);
    return;
  }

  backend_->SavePassword(request.reply->service(), settings_result.password, [this, request, settings_result](const CredentialsResult &save_result) {
    QMetaObject::invokeMethod(this, [this, request, settings_result, save_result]() { SettingsPasswordSaved(request, settings_result, save_result); }, Qt::QueuedConnection);
  });

}

void CredentialsManager::SettingsPasswordSaved(const Request &request, const CredentialsResult &settings_result, const CredentialsResult &save_result) {

  // The password is still returned when it can't be moved, it stays in the settings file.
  if (!save_result.success()) {
    qLog(Error) << "Failed to move password for" << request.reply->service() << "from the settings file to" << backend_->name() << save_result.error;
    other_backend_.reset();
    FinishRequest(request, settings_result, false);
    return;
  }

  qLog(Info) << "Moved password for" << request.reply->service() << "from the settings file to" << backend_->name();

  other_backend_->DeletePassword(request.reply->service(), [this, request, settings_result](const CredentialsResult &delete_result) {
    QMetaObject::invokeMethod(this, [this, request, settings_result, delete_result]() {
      if (!delete_result.success()) {
        qLog(Error) << "Failed to delete password for" << request.reply->service() << "from the settings file" << delete_result.error;
      }
      other_backend_.reset();
      FinishRequest(request, settings_result, true);
    }, Qt::QueuedConnection);
  });

}

void CredentialsManager::DeletePasswordFromSettings(const Request &request, const CredentialsResult &credentials_result) {

  other_backend_ = CreateBackend(BackendType::Settings);
  other_backend_->DeletePassword(request.reply->service(), [this, request, credentials_result](const CredentialsResult &delete_result) {
    QMetaObject::invokeMethod(this, [this, request, credentials_result, delete_result]() {
      if (!delete_result.success()) {
        qLog(Error) << "Failed to delete password for" << request.reply->service() << "from the settings file" << delete_result.error;
      }
      other_backend_.reset();
      // A deleted password still in the settings file would be moved back when it's read, so the delete fails.
      // The saved password is in the secure backend, so saving doesn't fail.
      FinishRequest(request, request.type == RequestType::Delete && !delete_result.success() ? delete_result : credentials_result, request.type != RequestType::Delete);
    }, Qt::QueuedConnection);
  });

}

void CredentialsManager::InitializeBackend() {

  backend_state_ = BackendState::Initializing;
  auto_backend_unavailable_ = false;

  // Try the backend selected in the settings first, or in Auto mode the backend which stored passwords before, then detect a backend automatically.
  candidate_backend_types_.clear();
  if (preferred_backend_type() != BackendType::Auto) {
    candidate_backend_types_ << preferred_backend_type();
  }
  const QList<BackendType> auto_backend_types = AutoBackendTypes();
  for (const BackendType backend_type : auto_backend_types) {
    if (!candidate_backend_types_.contains(backend_type)) {
      candidate_backend_types_ << backend_type;
    }
  }

  CheckNextBackend();

}

void CredentialsManager::CheckNextBackend() {

  while (!candidate_backend_types_.isEmpty()) {
    const BackendType backend_type = candidate_backend_types_.takeFirst();
    candidate_backend_type_ = backend_type;
    candidate_backend_is_selected_ = preferred_backend_type() != BackendType::Auto && backend_type == preferred_backend_type();
    candidate_backend_ = CreateBackend(backend_type);
    if (!candidate_backend_) {
      if (candidate_backend_is_selected_) {
        qLog(Warning) << "Credentials backend" << BackendToString(backend_type) << "is not supported, trying to detect a backend automatically.";
      }
      continue;
    }
    candidate_backend_->CheckAvailable([this](const bool available) {
      // Handle the result after the backend has returned from the callback, since the backend might be deleted.
      QMetaObject::invokeMethod(this, [this, available]() { BackendChecked(available); }, Qt::QueuedConnection);
    });
    return;
  }

  // The settings backend is always available, so this should not happen.
  backend_ = CreateBackend(BackendType::Settings);
  current_backend_type_ = BackendType::Settings;
  BackendReady();

}

void CredentialsManager::BackendChecked(const bool available) {

  if (available) {
    backend_ = std::move(candidate_backend_);
    current_backend_type_ = candidate_backend_type_;
    BackendReady();
    return;
  }

  if (candidate_backend_is_selected_) {
    if (backend_type_ == BackendType::Auto) {
      auto_backend_unavailable_ = true;
      qLog(Warning) << "Credentials backend" << candidate_backend_->name() << "which stored passwords before is not available, trying to detect a backend automatically, passwords stored in it are not available, and passwords can't be saved or deleted until it's available.";
    }
    else {
      qLog(Warning) << "Credentials backend" << candidate_backend_->name() << "is not available, trying to detect a backend automatically.";
    }
  }

  candidate_backend_.reset();
  CheckNextBackend();

}

void CredentialsManager::BackendReady() {

  candidate_backend_types_.clear();
  backend_state_ = BackendState::Ready;

  if (backend_->secure()) {
    qLog(Info) << "Using credentials backend" << backend_->name();
  }
  else if (backend_type_ == BackendType::Settings) {
    qLog(Warning) << "Using the settings file for storing passwords, passwords are not stored securely.";
  }
  else {
    qLog(Warning) << "No secure credentials backend is available, passwords will be stored in the settings file.";
  }

  StartNextRequest();

}
