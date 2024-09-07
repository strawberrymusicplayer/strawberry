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

#ifndef ICONMAPPER_H
#define ICONMAPPER_H

#include "config.h"

#include <QtGlobal>
#include <QMap>

namespace IconMapper {

using namespace Qt::Literals::StringLiterals;

struct IconProperties {
  explicit IconProperties() : min_size(0), max_size(0), allow_system_icon(true) {}
  IconProperties(const QStringList &_names, const int _min_size = 16, const int _max_size = 512, const bool _allow_system_icon = true) : names(_names), min_size(_min_size), max_size(_max_size), allow_system_icon(_allow_system_icon) {}
  QStringList names;
  int min_size;
  int max_size;
  bool allow_system_icon;
};

static const QMap<QString, IconProperties> iconmapper_ = {  // clazy:exclude=non-pod-global-static

    { u"albums"_s,                        { {u"media-optical"_s}} },
    { u"alsa"_s,                          { {}} },
    { u"application-exit"_s,              { {}} },
    { u"applications-internet"_s,         { {}} },
    { u"bluetooth"_s,                     { {u"preferences-system-bluetooth"_s, u"bluetooth-active"_s}} },
    { u"cdcase"_s,                        { {u"cdcover"_s, u"media-optical"_s}} },
    { u"media-optical"_s,                 { {u"cd"_s}} },
    { u"configure"_s,                     { {}} },
    { u"device-ipod-nano"_s,              { {}} },
    { u"device-ipod"_s,                   { {}} },
    { u"device-phone"_s,                  { {}} },
    { u"device"_s,                        { {u"drive-removable-media-usb-pendrive"_s}} },
    { u"device-usb-drive"_s,              { {}} },
    { u"device-usb-flash"_s,              { {}} },
    { u"dialog-error"_s,                  { {}} },
    { u"dialog-information"_s,            { {}} },
    { u"dialog-ok-apply"_s,               { {}} },
    { u"dialog-password"_s,               { {}} },
    { u"dialog-warning"_s,                { {}} },
    { u"document-download"_s,             { {u"download"_s}} },
    { u"document-new"_s,                  { {}} },
    { u"document-open-folder"_s,          { {}} },
    { u"document-open"_s,                 { {}} },
    { u"document-save"_s,                 { {}} },
    { u"document-search"_s,               { {}} },
    { u"document-open-remote"_s,          { {}} },
    { u"download"_s,                      { {u"applications-internet"_s, u"network-workgroup"_s}} },
    { u"edit-clear-list"_s,               { {u"edit-clear-list"_s, u"edit-clear-all"_s}} },
    { u"edit-clear-locationbar-ltr"_s,    { {u"edit-clear-locationbar-ltr"_s}} },
    { u"edit-copy"_s,                     { {}} },
    { u"edit-delete"_s,                   { {}} },
    { u"edit-find"_s,                     { {}} },
    { u"edit-redo"_s,                     { {}} },
    { u"edit-rename"_s,                   { {}} },
    { u"edit-undo"_s,                     { {}} },
    { u"electrocompaniet"_s,              { {}} },
    { u"equalizer"_s,                     { {u"view-media-equalizer"_s}} },
    { u"folder-new"_s,                    { {}} },
    { u"folder"_s,                        { {}} },
    { u"folder-sound"_s,                  { {u"folder-music"_s}} },
    { u"footsteps"_s,                     { {u"go-jump"_s}} },
    { u"go-down"_s,                       { {}} },
    { u"go-home"_s,                       { {}} },
    { u"go-jump"_s,                       { {}} },
    { u"go-next"_s,                       { {}} },
    { u"go-previous"_s,                   { {}} },
    { u"go-up"_s,                         { {}} },
    { u"gstreamer"_s,                     { {u"phonon-gstreamer"_s}} },
    { u"headset"_s,                       { {u"audio-headset"_s}} },
    { u"help-hint"_s,                     { {}} },
    { u"intel"_s,                         { {}} },
    { u"jack"_s,                          { {u"audio-input-line"_s}} },
    { u"keyboard"_s,                      { {u"input-keyboard"_s}} },
    { u"list-add"_s,                      { {}} },
    { u"list-remove"_s,                   { {}} },
    { u"love"_s,                          { {u"heart"_s, u"emblem-favorite"_s}} },
    { u"mcintosh-player"_s,               { {}} },
    { u"mcintosh"_s,                      { {}} },
    { u"mcintosh-text"_s,                 { {}} },
    { u"media-eject"_s,                   { {}} },
    { u"media-playback-pause"_s,          { {u"media-pause"_s}} },
    { u"media-playlist-repeat"_s,         { {}} },
    { u"media-playlist-shuffle"_s,        { {""_L1}} },
    { u"media-playback-start"_s,          { {u"media-play"_s, u"media-playback-playing"_s}} },
    { u"media-seek-backward"_s,           { {}} },
    { u"media-seek-forward"_s,            { {}} },
    { u"media-skip-backward"_s,           { {}} },
    { u"media-skip-forward"_s,            { {}} },
    { u"media-playback-stop"_s,           { {u"media-stop"_s}} },
    { u"moodbar"_s,                       { {u"preferences-desktop-icons"_s}} },
    { u"nvidia"_s,                        { {}} },
    { u"pulseaudio"_s,                    { {}} },
    { u"realtek"_s,                       { {}} },
    { u"scrobble-disabled"_s,             { {}} },
    { u"scrobble"_s,                      { {u"love"_s}} },
    { u"search"_s,                        { {}} },
    { u"soundcard"_s,                     { {u"audiocard"_s, u"audio-card"_s}} },
    { u"speaker"_s,                       { {}} },
    { u"star-grey"_s,                     { {}} },
    { u"star"_s,                          { {}} },
    { u"strawberry"_s,                    { {}} },
    { u"subsonic"_s,                      { {}} },
    { u"tidal"_s,                         { {}} },
    { u"tools-wizard"_s,                  { {}} },
    { u"view-choose"_s,                   { {}} },
    { u"view-fullscreen"_s,               { {}} },
    { u"view-media-lyrics"_s,             { {}} },
    { u"view-media-playlist"_s,           { {}} },
    { u"view-media-visualization"_s,      { {u"preferences-desktop-theme"_s}} },
    { u"view-refresh"_s,                  { {}} },
    { u"library-music"_s,                 { {u"vinyl"_s}} },
    { u"vlc"_s,                           { {}} },
    { u"zoom-in"_s,                       { {}} },
    { u"zoom-out"_s,                      { {}, 0, 0 } }

};

}  // namespace IconMapper

#endif  // ICONMAPPER_H
