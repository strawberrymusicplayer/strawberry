#!/usr/bin/env bash

# Strawberry Music Player
# Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
# Copyright 2021, Alexey Vazhnov
#
# Strawberry is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Strawberry is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
# SPDX-License-Identifier: GPL-3.0-only


# Based on https://wiki.strawberrymusicplayer.org/wiki/Import_collection_library_and_playlists_from_Clementine

set -o nounset
set -o errexit
set -o pipefail
shopt -s dotglob

# Use hardcoded path if no parameters. No need in quotes here! See `man bash`, "Parameter Expansion":
FILE_SRC=${1:-~/.config/Clementine/clementine.db}
FILE_DST=${2:-~/.local/share/strawberry/strawberry/strawberry.db}

test -f "$FILE_SRC" || { echo "No such file: $FILE_SRC"; exit 1; }
test -f "$FILE_DST" || { echo "No such file: $FILE_DST"; exit 1; }

echo "Will try to copy information from $FILE_SRC to $FILE_DST."
echo
echo 'This script will **delete all data** from the Strawberry database!'
read -r -p 'Do you want to continue? (Only YES is accepted) ' answer
if [ "$answer" != "YES" ]; then exit 1; fi

# 'heredoc' with substitution of variables, see `man bash`, "Here Documents":
sqlite3 -batch << EOF
.echo on
ATTACH '$FILE_DST' AS strawberry;
ATTACH '$FILE_SRC' AS clementine;
.bail on
.databases

/* This must be done when importing all data from Clementine because playlists are based on ROWIDs */

DELETE FROM strawberry.directories;
DELETE FROM strawberry.subdirectories;
DELETE FROM strawberry.songs;
DELETE FROM strawberry.playlists;
DELETE FROM strawberry.playlist_items;

/* Import all data from the collection library songs */

INSERT INTO strawberry.directories (path, subdirs) SELECT path, subdirs FROM clementine.directories;
INSERT INTO strawberry.subdirectories (directory_id, path, mtime) SELECT directory, path, mtime FROM clementine.subdirectories;
INSERT INTO strawberry.songs (ROWID, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, beginning, length, bitrate, samplerate, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path, rating)
SELECT ROWID, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, beginning, length, bitrate, samplerate, directory, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, sampler, forced_compilation_on, forced_compilation_off, effective_compilation, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path, rating FROM clementine.songs WHERE unavailable = 0;
UPDATE strawberry.songs SET source = 2;
UPDATE strawberry.songs SET artist_id = '';
UPDATE strawberry.songs SET album_id = '';
UPDATE strawberry.songs SET song_id = '';

/* Import playlists */

INSERT INTO strawberry.playlists (ROWID, name, last_played, special_type, ui_path, is_favorite, dynamic_playlist_type, dynamic_playlist_data, dynamic_playlist_backend)
SELECT ROWID, name, last_played, special_type, ui_path, is_favorite, dynamic_playlist_type, dynamic_playlist_data, dynamic_playlist_backend FROM clementine.playlists WHERE dynamic_playlist_type ISNULL;

/* Import playlist items */

INSERT INTO strawberry.playlist_items
(ROWID,
playlist,
collection_id,
title,
album,
artist,
albumartist,
track,
disc,
year,
originalyear,
genre,
compilation,
composer,
performer,
grouping,
comment,
lyrics,
beginning,
length,
bitrate,
samplerate,
directory_id,
url,
filetype,
filesize,
mtime,
ctime,
unavailable,
playcount,
skipcount,
lastplayed,
compilation_detected,
compilation_on,
compilation_off,
compilation_effective,
art_automatic,
art_manual,
effective_albumartist,
effective_originalyear,
cue_path,
rating
)
SELECT ROWID,
   playlist,
   library_id,
   title,
   album,
   artist,
   albumartist,
   track,
   disc,
   year,
   originalyear,
   genre,
   compilation,
   composer,
   performer,
   grouping,
   comment,
   lyrics,
   beginning,
   length,
   bitrate,
   samplerate,
   directory,
   CASE WHEN filename IS NULL THEN '' ELSE filename END,
   filetype,
   filesize,
   mtime,
   ctime,
   unavailable,
   playcount,
   skipcount,
   lastplayed,
   sampler,
   forced_compilation_on,
   forced_compilation_off,
   effective_compilation,
   art_automatic,
   art_manual,
   effective_albumartist,
   effective_originalyear,
   cue_path,
   rating FROM clementine.playlist_items WHERE type = 'Library';

UPDATE strawberry.playlist_items SET source = 2;
UPDATE strawberry.playlist_items SET type = 2;
UPDATE strawberry.playlist_items SET artist_id = '';
UPDATE strawberry.playlist_items SET album_id = '';
UPDATE strawberry.playlist_items SET song_id = '';

EOF

# To be sure script didn't exit because of any error (because we use `set -o errexit`):
echo "Script finished"
