/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#ifndef RADIOBROWSERSETTINGS_H
#define RADIOBROWSERSETTINGS_H

namespace RadioBrowserSettings {

constexpr char kSettingsGroup[] = "RadioBrowser";
constexpr char kServerUrl[] = "server_url";
constexpr char kSearchLimit[] = "search_limit";
constexpr int kSearchLimitDefault = 100;
constexpr char kHideBroken[] = "hide_broken";
constexpr bool kHideBrokenDefault = true;

}  // namespace RadioBrowserSettings

#endif  // RADIOBROWSERSETTINGS_H
