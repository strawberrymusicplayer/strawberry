/*
 * Strawberry Music Player
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

#ifndef EQUALIZERJSON_H
#define EQUALIZERJSON_H

#include "config.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "equalizer.h"

// Versioned JSON file format for exporting and importing the equalizer settings.
// Gains, the preamp and the stereo balance use the stored slider values from -100 to 100, so a round trip is exact.
namespace EqualizerJson {

constexpr char kFormat[] = "strawberry-equalizer";
constexpr int kVersion = 1;
// Larger files are rejected before parsing.
constexpr qint64 kMaximumFileSize = 1024 * 1024;

struct State {
  QMap<QString, Equalizer::Params> presets;
  QString selected_preset;
  Equalizer::Params current;
  bool enabled = false;
  bool stereo_balancer_enabled = false;
  int stereo_balance = 0;
};

QByteArray ToJson(const State &state);

// Validates the complete document; on failure returns false, sets the translated error and leaves state unchanged.
bool FromJson(const QByteArray &json, State *state, QString *error);

// Returns the names of imported presets that exist with a different curve and would be overwritten by merging.
QStringList ConflictingPresets(const QMap<QString, Equalizer::Params> &existing, const QMap<QString, Equalizer::Params> &imported);

}  // namespace EqualizerJson

#endif  // EQUALIZERJSON_H
