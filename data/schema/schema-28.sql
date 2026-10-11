DROP INDEX IF EXISTS idx_url;

DROP INDEX IF EXISTS idx_albumsort;

DROP INDEX IF EXISTS idx_titlesort;

DROP INDEX IF EXISTS idx_composersort;

DROP INDEX IF EXISTS idx_performersort;

CREATE INDEX IF NOT EXISTS idx_albumsort ON songs (albumsort);

CREATE INDEX IF NOT EXISTS idx_titlesort ON songs (titlesort);

CREATE INDEX IF NOT EXISTS idx_composersort ON songs (composersort);

CREATE INDEX IF NOT EXISTS idx_performersort ON songs (performersort);

UPDATE schema_version SET version=28;
