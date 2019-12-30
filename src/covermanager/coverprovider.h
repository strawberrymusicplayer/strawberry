/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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


#include <QObject>
#include <QList>
#include <QString>

#include "albumcoverfetcher.h"

class Application;

// Each implementation of this interface downloads covers from one online service.
// There are no limitations on what this service might be - last.fm, Amazon, Google Images - you name it.
class CoverProvider : public QObject {
  Q_OBJECT

 public:
  explicit CoverProvider(const QString &name, const float &quality, const bool &fetchall, Application *app, QObject *parent);

  // A name (very short description) of this provider, like "last.fm".
  QString name() const { return name_; }
  bool quality() const { return quality_; }
  bool fetchall() const { return fetchall_; }

  // Starts searching for covers matching the given query text.
  // Returns true if the query has been started, or false if an error occurred.
  // The provider should remember the ID and emit it along with the result when it finishes.
  virtual bool StartSearch(const QString &artist, const QString &album, int id) = 0;

  virtual void CancelSearch(int id) { Q_UNUSED(id); }

 signals:
  void SearchFinished(int id, const CoverSearchResults& results);

 private:
  Application *app_;
  QString name_;
  float quality_;
  bool fetchall_;

};

#endif // COVERPROVIDER_H
