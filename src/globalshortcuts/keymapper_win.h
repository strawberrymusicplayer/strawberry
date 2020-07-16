/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QMap>
#include <qt_windows.h>

namespace KeyMapperWin {
static const QMap<Qt::Key, quint32> keymapper_win_ = {

    { Qt::Key_0,                  quint32(Qt::Key_0) },
    { Qt::Key_1,                  quint32(Qt::Key_1) },
    { Qt::Key_2,                  quint32(Qt::Key_2) },
    { Qt::Key_3,                  quint32(Qt::Key_3) },
    { Qt::Key_4,                  quint32(Qt::Key_4) },
    { Qt::Key_5,                  quint32(Qt::Key_5) },
    { Qt::Key_6,                  quint32(Qt::Key_6) },
    { Qt::Key_7,                  quint32(Qt::Key_7) },
    { Qt::Key_8,                  quint32(Qt::Key_8) },
    { Qt::Key_9,                  quint32(Qt::Key_9) },

    { Qt::Key_A,                  quint32(Qt::Key_A) },
    { Qt::Key_B,                  quint32(Qt::Key_B) },
    { Qt::Key_C,                  quint32(Qt::Key_C) },
    { Qt::Key_D,                  quint32(Qt::Key_D) },
    { Qt::Key_E,                  quint32(Qt::Key_E) },
    { Qt::Key_F,                  quint32(Qt::Key_F) },
    { Qt::Key_G,                  quint32(Qt::Key_G) },
    { Qt::Key_H,                  quint32(Qt::Key_H) },
    { Qt::Key_I,                  quint32(Qt::Key_I) },
    { Qt::Key_J,                  quint32(Qt::Key_J) },
    { Qt::Key_K,                  quint32(Qt::Key_K) },
    { Qt::Key_L,                  quint32(Qt::Key_L) },
    { Qt::Key_M,                  quint32(Qt::Key_M) },
    { Qt::Key_N,                  quint32(Qt::Key_N) },
    { Qt::Key_O,                  quint32(Qt::Key_O) },
    { Qt::Key_P,                  quint32(Qt::Key_P) },
    { Qt::Key_Q,                  quint32(Qt::Key_Q) },
    { Qt::Key_R,                  quint32(Qt::Key_R) },
    { Qt::Key_S,                  quint32(Qt::Key_S) },
    { Qt::Key_T,                  quint32(Qt::Key_T) },
    { Qt::Key_U,                  quint32(Qt::Key_U) },
    { Qt::Key_V,                  quint32(Qt::Key_V) },
    { Qt::Key_W,                  quint32(Qt::Key_W) },
    { Qt::Key_X,                  quint32(Qt::Key_X) },
    { Qt::Key_Y,                  quint32(Qt::Key_Y) },
    { Qt::Key_Z,                  quint32(Qt::Key_Z) },

    { Qt::Key_F1,                  VK_F1 },
    { Qt::Key_F2,                  VK_F2 },
    { Qt::Key_F3,                  VK_F3 },
    { Qt::Key_F4,                  VK_F4 },
    { Qt::Key_F5,                  VK_F5 },
    { Qt::Key_F6,                  VK_F6 },
    { Qt::Key_F7,                  VK_F7 },
    { Qt::Key_F8,                  VK_F8 },
    { Qt::Key_F9,                  VK_F9 },
    { Qt::Key_F10,                 VK_F10 },
    { Qt::Key_F11,                 VK_F11 },
    { Qt::Key_F12,                 VK_F12 },
    { Qt::Key_F13,                 VK_F13 },
    { Qt::Key_F14,                 VK_F14 },
    { Qt::Key_F15,                 VK_F15 },
    { Qt::Key_F16,                 VK_F16 },
    { Qt::Key_F17,                 VK_F17 },
    { Qt::Key_F18,                 VK_F18 },
    { Qt::Key_F19,                 VK_F19 },
    { Qt::Key_F20,                 VK_F20 },
    { Qt::Key_F21,                 VK_F21 },
    { Qt::Key_F22,                 VK_F22 },
    { Qt::Key_F23,                 VK_F23 },
    { Qt::Key_F24,                 VK_F24 },

    { Qt::Key_Escape,             VK_ESCAPE },
    { Qt::Key_Tab,                VK_TAB },
    { Qt::Key_Backtab,            VK_TAB },
    { Qt::Key_Backspace,          VK_BACK },
    { Qt::Key_Return,             VK_RETURN },
    { Qt::Key_Enter,              VK_RETURN },
    { Qt::Key_Insert,             VK_INSERT },
    { Qt::Key_Delete,             VK_DELETE },
    { Qt::Key_Pause,              VK_PAUSE },
    { Qt::Key_Print,              VK_PRINT },
    { Qt::Key_Clear,              VK_CLEAR },
    { Qt::Key_Home,               VK_HOME },
    { Qt::Key_End,                VK_END },
    { Qt::Key_Left,               VK_LEFT },
    { Qt::Key_Up,                 VK_UP },
    { Qt::Key_Right,              VK_RIGHT },
    { Qt::Key_Down,               VK_DOWN },
    { Qt::Key_PageUp,             VK_PRIOR },
    { Qt::Key_PageDown,           VK_NEXT },
    { Qt::Key_Space,              VK_SPACE },
    { Qt::Key_Back,               VK_BACK },
    { Qt::Key_Asterisk,           VK_MULTIPLY },
    { Qt::Key_Plus,               VK_ADD },
    { Qt::Key_Minus,              VK_SUBTRACT },
    { Qt::Key_Comma,              VK_SEPARATOR },
    { Qt::Key_Slash,              VK_DIVIDE },

    { Qt::Key_VolumeDown,         VK_VOLUME_DOWN },
    { Qt::Key_VolumeMute,         VK_VOLUME_MUTE },
    { Qt::Key_VolumeUp,           VK_VOLUME_UP },
    { Qt::Key_MediaPlay,          VK_MEDIA_PLAY_PAUSE },
    { Qt::Key_MediaStop,          VK_MEDIA_STOP },
    { Qt::Key_MediaPrevious,      VK_MEDIA_PREV_TRACK },
    { Qt::Key_MediaNext,          VK_MEDIA_NEXT_TRACK },
    { Qt::Key_LaunchMail,         VK_LAUNCH_MAIL },
    { Qt::Key_LaunchMedia,        VK_LAUNCH_MEDIA_SELECT },
    { Qt::Key_Launch0,            VK_LAUNCH_APP1 },
    { Qt::Key_Launch1,            VK_LAUNCH_APP2 },

    {Qt::Key(0),                  0}

};
}  // namespace
