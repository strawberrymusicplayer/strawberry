CREATE TABLE schema_version (
  version INTEGER NOT NULL
);

INSERT INTO schema_version (version) VALUES (0);

CREATE TABLE directories (
  path TEXT NOT NULL,
  subdirs INTEGER NOT NULL
);

CREATE TABLE subdirectories (
  directory_id INTEGER NOT NULL,
  path TEXT NOT NULL,
  mtime INTEGER NOT NULL
);

CREATE TABLE songs (

  /* Metadata from taglib */

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  /* Information about the file on disk */

  directory_id INTEGER NOT NULL,
  filename TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL,
  mtime INTEGER NOT NULL,
  ctime INTEGER NOT NULL,
  unavailable INTEGER DEFAULT 0,

  /* Other */

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT,
  art_manual TEXT,

  effective_albumartist TEXT,
  effective_originalyear INTEGER NOT NULL DEFAULT 0,
  
  cue_path TEXT

);

CREATE TABLE playlists (

  name TEXT NOT NULL,
  last_played INTEGER NOT NULL DEFAULT -1,
  ui_order INTEGER NOT NULL DEFAULT 0,
  special_type TEXT,
  ui_path TEXT,
  is_favorite INTEGER NOT NULL DEFAULT 0

);

CREATE TABLE playlist_items (

  playlist INTEGER NOT NULL,
  type TEXT NOT NULL,
  collection_id INTEGER,
  url TEXT,

  /* Metadata from taglib */

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  /* Information about the file on disk */

  directory_id INTEGER,
  filename TEXT,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER,
  mtime INTEGER,
  ctime INTEGER,
  unavailable INTEGER DEFAULT 0,

  /* Other */

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT,
  art_manual TEXT,

  effective_albumartist TEXT,
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT

);

CREATE TABLE devices (
  unique_id TEXT NOT NULL,
  friendly_name TEXT,
  size INTEGER,
  icon TEXT,
  schema_version INTEGER NOT NULL DEFAULT 0,
  transcode_mode NOT NULL DEFAULT 3,
  transcode_format NOT NULL DEFAULT 5
);

CREATE INDEX idx_filename ON songs (filename);

CREATE INDEX idx_comp_artist ON songs (compilation_effective, artist);

CREATE INDEX idx_album ON songs (album);

CREATE INDEX idx_title ON songs (title);

CREATE VIEW duplicated_songs as select artist dup_artist, album dup_album, title dup_title from songs as inner_songs where artist != '' and album != '' and title != '' and unavailable = 0 group by artist, album , title having count(*) > 1;

CREATE VIRTUAL TABLE songs_fts USING fts3(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize=unicode

);

CREATE VIRTUAL TABLE playlist_items_fts_ USING fts3(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize=unicode

);

CREATE VIRTUAL TABLE %allsongstables_fts USING fts3(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize=unicode

);


INSERT INTO songs_fts		(ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment FROM songs;

INSERT INTO %allsongstables_fts (ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment FROM %allsongstables;
