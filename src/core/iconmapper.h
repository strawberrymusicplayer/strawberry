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

    { QStringLiteral("albums"),                        { {QStringLiteral("media-optical")}} },
    { QStringLiteral("alsa"),                          { {}} },
    { QStringLiteral("application-exit"),              { {}} },
    { QStringLiteral("applications-internet"),         { {}} },
    { QStringLiteral("bluetooth"),                     { {QStringLiteral("preferences-system-bluetooth"), QStringLiteral("bluetooth-active")}} },
    { QStringLiteral("cdcase"),                        { {QStringLiteral("cdcover"), QStringLiteral("media-optical")}} },
    { QStringLiteral("media-optical"),                 { {QStringLiteral("cd")}} },
    { QStringLiteral("configure"),                     { {}} },
    { QStringLiteral("device-ipod-nano"),              { {}} },
    { QStringLiteral("device-ipod"),                   { {}} },
    { QStringLiteral("device-phone"),                  { {}} },
    { QStringLiteral("device"),                        { {QStringLiteral("drive-removable-media-usb-pendrive")}} },
    { QStringLiteral("device-usb-drive"),              { {}} },
    { QStringLiteral("device-usb-flash"),              { {}} },
    { QStringLiteral("dialog-error"),                  { {}} },
    { QStringLiteral("dialog-information"),            { {}} },
    { QStringLiteral("dialog-ok-apply"),               { {}} },
    { QStringLiteral("dialog-password"),               { {}} },
    { QStringLiteral("dialog-warning"),                { {}} },
    { QStringLiteral("document-download"),             { {QStringLiteral("download")}} },
    { QStringLiteral("document-new"),                  { {}} },
    { QStringLiteral("document-open-folder"),          { {}} },
    { QStringLiteral("document-open"),                 { {}} },
    { QStringLiteral("document-save"),                 { {}} },
    { QStringLiteral("document-search"),               { {}} },
    { QStringLiteral("document-open-remote"),          { {}} },
    { QStringLiteral("download"),                      { {QStringLiteral("applications-internet"), QStringLiteral("network-workgroup")}} },
    { QStringLiteral("edit-clear-list"),               { {QStringLiteral("edit-clear-list"), QStringLiteral("edit-clear-all")}} },
    { QStringLiteral("edit-clear-locationbar-ltr"),    { {QStringLiteral("edit-clear-locationbar-ltr")}} },
    { QStringLiteral("edit-copy"),                     { {}} },
    { QStringLiteral("edit-delete"),                   { {}} },
    { QStringLiteral("edit-find"),                     { {}} },
    { QStringLiteral("edit-redo"),                     { {}} },
    { QStringLiteral("edit-rename"),                   { {}} },
    { QStringLiteral("edit-undo"),                     { {}} },
    { QStringLiteral("electrocompaniet"),              { {}} },
    { QStringLiteral("equalizer"),                     { {QStringLiteral("view-media-equalizer")}} },
    { QStringLiteral("folder-new"),                    { {}} },
    { QStringLiteral("folder"),                        { {}} },
    { QStringLiteral("folder-sound"),                  { {QStringLiteral("folder-music")}} },
    { QStringLiteral("footsteps"),                     { {QStringLiteral("go-jump")}} },
    { QStringLiteral("go-down"),                       { {}} },
    { QStringLiteral("go-home"),                       { {}} },
    { QStringLiteral("go-jump"),                       { {}} },
    { QStringLiteral("go-next"),                       { {}} },
    { QStringLiteral("go-previous"),                   { {}} },
    { QStringLiteral("go-up"),                         { {}} },
    { QStringLiteral("gstreamer"),                     { {QStringLiteral("phonon-gstreamer")}} },
    { QStringLiteral("headset"),                       { {QStringLiteral("audio-headset")}} },
    { QStringLiteral("help-hint"),                     { {}} },
    { QStringLiteral("intel"),                         { {}} },
    { QStringLiteral("jack"),                          { {QStringLiteral("audio-input-line")}} },
    { QStringLiteral("keyboard"),                      { {QStringLiteral("input-keyboard")}} },
    { QStringLiteral("list-add"),                      { {}} },
    { QStringLiteral("list-remove"),                   { {}} },
    { QStringLiteral("love"),                          { {QStringLiteral("heart"), QStringLiteral("emblem-favorite")}} },
    { QStringLiteral("mcintosh-player"),               { {}} },
    { QStringLiteral("mcintosh"),                      { {}} },
    { QStringLiteral("mcintosh-text"),                 { {}} },
    { QStringLiteral("media-eject"),                   { {}} },
    { QStringLiteral("media-playback-pause"),          { {QStringLiteral("media-pause")}} },
    { QStringLiteral("media-playlist-repeat"),         { {}} },
    { QStringLiteral("media-playlist-shuffle"),        { {""_L1}} },
    { QStringLiteral("media-playback-start"),          { {QStringLiteral("media-play"), QStringLiteral("media-playback-playing")}} },
    { QStringLiteral("media-seek-backward"),           { {}} },
    { QStringLiteral("media-seek-forward"),            { {}} },
    { QStringLiteral("media-skip-backward"),           { {}} },
    { QStringLiteral("media-skip-forward"),            { {}} },
    { QStringLiteral("media-playback-stop"),           { {QStringLiteral("media-stop")}} },
    { QStringLiteral("moodbar"),                       { {QStringLiteral("preferences-desktop-icons")}} },
    { QStringLiteral("nvidia"),                        { {}} },
    { QStringLiteral("pulseaudio"),                    { {}} },
    { QStringLiteral("realtek"),                       { {}} },
    { QStringLiteral("scrobble-disabled"),             { {}} },
    { QStringLiteral("scrobble"),                      { {QStringLiteral("love")}} },
    { QStringLiteral("search"),                        { {}} },
    { QStringLiteral("soundcard"),                     { {QStringLiteral("audiocard"), QStringLiteral("audio-card")}} },
    { QStringLiteral("speaker"),                       { {}} },
    { QStringLiteral("star-grey"),                     { {}} },
    { QStringLiteral("star"),                          { {}} },
    { QStringLiteral("strawberry"),                    { {}} },
    { QStringLiteral("subsonic"),                      { {}} },
    { QStringLiteral("tidal"),                         { {}} },
    { QStringLiteral("tools-wizard"),                  { {}} },
    { QStringLiteral("view-choose"),                   { {}} },
    { QStringLiteral("view-fullscreen"),               { {}} },
    { QStringLiteral("view-media-lyrics"),             { {}} },
    { QStringLiteral("view-media-playlist"),           { {}} },
    { QStringLiteral("view-media-visualization"),      { {QStringLiteral("preferences-desktop-theme")}} },
    { QStringLiteral("view-refresh"),                  { {}} },
    { QStringLiteral("library-music"),                 { {QStringLiteral("vinyl")}} },
    { QStringLiteral("vlc"),                           { {}} },
    { QStringLiteral("zoom-in"),                       { {}} },
    { QStringLiteral("zoom-out"),                      { {}, 0, 0 } }

};

}  // namespace IconMapper

#endif  // ICONMAPPER_H

