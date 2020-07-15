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

#pragma once

#include "config.h"

#include <QtGlobal>
#include <QMap>

namespace IconMapper {

struct IconProperties {
  explicit IconProperties() : min_size(0), max_size(0), allow_system_icon(true) {}
  IconProperties(const QStringList &_names, const int _min_size = 16, const int _max_size = 512, const bool _allow_system_icon = true) : names(_names), min_size(_min_size), max_size(_max_size), allow_system_icon(_allow_system_icon) {}
  QStringList names;
  int min_size;
  int max_size;
  bool allow_system_icon;
};

static const QMap<QString, IconProperties> iconmapper_ = {

    { "albums",                        { {"media-optical"}} },
    { "alsa",                          { {}} },
    { "application-exit",              { {}} },
    { "applications-internet",         { {}} },
    { "bluetooth",                     { {"preferences-system-bluetooth", "bluetooth-active"}} },
    { "cdcase",                        { {"cdcover", "media-optical"}} },
    { "media-optical",                 { {"cd"}} },
    { "configure",                     { {}} },
    { "device-ipod-nano",              { {}} },
    { "device-ipod",                   { {}} },
    { "device-phone",                  { {}} },
    { "device",                        { {"drive-removable-media-usb-pendrive"}} },
    { "device-usb-drive",              { {}} },
    { "device-usb-flash",              { {}} },
    { "dialog-error",                  { {}} },
    { "dialog-information",            { {}} },
    { "dialog-ok-apply",               { {}} },
    { "dialog-password",               { {}} },
    { "dialog-warning",                { {}} },
    { "document-download",             { {}} },
    { "document-new",                  { {}} },
    { "document-open-folder",          { {}} },
    { "document-open",                 { {}} },
    { "document-save",                 { {}} },
    { "document-search",               { {}} },
    { "document-open-remote",          { {}} },
    { "download",                      { {"applications-internet", "network-workgroup"}} },
    { "edit-clear-list",               { {}} },
    { "edit-clear-locationbar-ltr",    { {}, 0, 0, false } },
    { "edit-copy",                     { {}} },
    { "edit-delete",                   { {}} },
    { "edit-find",                     { {}} },
    { "edit-redo",                     { {}} },
    { "edit-rename",                   { {}} },
    { "edit-undo",                     { {}} },
    { "electrocompaniet",              { {}} },
    { "equalizer",                     { {"view-media-equalizer"}} },
    { "folder-new",                    { {}} },
    { "folder",                        { {}} },
    { "folder-sound",                  { {"folder-music"}} },
    { "footsteps",                     { {"go-jump"}} },
    { "go-down",                       { {}} },
    { "go-home",                       { {}} },
    { "go-jump",                       { {}} },
    { "go-next",                       { {}} },
    { "go-previous",                   { {}} },
    { "go-up",                         { {}} },
    { "gstreamer",                     { {"phonon-gstreamer"}} },
    { "headset",                       { {"audio-headset"}} },
    { "help-hint",                     { {}} },
    { "intel",                         { {}} },
    { "jack",                          { {"audio-input-line"}} },
    { "keyboard",                      { {"input-keyboard"}} },
    { "list-add",                      { {}} },
    { "list-remove",                   { {}} },
    { "love",                          { {"heart", "emblem-favorite"}} },
    { "mcintosh-player",               { {}} },
    { "mcintosh",                      { {}} },
    { "mcintosh-text",                 { {}} },
    { "media-eject",                   { {}} },
    { "media-playback-pause",          { {"media-pause"}} },
    { "media-playlist-repeat",         { {}} },
    { "media-playlist-shuffle",        { {""}} },
    { "media-playback-start",          { {"media-play", "media-playback-playing"}} },
    { "media-seek-backward",           { {}} },
    { "media-seek-forward",            { {}} },
    { "media-skip-backward",           { {}} },
    { "media-skip-forward",            { {}} },
    { "media-playback-stop",           { {"media-stop"}} },
    { "moodbar",                       { {"preferences-desktop-icons"}} },
    { "nvidia",                        { {}} },
    { "pulseaudio",                    { {}} },
    { "realtek",                       { {}} },
    { "scrobble-disabled",             { {}} },
    { "scrobble",                      { {}} },
    { "search",                        { {}} },
    { "soundcard",                     { {"audiocard", "audio-card"}} },
    { "speaker",                       { {}} },
    { "star-grey",                     { {}} },
    { "star",                          { {}} },
    { "strawberry",                    { {}} },
    { "subsonic",                      { {}} },
    { "tidal",                         { {}} },
    { "tools-wizard",                  { {}} },
    { "view-choose",                   { {}} },
    { "view-fullscreen",               { {}} },
    { "view-media-lyrics",             { {}} },
    { "view-media-playlist",           { {}} },
    { "view-media-visualization",      { {"preferences-desktop-theme"}} },
    { "view-refresh",                  { {}} },
    { "library-music",                 { {"vinyl"}} },
    { "vlc",                           { {}} },
    { "zoom-in",                       { {}} },
    { "zoom-out",                      { {}, 0, 0 } }

};

}  // namespace
