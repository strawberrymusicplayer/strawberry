ALTER TABLE playlist_items ADD COLUMN uuid TEXT;

UPDATE schema_version SET version=22;
