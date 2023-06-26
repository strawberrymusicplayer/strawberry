ALTER TABLE songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE subsonic_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE subsonic_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE tidal_artists_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE tidal_artists_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE tidal_albums_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE tidal_albums_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE tidal_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE tidal_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE qobuz_artists_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE qobuz_artists_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE qobuz_albums_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE qobuz_albums_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE qobuz_songs ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE qobuz_songs ADD COLUMN ebur128_loudness_range_lu REAL;

ALTER TABLE playlist_items ADD COLUMN ebur128_integrated_loudness_lufs REAL;

ALTER TABLE playlist_items ADD COLUMN ebur128_loudness_range_lu REAL;

UPDATE schema_version SET version=18;
