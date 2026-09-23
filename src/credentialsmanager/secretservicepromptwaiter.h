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

#ifndef SECRETSERVICEPROMPTWAITER_H
#define SECRETSERVICEPROMPTWAITER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QDBusObjectPath>
#include <QDBusVariant>

class QTimer;

// Shows a org.freedesktop.Secret.Prompt and waits asynchronously for its Completed signal.
// Finished is emitted once, and the object deletes itself afterwards.
class SecretServicePromptWaiter : public QObject {
  Q_OBJECT

 public:
  explicit SecretServicePromptWaiter(const QString &service_name, const QDBusObjectPath &prompt, QObject *parent = nullptr);
  ~SecretServicePromptWaiter() override;

  void Start();

 Q_SIGNALS:
  void Finished(const bool success, const QVariant &result, const QString &error);

 private Q_SLOTS:
  void Completed(const bool dismissed, const QDBusVariant &result);
  void Timeout();

 private:
  void Finish(const bool success, const QVariant &result, const QString &error);

 private:
  const QString service_name_;
  const QDBusObjectPath prompt_;
  QTimer *timer_;
  bool connected_;
  bool finished_;
};

#endif  // SECRETSERVICEPROMPTWAITER_H
