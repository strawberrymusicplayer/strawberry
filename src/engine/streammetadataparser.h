/*
 * Strawberry Music Player
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

/**
 * Parses stream metadata in place.
 *
 * A trimmed title beginning with `{` or `[` is treated as a JSON candidate.
 * For a valid JSON object, a nonempty trimmed string `title` is preferred over a nonempty trimmed string `name`.
 * The selected title replaces `metadata.title`, and trimmed string `artist` and `album` values fill only empty fields.
 * Existing nonempty artist and album values and all other metadata fields are preserved.
 * Other JSON keys, including `duration` and `type`, are ignored.
 * Valid arrays and objects without a usable title or name are left unchanged and are not delimiter-split.
 * Malformed JSON candidates fall back to legacy parsing, which runs only when title is nonempty and artist and album are empty.
 * Legacy parsing prefers ` - ` over `~` and, when at least two parts are present, uses up to the first three parts as artist, title, and album.
 *
 * @param metadata Metadata to parse and update in place.
 */
void Parse(EngineMetadata &metadata);

}  // namespace StreamMetadataParser

#endif  // STREAMMETADATAPARSER_H
