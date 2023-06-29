ALTER TABLE songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE subsonic_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE subsonic_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE tidal_artists_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE tidal_artists_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE tidal_albums_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE tidal_albums_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE tidal_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE tidal_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE qobuz_artists_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE qobuz_artists_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE qobuz_albums_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE qobuz_albums_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE qobuz_songs ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE qobuz_songs ADD COLUMN art_unset INTEGER DEFAULT 0;

ALTER TABLE playlist_items ADD COLUMN art_embedded INTEGER DEFAULT 0;

ALTER TABLE playlist_items ADD COLUMN art_unset INTEGER DEFAULT 0;

UPDATE songs SET art_embedded = 1 WHERE art_automatic = 'file:(embedded)';

UPDATE songs SET art_automatic = '' WHERE art_automatic = 'file:(embedded)';

UPDATE songs SET art_unset = 1 WHERE art_manual = 'file:(unset)';

UPDATE songs SET art_manual = '' WHERE art_manual = 'file:(unset)';

UPDATE schema_version SET version=17;
