CREATE TABLE IF NOT EXISTS radio_channels (
  source INTEGER NOT NULL DEFAULT 0,
  name TEXT NOT NULL,
  url TEXT NOT NULL,
  thumbnail_url TEXT
);

UPDATE schema_version SET version=15;
