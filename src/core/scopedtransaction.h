/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2016-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SCOPEDTRANSACTION_H
#define SCOPEDTRANSACTION_H

#include "config.h"

#include <QtGlobal>

class QSqlDatabase;

// Opens a transaction on a database.
// Rolls back the transaction if the object goes out of scope before Commit() is called.
class ScopedTransaction {
 public:
  explicit ScopedTransaction(QSqlDatabase *db);
  ~ScopedTransaction();

  // Returns false if the transaction could not be started, so the changes would not be made in a transaction.
  bool started() const { return started_; }

  // Returns false if the transaction was not started here, or could not be committed, in which case it is rolled back.
  bool Commit();

 private:
  QSqlDatabase *db_;
  bool started_;
  bool pending_;

  Q_DISABLE_COPY(ScopedTransaction)
};

#endif  // SCOPEDTRANSACTION_H
