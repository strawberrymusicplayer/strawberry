/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

namespace IconMapper {

struct IconProperties {
  IconProperties() : min_size(0), max_size(0) {}
  IconProperties(const QStringList &_names, const int _min_size, const int _max_size) : names(_names), min_size(_min_size), max_size(_max_size) {}
  QStringList names;
  int min_size;
  int max_size;
};

static const QMap<QString, IconProperties> iconmapper_ = {

    { "albums",                        { {"media-optical"}, 0, 0 } },
    { "alsa",                          { {}, 0, 0 } },
    { "application-exit",              { {}, 0, 0 } },
    { "applications-internet",         { {}, 0, 0 } },
    { "bluetooth",                     { {"preferences-system-bluetooth", "bluetooth-active"}, 0, 0 } },
    { "cdcase",                        { {"cdcover", "media-optical"}, 0, 0 } },
    { "media-optical",                 { {"cd"}, 0, 0 } },
    { "configure",                     { {}, 0, 0 } },
    { "device-ipod-nano",              { {}, 0, 0 } },
    { "device-ipod",                   { {}, 0, 0 } },
    { "device-phone",                  { {}, 0, 0 } },
    { "device",                        { {"drive-removable-media-usb-pendrive"}, 0, 0 } },
    { "device-usb-drive",              { {}, 0, 0 } },
    { "device-usb-flash",              { {}, 0, 0 } },
    { "dialog-error",                  { {}, 0, 0 } },
    { "dialog-information",            { {}, 0, 0 } },
    { "dialog-ok-apply",               { {}, 0, 0 } },
    { "dialog-password",               { {}, 0, 0 } },
    { "dialog-warning",                { {}, 0, 0 } },
    { "document-download",             { {}, 0, 0 } },
    { "document-new",                  { {}, 0, 0 } },
    { "document-open-folder",          { {}, 0, 0 } },
    { "document-open",                 { {}, 0, 0 } },
    { "document-save",                 { {}, 0, 0 } },
    { "document-search",               { {}, 0, 0 } },
    { "download",                      { {"applications-internet", "network-workgroup"}, 0, 0 } },
    { "edit-clear-list",               { {}, 0, 0 } },
    { "edit-clear-locationbar-ltr",    { {}, 0, 0 } },
    { "edit-copy",                     { {}, 0, 0 } },
    { "edit-delete",                   { {}, 0, 0 } },
    { "edit-find",                     { {}, 0, 0 } },
    { "edit-redo",                     { {}, 0, 0 } },
    { "edit-rename",                   { {}, 0, 0 } },
    { "edit-undo",                     { {}, 0, 0 } },
    { "electrocompaniet",              { {}, 0, 0 } },
    { "equalizer",                     { {"view-media-equalizer"}, 0, 0 } },
    { "folder-new",                    { {}, 0, 0 } },
    { "folder",                        { {}, 0, 0 } },
    { "folder-sound",                  { {"folder-music"}, 0, 0 } },
    { "footsteps",                     { {"go-jump"}, 0, 0 } },
    { "go-down",                       { {}, 0, 0 } },
    { "go-home",                       { {}, 0, 0 } },
    { "go-jump",                       { {}, 0, 0 } },
    { "go-next",                       { {}, 0, 0 } },
    { "go-previous",                   { {}, 0, 0 } },
    { "go-up",                         { {}, 0, 0 } },
    { "gstreamer",                     { {"phonon-gstreamer"}, 0, 0 } },
    { "headset",                       { {"audio-headset"}, 0, 0 } },
    { "help-hint",                     { {}, 0, 0 } },
    { "intel",                         { {}, 0, 0 } },
    { "jack",                          { {"audio-input-line"}, 0, 0 } },
    { "keyboard",                      { {"input-keyboard"}, 0, 0 } },
    { "list-add",                      { {}, 0, 0 } },
    { "list-remove",                   { {}, 0, 0 } },
    { "love",                          { {"heart", "emblem-favorite"}, 0, 0 } },
    { "mcintosh-player",               { {}, 0, 0 } },
    { "mcintosh",                      { {}, 0, 0 } },
    { "mcintosh-text",                 { {}, 0, 0 } },
    { "media-eject",                   { {}, 0, 0 } },
    { "media-playback-pause",          { {"media-pause"}, 0, 0 } },
    { "media-playlist-repeat",         { {}, 0, 0 } },
    { "media-playlist-shuffle",        { {""}, 0, 0 } },
    { "media-playback-start",          { {"media-play", "media-playback-playing"}, 0, 0 } },
    { "media-seek-backward",           { {}, 0, 0 } },
    { "media-seek-forward",            { {}, 0, 0 } },
    { "media-skip-backward",           { {}, 0, 0 } },
    { "media-skip-forward",            { {}, 0, 0 } },
    { "media-playback-stop",           { {"media-stop"}, 0, 0 } },
    { "moodbar",                       { {"preferences-desktop-icons"}, 0, 0 } },
    { "nvidia",                        { {}, 0, 0 } },
    { "pulseaudio",                    { {}, 0, 0 } },
    { "realtek",                       { {}, 0, 0 } },
    { "scrobble-disabled",             { {}, 0, 0 } },
    { "scrobble",                      { {}, 0, 0 } },
    { "search",                        { {}, 0, 0 } },
    { "soundcard",                     { {"audiocard", "audio-card"}, 0, 0 } },
    { "speaker",                       { {}, 0, 0 } },
    { "star-grey",                     { {}, 0, 0 } },
    { "star",                          { {}, 0, 0 } },
    { "strawberry",                    { {}, 0, 0 } },
    { "subsonic",                      { {}, 0, 0 } },
    { "tools-wizard",                  { {}, 0, 0 } },
    { "view-choose",                   { {}, 0, 0 } },
    { "view-fullscreen",               { {}, 0, 0 } },
    { "view-media-lyrics",             { {}, 0, 0 } },
    { "view-media-playlist",           { {}, 0, 0 } },
    { "view-media-visualization",      { {"preferences-desktop-theme"}, 0, 0 } },
    { "view-refresh",                  { {}, 0, 0 } },
    { "library-music",                 { {"vinyl"}, 0, 0 } },
    { "vlc",                           { {}, 0, 0 } },
    { "xine",                          { {}, 0, 0 } },
    { "zoom-in",                       { {}, 0, 0 } },
    { "zoom-out",                      { {}, 0, 0 } }

};

}  // namespace
