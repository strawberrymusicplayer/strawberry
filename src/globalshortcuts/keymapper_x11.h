/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef KEYMAPPER_X11_H
#define KEYMAPPER_X11_H

#include "config.h"

#include <QtGlobal>
#include <QMap>

#define XK_MISCELLANY
#define XK_XKB_KEYS
#define XK_LATIN1

#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>

namespace KeyMapperX11 {
static const QMap<Qt::Key, quint32> keymapper_x11_ = {  // clazy:exclude=non-pod-global-static

    { Qt::Key_0,                  XK_0 },
    { Qt::Key_1,                  XK_1 },
    { Qt::Key_2,                  XK_2 },
    { Qt::Key_3,                  XK_3 },
    { Qt::Key_4,                  XK_4 },
    { Qt::Key_5,                  XK_5 },
    { Qt::Key_6,                  XK_6 },
    { Qt::Key_7,                  XK_7 },
    { Qt::Key_8,                  XK_8 },
    { Qt::Key_9,                  XK_9 },

    { Qt::Key_A,                  XK_A },
    { Qt::Key_B,                  XK_B },
    { Qt::Key_C,                  XK_C },
    { Qt::Key_D,                  XK_D },
    { Qt::Key_E,                  XK_E },
    { Qt::Key_F,                  XK_F },
    { Qt::Key_G,                  XK_G },
    { Qt::Key_H,                  XK_H },
    { Qt::Key_I,                  XK_I },
    { Qt::Key_J,                  XK_J },
    { Qt::Key_K,                  XK_K },
    { Qt::Key_L,                  XK_L },
    { Qt::Key_M,                  XK_M },
    { Qt::Key_N,                  XK_N },
    { Qt::Key_O,                  XK_O },
    { Qt::Key_P,                  XK_P },
    { Qt::Key_Q,                  XK_Q },
    { Qt::Key_R,                  XK_R },
    { Qt::Key_S,                  XK_S },
    { Qt::Key_T,                  XK_T },
    { Qt::Key_U,                  XK_U },
    { Qt::Key_V,                  XK_V },
    { Qt::Key_W,                  XK_W },
    { Qt::Key_X,                  XK_X },
    { Qt::Key_Y,                  XK_Y },
    { Qt::Key_Z,                  XK_Z },

    { Qt::Key_Escape,             XK_Escape },
    { Qt::Key_Tab,                XK_Tab },
    { Qt::Key_Backtab,            XK_ISO_Left_Tab },
    { Qt::Key_Backspace,          XK_BackSpace },
    { Qt::Key_Return,             XK_Return },
    { Qt::Key_Enter,              XK_KP_Enter },
    { Qt::Key_Insert,             XK_Insert },
    { Qt::Key_Delete,             XK_Delete },
    { Qt::Key_Pause,              XK_Pause },
    { Qt::Key_Print,              XK_Print },
    { Qt::Key_Clear,              XK_Clear },
    { Qt::Key_Home,               XK_Home },
    { Qt::Key_End,                XK_End },
    { Qt::Key_Left,               XK_Left },
    { Qt::Key_Up,                 XK_Up },
    { Qt::Key_Right,              XK_Right },
    { Qt::Key_Down,               XK_Down },
    { Qt::Key_PageUp,             XK_Prior },
    { Qt::Key_PageDown,           XK_Next },
    { Qt::Key_Space,              XK_space },
    { Qt::Key_Exclam,             XK_exclam },
    { Qt::Key_QuoteDbl,           XK_quotedbl },
    { Qt::Key_NumberSign,         XK_numbersign },
    { Qt::Key_Dollar,             XK_dollar },
    { Qt::Key_Percent,            XK_percent },
    { Qt::Key_Ampersand,          XK_ampersand },
    { Qt::Key_Apostrophe,         XK_apostrophe },
    { Qt::Key_ParenLeft,          XK_parenleft },
    { Qt::Key_ParenRight,         XK_parenright },
    { Qt::Key_Asterisk,           XK_asterisk },
    { Qt::Key_Plus,               XK_plus },
    { Qt::Key_Comma,              XK_comma },
    { Qt::Key_Minus,              XK_minus },
    { Qt::Key_Period,             XK_period },
    { Qt::Key_Slash,              XK_slash },
    { Qt::Key_Colon,              XK_colon },
    { Qt::Key_Semicolon,          XK_semicolon },
    { Qt::Key_Less,               XK_less },
    { Qt::Key_Equal,              XK_equal },
    { Qt::Key_Greater,            XK_greater },
    { Qt::Key_Question,           XK_question },
    { Qt::Key_BracketLeft,        XK_bracketleft },
    { Qt::Key_Backslash,          XK_backslash },
    { Qt::Key_BracketRight,       XK_bracketright },
    { Qt::Key_AsciiCircum,        XK_asciicircum },
    { Qt::Key_Underscore,         XK_underscore },
    { Qt::Key_QuoteLeft,          XK_quoteleft },
    { Qt::Key_BraceLeft,          XK_braceleft },
    { Qt::Key_Bar,                XK_bar },
    { Qt::Key_BraceRight,         XK_braceright },
    { Qt::Key_AsciiTilde,         XK_asciitilde },
    { Qt::Key_nobreakspace,       XK_nobreakspace },
    { Qt::Key_exclamdown,         XK_exclamdown },
    { Qt::Key_cent,               XK_cent },
    { Qt::Key_sterling,           XK_sterling },
    { Qt::Key_currency,           XK_currency },
    { Qt::Key_yen,                XK_yen },
    { Qt::Key_brokenbar,          XK_brokenbar },
    { Qt::Key_section,            XK_section },
    { Qt::Key_diaeresis,          XK_diaeresis },
    { Qt::Key_copyright,          XK_copyright },
    { Qt::Key_ordfeminine,        XK_ordfeminine },
    { Qt::Key_guillemotleft,      XK_guillemotleft },
    { Qt::Key_notsign,            XK_notsign },
    { Qt::Key_hyphen,             XK_hyphen },
    { Qt::Key_registered,         XK_registered },
    { Qt::Key_macron,             XK_macron },
    { Qt::Key_degree,             XK_degree },
    { Qt::Key_plusminus,          XK_plusminus },
    { Qt::Key_twosuperior,        XK_twosuperior },
    { Qt::Key_threesuperior,      XK_threesuperior },
    { Qt::Key_acute,              XK_acute },
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    { Qt::Key_micro,              XK_mu },
#else
    { Qt::Key_mu,                 XK_mu },
#endif
    { Qt::Key_paragraph,          XK_paragraph },
    { Qt::Key_periodcentered,     XK_periodcentered },
    { Qt::Key_cedilla,            XK_cedilla },
    { Qt::Key_onesuperior,        XK_onesuperior },
    { Qt::Key_masculine,          XK_masculine },
    { Qt::Key_guillemotright,     XK_guillemotright },
    { Qt::Key_onequarter,         XK_onequarter },
    { Qt::Key_onehalf,            XK_onehalf },
    { Qt::Key_threequarters,      XK_threequarters },
    { Qt::Key_questiondown,       XK_questiondown },
    { Qt::Key_Agrave,             XK_Agrave },
    { Qt::Key_Aacute,             XK_Aacute },
    { Qt::Key_Acircumflex,        XK_Acircumflex },
    { Qt::Key_Atilde,             XK_Atilde },
    { Qt::Key_Adiaeresis,         XK_Adiaeresis },
    { Qt::Key_Aring,              XK_Aring },
    { Qt::Key_AE,                 XK_AE },
    { Qt::Key_Ccedilla,           XK_Ccedilla },
    { Qt::Key_Egrave,             XK_Egrave },
    { Qt::Key_Eacute,             XK_Eacute },
    { Qt::Key_Ecircumflex,        XK_Ecircumflex },
    { Qt::Key_Ediaeresis,         XK_Ediaeresis },
    { Qt::Key_Igrave,             XK_Igrave },
    { Qt::Key_Iacute,             XK_Iacute },
    { Qt::Key_Icircumflex,        XK_Icircumflex },
    { Qt::Key_Idiaeresis,         XK_Idiaeresis },
    { Qt::Key_ETH,                XK_ETH },
    { Qt::Key_Ntilde,             XK_Ntilde },
    { Qt::Key_Ograve,             XK_Ograve },
    { Qt::Key_Oacute,             XK_Oacute },
    { Qt::Key_Ocircumflex,        XK_Ocircumflex },
    { Qt::Key_Otilde,             XK_Otilde },
    { Qt::Key_Odiaeresis,         XK_Odiaeresis },
    { Qt::Key_multiply,           XK_multiply },
    { Qt::Key_Ooblique,           XK_Ooblique },
    { Qt::Key_Ugrave,             XK_Ugrave },
    { Qt::Key_Uacute,             XK_Uacute },
    { Qt::Key_Ucircumflex,        XK_Ucircumflex },
    { Qt::Key_Udiaeresis,         XK_Udiaeresis },
    { Qt::Key_Yacute,             XK_Yacute },
    { Qt::Key_THORN,              XK_THORN },
    { Qt::Key_ssharp,             XK_ssharp },
    { Qt::Key_division,           XK_division },
    { Qt::Key_ydiaeresis,         XK_ydiaeresis },
    { Qt::Key_Multi_key,          XK_Multi_key },
    { Qt::Key_Codeinput,          XK_Codeinput },
    { Qt::Key_SingleCandidate,    XK_SingleCandidate },
    { Qt::Key_MultipleCandidate,  XK_MultipleCandidate },
    { Qt::Key_PreviousCandidate,  XK_PreviousCandidate },
    { Qt::Key_Mode_switch,        XK_Mode_switch },

    { Qt::Key_Back,               XF86XK_Back },
    { Qt::Key_Forward,            XF86XK_Forward },
    { Qt::Key_Stop,               XF86XK_Stop },
    { Qt::Key_Refresh,            XF86XK_Refresh },
    { Qt::Key_VolumeDown,         XF86XK_AudioLowerVolume },
    { Qt::Key_VolumeMute,         XF86XK_AudioMute },
    { Qt::Key_VolumeUp,           XF86XK_AudioRaiseVolume },
    { Qt::Key_MediaPlay,          XF86XK_AudioPlay },
    { Qt::Key_MediaStop,          XF86XK_AudioStop },
    { Qt::Key_MediaPrevious,      XF86XK_AudioPrev },
    { Qt::Key_MediaNext,          XF86XK_AudioNext },
    { Qt::Key_MediaRecord,        XF86XK_AudioRecord },
    { Qt::Key_MediaPause,         XF86XK_AudioPause },
    { Qt::Key_HomePage,           XF86XK_HomePage },
    { Qt::Key_Favorites,          XF86XK_Favorites },
    { Qt::Key_Search,             XF86XK_Search },
    { Qt::Key_Standby,            XF86XK_Standby },
    { Qt::Key_OpenUrl,            XF86XK_OpenURL },
    { Qt::Key_LaunchMail,         XF86XK_Mail },
    { Qt::Key_LaunchMedia,        XF86XK_AudioMedia },
    { Qt::Key_Launch0,            XF86XK_MyComputer },
    { Qt::Key_Launch1,            XF86XK_Calculator },
    { Qt::Key_Launch2,            XF86XK_Launch0 },
    { Qt::Key_Launch3,            XF86XK_Launch1 },
    { Qt::Key_Launch4,            XF86XK_Launch2 },
    { Qt::Key_Launch5,            XF86XK_Launch3 },
    { Qt::Key_Launch6,            XF86XK_Launch4 },
    { Qt::Key_Launch7,            XF86XK_Launch5 },
    { Qt::Key_Launch8,            XF86XK_Launch6 },
    { Qt::Key_Launch9,            XF86XK_Launch7 },
    { Qt::Key_LaunchA,            XF86XK_Launch8 },
    { Qt::Key_LaunchB,            XF86XK_Launch9 },
    { Qt::Key_LaunchC,            XF86XK_LaunchA },
    { Qt::Key_LaunchD,            XF86XK_LaunchB },
    { Qt::Key_LaunchE,            XF86XK_LaunchC },
    { Qt::Key_LaunchF,            XF86XK_LaunchD },
    { Qt::Key_LaunchG,            XF86XK_LaunchE },
    { Qt::Key_LaunchH,            XF86XK_LaunchF },

    {Qt::Key(0),                  0}

};
}  // namespace

#endif  // KEYMAPPER_X11_H
