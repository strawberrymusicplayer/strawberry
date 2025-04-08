/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COVERPROVIDER_H
#define COVERPROVIDER_H

#include "config.h"

#include <QVariant>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"
#include "albumcoverfetcher.h"

class NetworkAccessManager;

class CoverProvider : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit CoverProvider(const QString &name, const bool enabled, const bool authentication_required, const float quality, const bool batch, const bool allow_missing_album, const SharedPtr<NetworkAccessManager> network, QObject *parent);

  QString name() const { return name_; }
  bool enabled() const { return enabled_; }
  int order() const { return order_; }
  float quality() const { return quality_; }
  bool batch() const { return batch_; }
  bool allow_missing_album() const { return allow_missing_album_; }

  void set_enabled(const bool enabled) { enabled_ = enabled; }
  void set_order(const int order) { order_ = order; }

  virtual QString service_name() const override { return name_; }
  virtual bool authentication_required() const override { return authentication_required_; }
  virtual bool authenticated() const override { return true; }
  virtual bool use_authorization_header() const override { return false; }
  virtual QByteArray authorization_header() const override { return QByteArray(); }

  virtual void Authenticate() {}
  virtual void ClearSession() {}

  // Starts searching for covers matching the given query text.
  // Returns true if the query has been started, or false if an error occurred.
  // The provider should remember the ID and emit it along with the result when it finishes.
  virtual bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) = 0;
  virtual void CancelSearch(const int id) { Q_UNUSED(id); }

 Q_SIGNALS:
  void AuthenticationFinished(const bool success, const QString &error = QString());
  void AuthenticationSuccess();
  void AuthenticationFailure(const QString &error);
  void SearchResults(const int id, const CoverProviderSearchResults &results);
  void SearchFinished(const int id, const CoverProviderSearchResults &results);

 protected:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  const SharedPtr<NetworkAccessManager> network_;
  const QString name_;
  bool enabled_;
  int order_;
  const bool authentication_required_;
  const float quality_;
  const bool batch_;
  const bool allow_missing_album_;
};

#endif  // COVERPROVIDER_H
