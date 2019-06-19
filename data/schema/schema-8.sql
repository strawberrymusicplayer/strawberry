ALTER TABLE songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE playlist_items RENAME COLUMN album_id TO album_id_old;

ALTER TABLE tidal_artists_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE tidal_albums_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE tidal_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE qobuz_artists_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE qobuz_albums_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE qobuz_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE subsonic_songs RENAME COLUMN album_id TO album_id_old;

ALTER TABLE songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE playlist_items ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE tidal_artists_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE tidal_albums_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE tidal_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE qobuz_artists_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE qobuz_albums_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE qobuz_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

ALTER TABLE subsonic_songs ADD COLUMN album_id TEXT NOT NULL DEFAULT "";

UPDATE schema_version SET version=8;
