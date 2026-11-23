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

#ifndef SETTINGSMIGRATION_H
#define SETTINGSMIGRATION_H

#include <QList>

namespace SettingsMigration {

struct RenamedKey {
  const char *group;
  const char *old_key;
  const char *new_key;
};

// Updates settings saved by previous versions: renamed keys are moved to their new keys and keys which are not used anymore are removed.
// It must be called before the settings are read, settings which have been migrated are not migrated again.
void Migrate();

// Returns the keys which are moved to new keys, the tests check them against the keys read by the code.
QList<RenamedKey> RenamedKeys();

}  // namespace SettingsMigration

#endif  // SETTINGSMIGRATION_H
