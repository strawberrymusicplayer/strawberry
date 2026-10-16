DROP INDEX IF EXISTS idx_songs_url_beginning;

CREATE UNIQUE INDEX IF NOT EXISTS idx_songs_url_beginning_cue_path ON songs (url, beginning, IFNULL(cue_path, ''));

UPDATE schema_version SET version=29;
