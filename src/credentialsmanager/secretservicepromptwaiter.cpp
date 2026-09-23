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
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>

#include "secretservicepromptwaiter.h"
#include "credentialsdbusutils.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kPromptInterface[] = "org.freedesktop.Secret.Prompt";
// Prompts wait for user input, so give the user plenty of time.
constexpr int kPromptTimeout = 300000;
}  // namespace

SecretServicePromptWaiter::SecretServicePromptWaiter(const QString &service_name, const QDBusObjectPath &prompt, QObject *parent)
    : QObject(parent),
      service_name_(service_name),
      prompt_(prompt),
      timer_(new QTimer(this)),
      connected_(false),
      finished_(false) {

  timer_->setSingleShot(true);
  timer_->setInterval(kPromptTimeout);
  QObject::connect(timer_, &QTimer::timeout, this, &SecretServicePromptWaiter::Timeout);

}

SecretServicePromptWaiter::~SecretServicePromptWaiter() {

  if (connected_) {
    QDBusConnection::sessionBus().disconnect(service_name_, prompt_.path(), QLatin1String(kPromptInterface), u"Completed"_s, this, SLOT(Completed(bool,QDBusVariant)));
  }

}

void SecretServicePromptWaiter::Start() {

  connected_ = QDBusConnection::sessionBus().connect(service_name_, prompt_.path(), QLatin1String(kPromptInterface), u"Completed"_s, this, SLOT(Completed(bool,QDBusVariant)));
  if (!connected_) {
    Finish(false, QVariant(), u"Failed to connect to Secret Service prompt."_s);
    return;
  }

  timer_->start();

  QDBusMessage message = QDBusMessage::createMethodCall(service_name_, prompt_.path(), QLatin1String(kPromptInterface), u"Prompt"_s);
  message.setArguments(QVariantList() << QString());
  CredentialsDBusUtils::AsyncCall(this, message, [this](const QDBusMessage &reply) {
    QString error;
    if (!CredentialsDBusUtils::CheckReply(reply, 0, error)) {
      Finish(false, QVariant(), error);
    }
  });

}

void SecretServicePromptWaiter::Completed(const bool dismissed, const QDBusVariant &result) {

  if (dismissed) {
    Finish(false, QVariant(), u"Secret Service prompt was dismissed."_s);
  }
  else {
    Finish(true, result.variant(), QString());
  }

}

void SecretServicePromptWaiter::Timeout() {

  Finish(false, QVariant(), u"Timed out waiting for Secret Service prompt."_s);

}

void SecretServicePromptWaiter::Finish(const bool success, const QVariant &result, const QString &error) {

  if (finished_) return;
  finished_ = true;

  timer_->stop();

  Q_EMIT Finished(success, result, error);

  deleteLater();

}
