CREATE TABLE IF NOT EXISTS radio_channels (
  source INTEGER NOT NULL DEFAULT 0,
  name TEXT DEFAULT '',
  url TEXT DEFAULT '',
  thumbnail_url TEXT DEFAULT ''
);

UPDATE schema_version SET version=15;
