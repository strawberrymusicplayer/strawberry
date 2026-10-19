/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
 * Copyright 2026, Strawberry Music Player contributors
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

#ifndef STREAMMETADATAPARSER_H
#define STREAMMETADATAPARSER_H

class EngineMetadata;

namespace StreamMetadataParser {

// Parses the stream metadata in place.
// A title which is a JSON object replaces the title with the nonempty "title" or "name" value, and fills the empty artist and album from "artist" and "album".
// Valid JSON arrays and objects without a usable title or name are left unchanged.
// Other titles, including malformed JSON, are split into artist, title and album on " - " or "~" when the artist and album are empty.
void Parse(EngineMetadata &metadata);

}  // namespace StreamMetadataParser

#endif  // STREAMMETADATAPARSER_H
