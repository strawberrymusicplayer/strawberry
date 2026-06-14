ALTER TABLE playlists ADD COLUMN half_playing_time_s INTEGER;

ALTER TABLE playlists ADD COLUMN position_playing_time INTEGER;

UPDATE schema_version SET version=23;
