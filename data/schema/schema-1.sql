ALTER TABLE playlist_items ADD COLUMN internet_service TEXT;

UPDATE schema_version SET version=1;
