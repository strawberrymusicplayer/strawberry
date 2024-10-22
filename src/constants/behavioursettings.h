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

#ifndef BEHAVIOURSETTINGS_H
#define BEHAVIOURSETTINGS_H

namespace BehaviourSettings {

constexpr char kSettingsGroup[] = "Behaviour";

enum class StartupBehaviour {
  Remember = 1,
  Show = 2,
  Hide = 3,
  ShowMaximized = 4,
  ShowMinimized = 5
};

enum class PlayBehaviour {
  Never = 1,
  IfStopped = 2,
  Always = 3
};

enum class PreviousBehaviour {
  DontRestart = 1,
  Restart = 2
};

enum class AddBehaviour {
  Append = 1,
  Enqueue = 2,
  Load = 3,
  OpenInNew = 4
};

enum class PlaylistAddBehaviour {
  Play = 1,
  Enqueue = 2
};

constexpr char kKeepRunning[] = "keeprunning";
constexpr char kShowTrayIcon[] = "showtrayicon";
constexpr char kTrayIconProgress[] = "trayicon_progress";
constexpr char kTaskbarProgress[] = "taskbar_progress";
constexpr char kResumePlayback[] = "resumeplayback";
constexpr char kPlayingWidget[] = "playing_widget";
constexpr char kStartupBehaviour[] = "startupbehaviour";
constexpr char kLanguage[] = "language";
constexpr char kMenuPlayMode[] = "menu_playmode";
constexpr char kMenuPreviousMode[] = "menu_previousmode";
constexpr char kDoubleClickAddMode[] = "doubleclick_addmode";
constexpr char kDoubleClickPlayMode[] = "doubleclick_playmode";
constexpr char kDoubleClickPlaylistAddMode[] = "doubleclick_playlist_addmode";
constexpr char kSeekStepSec[] = "seek_step_sec";
constexpr char kVolumeIncrement[] = "volume_increment";

}  // namespace

#endif  // BEHAVIOURSETTINGS_H
