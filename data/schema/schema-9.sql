DROP TABLE %allsongstables_fts;

DROP TABLE playlist_items_fts_;

CREATE VIRTUAL TABLE %allsongstables_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 0"

);

CREATE VIRTUAL TABLE playlist_items_fts_ USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 0"

);

INSERT INTO %allsongstables_fts (ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment FROM %allsongstables;

INSERT INTO playlist_items_fts_ (ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment FROM playlist_items;

UPDATE schema_version SET version=9;
