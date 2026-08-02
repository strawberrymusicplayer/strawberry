ALTER TABLE playlists ADD COLUMN half_playing_time_s INTEGER NOT NULL DEFAULT 0;

ALTER TABLE playlists ADD COLUMN position_playing_time INTEGER NOT NULL DEFAULT 50;

UPDATE schema_version SET version=24;
