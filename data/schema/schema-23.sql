UPDATE playlist_items SET collection_id = (SELECT keep.ROWID FROM songs AS keep, songs AS cur WHERE cur.ROWID = playlist_items.collection_id AND keep.url = cur.url AND keep.beginning = cur.beginning ORDER BY keep.unavailable ASC, keep.ROWID ASC LIMIT 1) WHERE playlist_items.type = 2 AND playlist_items.collection_id IN (SELECT ROWID FROM songs);

UPDATE songs SET playcount = (SELECT SUM(d.playcount) FROM songs AS d WHERE d.url = songs.url AND d.beginning = songs.beginning), skipcount = (SELECT SUM(d.skipcount) FROM songs AS d WHERE d.url = songs.url AND d.beginning = songs.beginning), lastplayed = (SELECT MAX(d.lastplayed) FROM songs AS d WHERE d.url = songs.url AND d.beginning = songs.beginning), lastseen = (SELECT MAX(d.lastseen) FROM songs AS d WHERE d.url = songs.url AND d.beginning = songs.beginning), rating = (SELECT MAX(d.rating) FROM songs AS d WHERE d.url = songs.url AND d.beginning = songs.beginning) WHERE songs.ROWID = (SELECT keep.ROWID FROM songs AS keep WHERE keep.url = songs.url AND keep.beginning = songs.beginning ORDER BY keep.unavailable ASC, keep.ROWID ASC LIMIT 1) AND EXISTS (SELECT 1 FROM songs AS d2 WHERE d2.url = songs.url AND d2.beginning = songs.beginning AND d2.ROWID <> songs.ROWID);

DELETE FROM songs WHERE ROWID NOT IN (SELECT (SELECT keep.ROWID FROM songs AS keep WHERE keep.url = songs.url AND keep.beginning = songs.beginning ORDER BY keep.unavailable ASC, keep.ROWID ASC LIMIT 1) FROM songs);

CREATE UNIQUE INDEX IF NOT EXISTS idx_songs_url_beginning ON songs (url, beginning);

UPDATE schema_version SET version=23;
