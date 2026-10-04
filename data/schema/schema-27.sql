ALTER TABLE %allsongstables ADD COLUMN art_automatic_mtime INTEGER NOT NULL DEFAULT 0;

ALTER TABLE %allsongstables ADD COLUMN lyrics_storage_type INTEGER NOT NULL DEFAULT 0;

ALTER TABLE %allsongstables ADD COLUMN synced_lyrics_storage_type INTEGER NOT NULL DEFAULT 0;

ALTER TABLE %allsongstables ADD COLUMN lrc_mtime INTEGER NOT NULL DEFAULT 0;

UPDATE %allsongstables SET lyrics_storage_type = 2 WHERE lyrics IS NOT NULL AND lyrics != '';

UPDATE %allsongstables SET synced_lyrics_storage_type = 2 WHERE synced_lyrics IS NOT NULL AND synced_lyrics != '';

UPDATE schema_version SET version=27;
