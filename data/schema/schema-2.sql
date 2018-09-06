ALTER TABLE songs ADD COLUMN lyrics TEXT;

ALTER TABLE playlist_items ADD COLUMN lyrics TEXT;

UPDATE schema_version SET version=2;
