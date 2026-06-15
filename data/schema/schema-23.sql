DELETE FROM songs WHERE ROWID NOT IN (SELECT (SELECT keep.ROWID FROM songs AS keep WHERE keep.url = songs.url AND keep.beginning = songs.beginning ORDER BY keep.unavailable ASC, keep.ROWID ASC LIMIT 1) FROM songs);

CREATE UNIQUE INDEX IF NOT EXISTS idx_songs_url_beginning ON songs (url, beginning);

UPDATE schema_version SET version=23;
