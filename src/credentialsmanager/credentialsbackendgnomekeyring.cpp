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

#include <QObject>
#include <QList>
#include <QMap>
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>

#include "credentialsbackendgnomekeyring.h"
#include "credentialsdbusutils.h"
#include "credentialsresult.h"
#include "secretservicepromptwaiter.h"
#include "secretservicesecret.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kServiceName[] = "org.freedesktop.secrets";
constexpr char kServicePath[] = "/org/freedesktop/secrets";
constexpr char kServiceInterface[] = "org.freedesktop.Secret.Service";
constexpr char kCollectionInterface[] = "org.freedesktop.Secret.Collection";
constexpr char kItemInterface[] = "org.freedesktop.Secret.Item";
constexpr char kSessionInterface[] = "org.freedesktop.Secret.Session";

// Same schema name and attributes as the libsecret backend.
constexpr char kSchemaName[] = "org.strawberrymusicplayer.Password";
constexpr char kApplication[] = "strawberry";

// A path of "/" means no object, for example no prompt is needed.
bool IsNullPath(const QDBusObjectPath &path) {
  return path.path().isEmpty() || path.path() == "/"_L1;
}

}  // namespace

CredentialsBackendGnomeKeyring::CredentialsBackendGnomeKeyring(QObject *parent) : CredentialsBackendInterface(parent) {

  qDBusRegisterMetaType<SecretServiceSecret>();
  qDBusRegisterMetaType<SecretServiceAttributes>();

}

CredentialsBackendGnomeKeyring::~CredentialsBackendGnomeKeyring() {

  if (!IsNullPath(session_)) {
    const QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(kServiceName), session_.path(), QLatin1String(kSessionInterface), u"Close"_s);
    QDBusConnection::sessionBus().send(message);
  }

}

QString CredentialsBackendGnomeKeyring::name() const {
  return u"GNOME Keyring"_s;
}

void CredentialsBackendGnomeKeyring::Call(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments, const CredentialsDBusUtils::ReplyCallback &callback) {

  QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(kServiceName), path, interface, method);
  message.setArguments(arguments);

  CredentialsDBusUtils::AsyncCall(this, message, callback);

}

SecretServiceAttributes CredentialsBackendGnomeKeyring::Attributes(const QString &service) {

  SecretServiceAttributes attributes;
  attributes.insert(u"xdg:schema"_s, QLatin1String(kSchemaName));
  attributes.insert(u"application"_s, QLatin1String(kApplication));
  attributes.insert(u"service"_s, service);

  return attributes;

}

void CredentialsBackendGnomeKeyring::CheckAvailable(const AvailableCallback &available_callback) {

  CredentialsDBusUtils::CheckServiceAvailable(this, QLatin1String(kServiceName), [this, available_callback](const bool available) {
    if (!available) {
      available_callback(false);
      return;
    }
    OpenSession([available_callback](const bool success, const QString &error) {
      Q_UNUSED(error)
      available_callback(success);
    });
  });

}

void CredentialsBackendGnomeKeyring::OpenSession(const SuccessCallback &callback) {

  if (!IsNullPath(session_)) {
    callback(true, QString());
    return;
  }

  // The "plain" algorithm sends the secret unencrypted over the session bus, which is only accessible by the current user.
  Call(QLatin1String(kServicePath), QLatin1String(kServiceInterface), u"OpenSession"_s, QVariantList() << u"plain"_s << QVariant::fromValue(QDBusVariant(QString())), [this, callback](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 2, error)) {
      callback(false, error);
      return;
    }
    const QDBusObjectPath session = qvariant_cast<QDBusObjectPath>(reply.arguments().at(1));
    if (IsNullPath(session)) {
      callback(false, u"Failed to open Secret Service session."_s);
      return;
    }
    session_ = session;
    callback(true, QString());
  });

}

void CredentialsBackendGnomeKeyring::RunPrompt(const QDBusObjectPath &prompt, const PromptCallback &callback) {

  if (IsNullPath(prompt)) {
    callback(true, QVariant(), QString());
    return;
  }

  SecretServicePromptWaiter *waiter = new SecretServicePromptWaiter(QLatin1String(kServiceName), prompt, this);
  QObject::connect(waiter, &SecretServicePromptWaiter::Finished, this, [callback](const bool success, const QVariant &result, const QString &error) {
    callback(success, result, error);
  });
  waiter->Start();

}

void CredentialsBackendGnomeKeyring::Unlock(const QList<QDBusObjectPath> &objects, const PathsCallback &callback) {

  Call(QLatin1String(kServicePath), QLatin1String(kServiceInterface), u"Unlock"_s, QVariantList() << QVariant::fromValue(objects), [this, callback](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 2, error)) {
      callback(false, QList<QDBusObjectPath>(), error);
      return;
    }
    const QList<QDBusObjectPath> unlocked = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(0));
    RunPrompt(qvariant_cast<QDBusObjectPath>(reply.arguments().at(1)), [unlocked, callback](const bool success, const QVariant &result, const QString &prompt_error) {
      if (!success) {
        callback(false, QList<QDBusObjectPath>(), prompt_error);
        return;
      }
      QList<QDBusObjectPath> all_unlocked = unlocked;
      if (result.isValid()) {
        all_unlocked << qdbus_cast<QList<QDBusObjectPath>>(result);
      }
      callback(true, all_unlocked, QString());
    });
  });

}

void CredentialsBackendGnomeKeyring::SearchItems(const QString &service, const PathsCallback &callback) {

  Call(QLatin1String(kServicePath), QLatin1String(kServiceInterface), u"SearchItems"_s, QVariantList() << QVariant::fromValue(Attributes(service)), [this, callback](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 2, error)) {
      callback(false, QList<QDBusObjectPath>(), error);
      return;
    }
    const QList<QDBusObjectPath> items = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(0));
    const QList<QDBusObjectPath> locked = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(1));
    if (locked.isEmpty()) {
      callback(true, items, QString());
      return;
    }
    Unlock(locked, [items, callback](const bool success, const QList<QDBusObjectPath> &unlocked, const QString &unlock_error) {
      if (!success) {
        callback(false, QList<QDBusObjectPath>(), unlock_error);
        return;
      }
      callback(true, items + unlocked, QString());
    });
  });

}

void CredentialsBackendGnomeKeyring::DefaultCollection(const PathCallback &callback) {

  Call(QLatin1String(kServicePath), QLatin1String(kServiceInterface), u"ReadAlias"_s, QVariantList() << u"default"_s, [this, callback](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 1, error)) {
      callback(false, QDBusObjectPath(), error);
      return;
    }
    const QDBusObjectPath collection = qvariant_cast<QDBusObjectPath>(reply.arguments().at(0));
    if (!IsNullPath(collection)) {
      callback(true, collection, QString());
      return;
    }
    // There is no default collection, create one.
    QVariantMap properties;
    properties.insert(u"org.freedesktop.Secret.Collection.Label"_s, u"Default keyring"_s);
    Call(QLatin1String(kServicePath), QLatin1String(kServiceInterface), u"CreateCollection"_s, QVariantList() << properties << u"default"_s, [this, callback](const QDBusMessage &create_reply) {
      QString create_error;
      if (!CredentialsDBusUtils::CheckReply(create_reply, 2, create_error)) {
        callback(false, QDBusObjectPath(), create_error);
        return;
      }
      const QDBusObjectPath created_collection = qvariant_cast<QDBusObjectPath>(create_reply.arguments().at(0));
      if (!IsNullPath(created_collection)) {
        callback(true, created_collection, QString());
        return;
      }
      RunPrompt(qvariant_cast<QDBusObjectPath>(create_reply.arguments().at(1)), [callback](const bool success, const QVariant &result, const QString &prompt_error) {
        const QDBusObjectPath prompted_collection = success ? qdbus_cast<QDBusObjectPath>(result) : QDBusObjectPath();
        if (IsNullPath(prompted_collection)) {
          callback(false, QDBusObjectPath(), success ? u"Failed to create default Secret Service collection."_s : prompt_error);
          return;
        }
        callback(true, prompted_collection, QString());
      });
    });
  });

}

void CredentialsBackendGnomeKeyring::CreateItem(const QDBusObjectPath &collection, const QString &service, const QString &password, const ResultCallback &callback) {

  QVariantMap properties;
  properties.insert(u"org.freedesktop.Secret.Item.Label"_s, QStringLiteral("Strawberry %1 password").arg(service));
  properties.insert(u"org.freedesktop.Secret.Item.Attributes"_s, QVariant::fromValue(Attributes(service)));

  SecretServiceSecret secret;
  secret.session = session_;
  secret.value = password.toUtf8();
  secret.content_type = u"text/plain"_s;

  Call(collection.path(), QLatin1String(kCollectionInterface), u"CreateItem"_s, QVariantList() << properties << QVariant::fromValue(secret) << true, [this, callback](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 2, error)) {
      // The session might be invalid if the keyring daemon was restarted, open a new session next time.
      session_ = QDBusObjectPath();
      callback(CredentialsResult::Error(error));
      return;
    }
    RunPrompt(qvariant_cast<QDBusObjectPath>(reply.arguments().at(1)), [callback](const bool success, const QVariant &result, const QString &prompt_error) {
      Q_UNUSED(result)
      callback(success ? CredentialsResult::Success() : CredentialsResult::Error(prompt_error));
    });
  });

}

void CredentialsBackendGnomeKeyring::DeleteItems(const QList<QDBusObjectPath> &items, const ResultCallback &callback) {

  if (items.isEmpty()) {
    callback(CredentialsResult::Success());
    return;
  }

  const QList<QDBusObjectPath> remaining_items = items.mid(1);
  Call(items.first().path(), QLatin1String(kItemInterface), u"Delete"_s, QVariantList(), [this, remaining_items, callback](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 1, error)) {
      callback(CredentialsResult::Error(error));
      return;
    }
    RunPrompt(qvariant_cast<QDBusObjectPath>(reply.arguments().at(0)), [this, remaining_items, callback](const bool success, const QVariant &result, const QString &prompt_error) {
      Q_UNUSED(result)
      if (!success) {
        callback(CredentialsResult::Error(prompt_error));
        return;
      }
      DeleteItems(remaining_items, callback);
    });
  });

}

void CredentialsBackendGnomeKeyring::ReadPassword(const QString &service, const ResultCallback &callback) {

  OpenSession([this, service, callback](const bool session_success, const QString &session_error) {
    if (!session_success) {
      callback(CredentialsResult::Error(session_error));
      return;
    }
    SearchItems(service, [this, callback](const bool success, const QList<QDBusObjectPath> &items, const QString &error) {
      if (!success) {
        callback(CredentialsResult::Error(error));
        return;
      }
      if (items.isEmpty()) {
        callback(CredentialsResult::NotFound());
        return;
      }
      Call(items.first().path(), QLatin1String(kItemInterface), u"GetSecret"_s, QVariantList() << QVariant::fromValue(session_), [this, callback](const QDBusMessage &reply) {
        QString secret_error;
        if (!CredentialsDBusUtils::CheckReply(reply, 1, secret_error)) {
          // The session might be invalid if the keyring daemon was restarted, open a new session next time.
          session_ = QDBusObjectPath();
          callback(CredentialsResult::Error(secret_error));
          return;
        }
        const SecretServiceSecret secret = qdbus_cast<SecretServiceSecret>(reply.arguments().at(0));
        callback(CredentialsResult::Success(QString::fromUtf8(secret.value)));
      });
    });
  });

}

void CredentialsBackendGnomeKeyring::SavePassword(const QString &service, const QString &password, const ResultCallback &callback) {

  OpenSession([this, service, password, callback](const bool session_success, const QString &session_error) {
    if (!session_success) {
      callback(CredentialsResult::Error(session_error));
      return;
    }
    DefaultCollection([this, service, password, callback](const bool success, const QDBusObjectPath &collection, const QString &error) {
      if (!success) {
        callback(CredentialsResult::Error(error));
        return;
      }
      // Unlocking an already unlocked collection finishes without a prompt.
      Unlock(QList<QDBusObjectPath>() << collection, [this, collection, service, password, callback](const bool unlock_success, const QList<QDBusObjectPath> &unlocked, const QString &unlock_error) {
        if (!unlock_success) {
          callback(CredentialsResult::Error(unlock_error));
          return;
        }
        if (!unlocked.contains(collection)) {
          callback(CredentialsResult::Error(u"Failed to unlock Secret Service collection."_s));
          return;
        }
        CreateItem(collection, service, password, callback);
      });
    });
  });

}

void CredentialsBackendGnomeKeyring::DeletePassword(const QString &service, const ResultCallback &callback) {

  SearchItems(service, [this, callback](const bool success, const QList<QDBusObjectPath> &items, const QString &error) {
    if (!success) {
      callback(CredentialsResult::Error(error));
      return;
    }
    DeleteItems(items, callback);
  });

}
