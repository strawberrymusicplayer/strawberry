DROP INDEX IF EXISTS idx_artistsort;

DROP INDEX IF EXISTS idx_albumartistsort;

ALTER TABLE songs ADD COLUMN artistsort TEXT;

ALTER TABLE songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE subsonic_songs ADD COLUMN artistsort TEXT;

ALTER TABLE subsonic_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE tidal_artists_songs ADD COLUMN artistsort TEXT;

ALTER TABLE tidal_artists_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE tidal_albums_songs ADD COLUMN artistsort TEXT;

ALTER TABLE tidal_albums_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE tidal_songs ADD COLUMN artistsort TEXT;

ALTER TABLE tidal_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE spotify_artists_songs ADD COLUMN artistsort TEXT;

ALTER TABLE spotify_artists_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE spotify_albums_songs ADD COLUMN artistsort TEXT;

ALTER TABLE spotify_albums_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE spotify_songs ADD COLUMN artistsort TEXT;

ALTER TABLE spotify_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE qobuz_artists_songs ADD COLUMN artistsort TEXT;

ALTER TABLE qobuz_artists_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE qobuz_albums_songs ADD COLUMN artistsort TEXT;

ALTER TABLE qobuz_albums_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE qobuz_songs ADD COLUMN artistsort TEXT;

ALTER TABLE qobuz_songs ADD COLUMN albumartistsort TEXT;

ALTER TABLE playlist_items ADD COLUMN artistsort TEXT;

ALTER TABLE playlist_items ADD COLUMN albumartistsort TEXT;

CREATE INDEX IF NOT EXISTS idx_artistsort ON songs (artistsort);

CREATE INDEX IF NOT EXISTS idx_albumartistsort ON songs (albumartistsort);

UPDATE schema_version SET version=21;
