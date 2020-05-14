/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "albumcoverfetcher.h"

class Application;

// Each implementation of this interface downloads covers from one online service.
// There are no limitations on what this service might be - last.fm, Amazon, Google Images - you name it.
class CoverProvider : public QObject {
  Q_OBJECT

 public:
  explicit CoverProvider(const QString &name, const bool enabled, const bool authentication_required, const float quality, const bool fetchall, const bool allow_missing_album, Application *app, QObject *parent);

  // A name (very short description) of this provider, like "last.fm".
  QString name() const { return name_; }
  bool is_enabled() const { return enabled_; }
  int order() const { return order_; }
  bool quality() const { return quality_; }
  bool fetchall() const { return fetchall_; }
  bool allow_missing_album() const { return allow_missing_album_; }

  void set_enabled(const bool enabled) { enabled_ = enabled; }
  void set_order(const int order) { order_ = order; }

  bool AuthenticationRequired() const { return authentication_required_; }
  virtual bool IsAuthenticated() const { return true; }
  virtual void Authenticate() {}
  virtual void Deauthenticate() {}

  // Starts searching for covers matching the given query text.
  // Returns true if the query has been started, or false if an error occurred.
  // The provider should remember the ID and emit it along with the result when it finishes.
  virtual bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) = 0;
  virtual void CancelSearch(const int id) { Q_UNUSED(id); }

  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

 signals:
  void AuthenticationComplete(bool, QStringList = QStringList());
  void AuthenticationSuccess();
  void AuthenticationFailure(QStringList);
  void SearchResults(int, CoverSearchResults);
  void SearchFinished(int, CoverSearchResults);

 private:
  Application *app_;
  QString name_;
  bool enabled_;
  int order_;
  bool authentication_required_;
  float quality_;
  bool fetchall_;
  bool allow_missing_album_;

};

#endif // COVERPROVIDER_H
