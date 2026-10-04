ALTER TABLE %allsongstables ADD COLUMN synced_lyrics TEXT;

UPDATE schema_version SET version=26;
