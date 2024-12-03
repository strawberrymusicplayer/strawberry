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

#ifndef NETWORKREMOTECONSTANTS_H
#define NETWORKREMOTECONSTANTS_H

#include <QStringList>

using namespace Qt::Literals::StringLiterals;

namespace NetworkRemoteConstants {

const QStringList kDefaultMusicExtensionsAllowedRemotely = { u"aac"_s, u"alac"_s, u"flac"_s, u"m3u"_s, u"m4a"_s, u"mp3"_s, u"ogg"_s, u"wav"_s, u"wmv"_s };
constexpr quint16 kDefaultServerPort = 5500;
constexpr char kTranscoderSettingPostfix[] = "/NetworkRemote";
constexpr quint32 kFileChunkSize = 100000;

}  // namespace NetworkRemoteConstants

#endif // NETWORKREMOTECONSTANTS_H
