ALTER TABLE %allsongstables ADD COLUMN fingerprint TEXT DEFAULT '';

ALTER TABLE %allsongstables ADD COLUMN lastseen INTEGER NOT NULL DEFAULT -1;

UPDATE schema_version SET version=14;
