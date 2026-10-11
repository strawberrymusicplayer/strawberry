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

#include "config.h"

#include <QSqlDatabase>
#include <QSqlError>

#include "core/logging.h"
#include "scopedtransaction.h"

ScopedTransaction::ScopedTransaction(QSqlDatabase *db) : db_(db), started_(false), pending_(false) {

  started_ = db->transaction();
  if (!started_) {
    qLog(Error) << "Could not start transaction:" << db->lastError().text();
  }

  // Only commit or roll back a transaction started here, not one started by someone else on the same connection.
  pending_ = started_;

}

ScopedTransaction::~ScopedTransaction() {

  if (pending_) {
    qLog(Warning) << "Rolling back transaction";
    db_->rollback();
  }

}

bool ScopedTransaction::Commit() {

  if (!started_) {
    return false;
  }

  if (!pending_) {
    qLog(Warning) << "Tried to commit a ScopedTransaction twice";
    return false;
  }

  pending_ = false;

  if (!db_->commit()) {
    qLog(Error) << "Could not commit transaction:" << db_->lastError().text();
    db_->rollback();
    return false;
  }

  return true;

}
