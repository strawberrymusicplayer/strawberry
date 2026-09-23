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

#include <QtGlobal>
#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include "credentialsbackendkwallet.h"
#include "credentialsdbusutils.h"
#include "credentialsresult.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kInterface[] = "org.kde.KWallet";
constexpr char kAppId[] = "Strawberry";
constexpr char kFolder[] = "Strawberry";

// Opening the wallet can show a password prompt, so give the user plenty of time.
constexpr int kOpenTimeout = 300000;

struct KWalletService {
  const char *service_name;
  const char *path;
};

constexpr KWalletService kKWalletServices[] = {
  { "org.kde.kwalletd6", "/modules/kwalletd6" },
  { "org.kde.kwalletd5", "/modules/kwalletd5" },
};

constexpr int kKWalletServicesCount = sizeof(kKWalletServices) / sizeof(kKWalletServices[0]);

template<typename T>
bool ReplyValue(const QDBusMessage &reply, T &value, QString &error) {

  const QDBusReply<T> dbus_reply = reply;
  if (!dbus_reply.isValid()) {
    error = dbus_reply.error().message();
    if (error.isEmpty()) error = u"Invalid reply from KWallet."_s;
    return false;
  }

  value = dbus_reply.value();

  return true;

}

}  // namespace

CredentialsBackendKWallet::CredentialsBackendKWallet(QObject *parent) : CredentialsBackendInterface(parent), handle_(-1) {}

CredentialsBackendKWallet::~CredentialsBackendKWallet() {

  if (handle_ >= 0 && !service_name_.isEmpty()) {
    QDBusMessage message = QDBusMessage::createMethodCall(service_name_, path_, QLatin1String(kInterface), u"close"_s);
    message.setArguments(QVariantList() << handle_ << false << QLatin1String(kAppId));
    QDBusConnection::sessionBus().send(message);
  }

}

QString CredentialsBackendKWallet::name() const {
  return u"KWallet"_s;
}

void CredentialsBackendKWallet::Call(const QString &method, const QVariantList &arguments, const CredentialsDBusUtils::ReplyCallback &callback, const int timeout) {

  QDBusMessage message = QDBusMessage::createMethodCall(service_name_, path_, QLatin1String(kInterface), method);
  message.setArguments(arguments);

  CredentialsDBusUtils::AsyncCall(this, message, callback, timeout);

}

void CredentialsBackendKWallet::CheckAvailable(const AvailableCallback &available_callback) {

  CheckService(0, available_callback);

}

void CredentialsBackendKWallet::CheckService(const int index, const AvailableCallback &available_callback) {

  if (index >= kKWalletServicesCount) {
    service_name_.clear();
    path_.clear();
    available_callback(false);
    return;
  }

  const QString service_name = QLatin1String(kKWalletServices[index].service_name);
  const QString path = QLatin1String(kKWalletServices[index].path);

  CredentialsDBusUtils::CheckServiceAvailable(this, service_name, [this, index, service_name, path, available_callback](const bool available) {
    if (!available) {
      CheckService(index + 1, available_callback);
      return;
    }
    service_name_ = service_name;
    path_ = path;
    Call(u"isEnabled"_s, QVariantList(), [this, index, available_callback](const QDBusMessage &reply) {
      bool enabled = false;
      QString error;
      if (ReplyValue(reply, enabled, error) && enabled) {
        available_callback(true);
      }
      else {
        CheckService(index + 1, available_callback);
      }
    });
  });

}

void CredentialsBackendKWallet::Open(const SuccessCallback &callback) {

  if (service_name_.isEmpty()) {
    callback(false, u"KWallet is not available."_s);
    return;
  }

  if (handle_ < 0) {
    OpenWallet(callback);
    return;
  }

  // The wallet might have been closed since it was opened.
  Call(u"isOpen"_s, QVariantList() << handle_, [this, callback](const QDBusMessage &reply) {
    bool is_open = false;
    QString error;
    if (ReplyValue(reply, is_open, error) && is_open) {
      callback(true, QString());
      return;
    }
    handle_ = -1;
    OpenWallet(callback);
  });

}

void CredentialsBackendKWallet::OpenWallet(const SuccessCallback &callback) {

  Call(u"networkWallet"_s, QVariantList(), [this, callback](const QDBusMessage &wallet_reply) {
    QString wallet;
    QString error;
    if (!ReplyValue(wallet_reply, wallet, error)) {
      callback(false, error);
      return;
    }
    Call(u"open"_s, QVariantList() << wallet << static_cast<qlonglong>(0) << QLatin1String(kAppId), [this, wallet, callback](const QDBusMessage &open_reply) {
      int handle = -1;
      QString open_error;
      if (!ReplyValue(open_reply, handle, open_error)) {
        callback(false, open_error);
        return;
      }
      if (handle < 0) {
        callback(false, QStringLiteral("Failed to open wallet %1.").arg(wallet));
        return;
      }
      handle_ = handle;
      callback(true, QString());
    }, kOpenTimeout);
  });

}

void CredentialsBackendKWallet::HasFolder(const BoolCallback &callback) {

  Call(u"hasFolder"_s, QVariantList() << handle_ << QLatin1String(kFolder) << QLatin1String(kAppId), [callback](const QDBusMessage &reply) {
    bool has_folder = false;
    QString error;
    const bool success = ReplyValue(reply, has_folder, error);
    callback(success, has_folder, error);
  });

}

void CredentialsBackendKWallet::HasEntry(const QString &service, const BoolCallback &callback) {

  HasFolder([this, service, callback](const bool success, const bool has_folder, const QString &error) {
    if (!success || !has_folder) {
      callback(success, false, error);
      return;
    }
    Call(u"hasEntry"_s, QVariantList() << handle_ << QLatin1String(kFolder) << service << QLatin1String(kAppId), [callback](const QDBusMessage &reply) {
      bool has_entry = false;
      QString entry_error;
      const bool entry_success = ReplyValue(reply, has_entry, entry_error);
      callback(entry_success, has_entry, entry_error);
    });
  });

}

void CredentialsBackendKWallet::CreateFolder(const SuccessCallback &callback) {

  Call(u"createFolder"_s, QVariantList() << handle_ << QLatin1String(kFolder) << QLatin1String(kAppId), [callback](const QDBusMessage &reply) {
    bool folder_created = false;
    QString error;
    if (!ReplyValue(reply, folder_created, error)) {
      callback(false, error);
      return;
    }
    if (!folder_created) {
      callback(false, QStringLiteral("Failed to create folder %1 in wallet.").arg(QLatin1String(kFolder)));
      return;
    }
    callback(true, QString());
  });

}

void CredentialsBackendKWallet::WritePassword(const QString &service, const QString &password, const ResultCallback &callback) {

  Call(u"writePassword"_s, QVariantList() << handle_ << QLatin1String(kFolder) << service << password << QLatin1String(kAppId), [callback](const QDBusMessage &reply) {
    int result = -1;
    QString error;
    if (!ReplyValue(reply, result, error)) {
      callback(CredentialsResult::Error(error));
      return;
    }
    if (result != 0) {
      callback(CredentialsResult::Error(QStringLiteral("Failed to write password to wallet, error code %1.").arg(result)));
      return;
    }
    callback(CredentialsResult::Success());
  });

}

void CredentialsBackendKWallet::ReadPassword(const QString &service, const ResultCallback &callback) {

  Open([this, service, callback](const bool open_success, const QString &open_error) {
    if (!open_success) {
      callback(CredentialsResult::Error(open_error));
      return;
    }
    HasEntry(service, [this, service, callback](const bool success, const bool has_entry, const QString &error) {
      if (!success) {
        callback(CredentialsResult::Error(error));
        return;
      }
      if (!has_entry) {
        callback(CredentialsResult::NotFound());
        return;
      }
      Call(u"readPassword"_s, QVariantList() << handle_ << QLatin1String(kFolder) << service << QLatin1String(kAppId), [callback](const QDBusMessage &reply) {
        QString password;
        QString read_error;
        if (!ReplyValue(reply, password, read_error)) {
          callback(CredentialsResult::Error(read_error));
          return;
        }
        callback(CredentialsResult::Success(password));
      });
    });
  });

}

void CredentialsBackendKWallet::SavePassword(const QString &service, const QString &password, const ResultCallback &callback) {

  Open([this, service, password, callback](const bool open_success, const QString &open_error) {
    if (!open_success) {
      callback(CredentialsResult::Error(open_error));
      return;
    }
    HasFolder([this, service, password, callback](const bool success, const bool has_folder, const QString &error) {
      if (!success) {
        callback(CredentialsResult::Error(error));
        return;
      }
      if (has_folder) {
        WritePassword(service, password, callback);
        return;
      }
      CreateFolder([this, service, password, callback](const bool create_success, const QString &create_error) {
        if (!create_success) {
          callback(CredentialsResult::Error(create_error));
          return;
        }
        WritePassword(service, password, callback);
      });
    });
  });

}

void CredentialsBackendKWallet::DeletePassword(const QString &service, const ResultCallback &callback) {

  Open([this, service, callback](const bool open_success, const QString &open_error) {
    if (!open_success) {
      callback(CredentialsResult::Error(open_error));
      return;
    }
    HasEntry(service, [this, service, callback](const bool success, const bool has_entry, const QString &error) {
      if (!success) {
        callback(CredentialsResult::Error(error));
        return;
      }
      if (!has_entry) {
        callback(CredentialsResult::Success());
        return;
      }
      Call(u"removeEntry"_s, QVariantList() << handle_ << QLatin1String(kFolder) << service << QLatin1String(kAppId), [callback](const QDBusMessage &reply) {
        int result = -1;
        QString remove_error;
        if (!ReplyValue(reply, result, remove_error)) {
          callback(CredentialsResult::Error(remove_error));
          return;
        }
        if (result != 0) {
          callback(CredentialsResult::Error(QStringLiteral("Failed to remove password from wallet, error code %1.").arg(result)));
          return;
        }
        callback(CredentialsResult::Success());
      });
    });
  });

}
