/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LYRICSPROVIDER_H
#define LYRICSPROVIDER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QRegularExpression>

#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class LyricsProvider : public QObject {
  Q_OBJECT

 public:
  explicit LyricsProvider(const QString &name, const bool enabled, const bool authentication_required, SharedPtr<NetworkAccessManager> network, QObject *parent);

  QString name() const { return name_; }
  bool is_enabled() const { return enabled_; }
  int order() const { return order_; }

  void set_enabled(const bool enabled) { enabled_ = enabled; }
  void set_order(const int order) { order_ = order; }

  virtual bool StartSearchAsync(const int id, const LyricsSearchRequest &request);
  virtual void CancelSearchAsync(const int id) { Q_UNUSED(id); }
  virtual bool AuthenticationRequired() const { return authentication_required_; }
  virtual void Authenticate() {}
  virtual bool IsAuthenticated() const { return !authentication_required_; }
  virtual void Deauthenticate() {}

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

 protected Q_SLOTS:
  virtual void StartSearch(const int id, const LyricsSearchRequest &request) = 0;

 Q_SIGNALS:
  void AuthenticationComplete(const bool success, const QStringList &errors = QStringList());
  void AuthenticationSuccess();
  void AuthenticationFailure(const QStringList &errors);
  void SearchFinished(const int id, const LyricsSearchResults &results = LyricsSearchResults());

 protected:
  SharedPtr<NetworkAccessManager> network_;
  const QString name_;
  bool enabled_;
  int order_;
  const bool authentication_required_;
};

#endif  // LYRICSPROVIDER_H
