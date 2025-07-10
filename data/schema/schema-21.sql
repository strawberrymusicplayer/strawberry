DROP INDEX IF EXISTS idx_albumartistsort;

DROP INDEX IF EXISTS idx_albumsort;

DROP INDEX IF EXISTS idx_artistsort;

DROP INDEX IF EXISTS idx_composersort;

DROP INDEX IF EXISTS idx_performersort;

DROP INDEX IF EXISTS idx_titlesort;

ALTER TABLE %allsongstables ADD COLUMN albumartistsort TEXT;

ALTER TABLE %allsongstables ADD COLUMN albumsort TEXT;

ALTER TABLE %allsongstables ADD COLUMN artistsort TEXT;

ALTER TABLE %allsongstables ADD COLUMN composersort TEXT;

ALTER TABLE %allsongstables ADD COLUMN performersort TEXT;

ALTER TABLE %allsongstables ADD COLUMN titlesort TEXT;

ALTER TABLE %allsongstables ADD COLUMN bpm REAL;

ALTER TABLE %allsongstables ADD COLUMN mood TEXT;

ALTER TABLE %allsongstables ADD COLUMN initial_key TEXT;

CREATE INDEX IF NOT EXISTS idx_albumartistsort ON songs (albumartistsort);

CREATE INDEX IF NOT EXISTS idx_albumsort ON songs (album);

CREATE INDEX IF NOT EXISTS idx_artistsort ON songs (artistsort);

CREATE INDEX IF NOT EXISTS idx_composersort ON songs (title);

CREATE INDEX IF NOT EXISTS idx_performersort ON songs (title);

CREATE INDEX IF NOT EXISTS idx_titlesort ON songs (title);

UPDATE schema_version SET version=21;
