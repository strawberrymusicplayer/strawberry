/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QPair>
#include <QList>
#include <QString>

#include "lyricsfetcher.h"

class LyricsProvider : public QObject {
  Q_OBJECT

 public:
  explicit LyricsProvider(const QString &name, const bool enabled, const bool authentication_required, QObject *parent);

  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  QString name() const { return name_; }
  bool is_enabled() const { return enabled_; }
  int order() const { return order_; }

  void set_enabled(const bool enabled) { enabled_ = enabled; }
  void set_order(const int order) { order_ = order; }

  virtual bool StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) = 0;
  virtual void CancelSearch(const quint64 id) { Q_UNUSED(id); }
  virtual bool AuthenticationRequired() { return authentication_required_; }
  virtual void Authenticate() {}
  virtual bool IsAuthenticated() { return !authentication_required_; }
  virtual void Deauthenticate() {}

 signals:
  void AuthenticationComplete(bool, QStringList = QStringList());
  void AuthenticationSuccess();
  void AuthenticationFailure(QStringList);
  void SearchFinished(const quint64 id, const LyricsSearchResults &results);

 private:
  QString name_;
  bool enabled_;
  int order_;
  bool authentication_required_;
};

#endif // LYRICSPROVIDER_H
