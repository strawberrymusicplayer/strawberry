/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#ifndef WAVEFORMSETTINGS_H
#define WAVEFORMSETTINGS_H

namespace WaveformSettings {

constexpr char kSettingsGroup[] = "Waveform";

// The waveform on/off toggle is a runtime-only choice (slider context menu) and
// is not persisted yet. The settings work will add kEnabled / kShow keys here
// and hook them to a settings dialog.

}  // namespace WaveformSettings

#endif  // WAVEFORMSETTINGS_H
