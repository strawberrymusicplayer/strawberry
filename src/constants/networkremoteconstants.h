/*
 * Strawberry Music Player
 * Copyright 2025, Leopold List <leo@zudiewiener.com>
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

#include <QtGlobal>

namespace NetworkRemoteConstants {

// Protocol version history:
// 1 - initial protocol (song info, transport control, engine state push)
// 2 - position/length in ResponseSongMetadata, version field in Message
constexpr quint32 kProtocolVersion = 3;

// Oldest client protocol version this server accepts.
// 0 = clients that predate the version field.
constexpr quint32 kMinSupportedVersion = 3;

}  // namespace NetworkRemoteConstants

#endif  // NETWORKREMOTECONSTANTS_H
