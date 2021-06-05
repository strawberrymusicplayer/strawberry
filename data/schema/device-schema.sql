CREATE TABLE device_%deviceid_directories (
  path TEXT NOT NULL DEFAULT '',
  subdirs INTEGER NOT NULL
);

CREATE TABLE device_%deviceid_subdirectories (
  directory_id INTEGER NOT NULL,
  path TEXT NOT NULL DEFAULT '',
  mtime INTEGER NOT NULL
);

CREATE TABLE device_%deviceid_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE INDEX idx_device_%deviceid_songs_album ON device_%deviceid_songs (album);

CREATE INDEX idx_device_%deviceid_songs_comp_artist ON device_%deviceid_songs (compilation_effective, artist);

CREATE VIRTUAL TABLE device_%deviceid_fts USING fts5(
  ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment,
  tokenize = "unicode61 remove_diacritics 1"
);

UPDATE devices SET schema_version=2 WHERE ROWID=%deviceid;
