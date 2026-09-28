ALTER TABLE %allsongstables ADD COLUMN lyrics_synced TEXT;

UPDATE schema_version SET version=26;
