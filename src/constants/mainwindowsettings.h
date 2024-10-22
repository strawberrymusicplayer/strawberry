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

#ifndef MAINWINDOWSETTINGS_H
#define MAINWINDOWSETTINGS_H

namespace MainWindowSettings {

constexpr char kSettingsGroup[] = "MainWindow";
constexpr char kSearchForCoverAuto[] = "search_for_cover_auto";
constexpr char kShowSidebar[] = "show_sidebar";
constexpr char kMaximized[] = "maximized";
constexpr char kMinimized[] = "minimized";
constexpr char kHidden[] = "hidden";
constexpr char kGeometry[] = "geometry";
constexpr char kSplitterState[] = "splitter_state";
constexpr char kDoNotShowSponsorMessage[] = "do_not_show_sponsor_message";

} // namespace

#endif // MAINWINDOWSETTINGS_H
