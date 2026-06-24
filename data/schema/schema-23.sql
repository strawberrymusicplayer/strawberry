UPDATE playlist_items SET collection_id = (SELECT keep.ROWID FROM songs AS keep, songs AS cur WHERE cur.ROWID = playlist_items.collection_id AND keep.url = cur.url AND keep.beginning = cur.beginning ORDER BY keep.unavailable ASC, keep.ROWID ASC LIMIT 1) WHERE playlist_items.type = 2 AND playlist_items.collection_id IN (SELECT ROWID FROM songs);

DELETE FROM songs WHERE ROWID NOT IN (SELECT (SELECT keep.ROWID FROM songs AS keep WHERE keep.url = songs.url AND keep.beginning = songs.beginning ORDER BY keep.unavailable ASC, keep.ROWID ASC LIMIT 1) FROM songs);

CREATE UNIQUE INDEX IF NOT EXISTS idx_songs_url_beginning ON songs (url, beginning);

UPDATE schema_version SET version=23;
