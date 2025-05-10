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

#ifndef FILEFILTERCONSTANTS_H
#define FILEFILTERCONSTANTS_H

#include <QtGlobal>

constexpr char kAllFilesFilterSpec[] = QT_TRANSLATE_NOOP("FileFilter", "All Files (*)");

constexpr char kFileFilter[] =
    "*.wav *.flac *.wv *.ogg *.oga *.opus *.spx *.ape *.mpc "
    "*.mp2 *.mp3 *.m4a *.mp4 *.aac *.asf *.asx *.wma "
    "*.aif *.aiff *.mka *.tta *.dsf *.dsd "
    "*.cue *.m3u *.m3u8 *.pls *.xspf *.asxini "
    "*.ac3 *.dts "
    "*.mod *.s3m *.xm *.it "
    "*.spc *.vgm";

constexpr char kLoadImageFileFilter[] = QT_TRANSLATE_NOOP("FileFilter", "Images (*.png *.jpg *.jpeg *.bmp *.gif *.xpm *.pbm *.pgm *.ppm *.xbm)");
constexpr char kSaveImageFileFilter[] = QT_TRANSLATE_NOOP("FileFilter", "Images (*.png *.jpg *.jpeg *.bmp *.xpm *.pbm *.ppm *.xbm)");

#endif  // FILEFILTERCONSTANTS_H
