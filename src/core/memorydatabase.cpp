/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "memorydatabase.h"

using namespace Qt::Literals::StringLiterals;

MemoryDatabase::MemoryDatabase(SharedPtr<TaskManager> task_manager, QObject *parent)
      : Database(task_manager, parent, u":memory:"_s) {}

MemoryDatabase::~MemoryDatabase() {
  // Make sure Qt doesn't reuse the same database
  Close();
}
