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

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/httpbaserequest.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class LyricsProvider : public HttpBaseRequest {
  Q_OBJECT

 public:
  explicit LyricsProvider(const QString &name, const bool enabled, const bool authentication_required, const SharedPtr<NetworkAccessManager> network, QObject *parent);

  QString name() const { return name_; }
  bool is_enabled() const { return enabled_; }
  int order() const { return order_; }

  void set_enabled(const bool enabled) { enabled_ = enabled; }
  void set_order(const int order) { order_ = order; }

  virtual QString service_name() const override { return name_; }
  virtual bool authentication_required() const override { return authentication_required_; }
  virtual bool authenticated() const override { return false; }
  virtual bool use_authorization_header() const override { return authentication_required_; }
  virtual QByteArray authorization_header() const override { return QByteArray(); }

  virtual bool StartSearchAsync(const int id, const LyricsSearchRequest &request);
  virtual void CancelSearchAsync(const int id) { Q_UNUSED(id); }
  virtual void Authenticate() {}
  virtual void ClearSession() {}

 protected Q_SLOTS:
  virtual void StartSearch(const int id, const LyricsSearchRequest &request) = 0;

 Q_SIGNALS:
  void AuthenticationComplete(const bool success, const QString &error = QString());
  void AuthenticationSuccess();
  void AuthenticationFailure(const QString &error);
  void SearchFinished(const int id, const LyricsSearchResults &results = LyricsSearchResults());

 protected:
  const SharedPtr<NetworkAccessManager> network_;
  const QString name_;
  bool enabled_;
  int order_;
  const bool authentication_required_;
};

#endif  // LYRICSPROVIDER_H
