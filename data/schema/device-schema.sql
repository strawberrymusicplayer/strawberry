CREATE TABLE device_%deviceid_directories (
  path TEXT NOT NULL,
  subdirs INTEGER NOT NULL
);

CREATE TABLE device_%deviceid_subdirectories (
  directory_id INTEGER NOT NULL,
  path TEXT NOT NULL,
  mtime INTEGER NOT NULL
);

CREATE TABLE device_%deviceid_songs (

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
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id INTEGER NOT NULL DEFAULT -1,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  filename TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

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

CREATE INDEX idx_device_%deviceid_songs_album ON device_%deviceid_songs (album);

CREATE INDEX idx_device_%deviceid_songs_comp_artist ON device_%deviceid_songs (compilation_effective, artist);

CREATE VIRTUAL TABLE device_%deviceid_fts USING fts3(
  ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment,
  tokenize=unicode
);

UPDATE devices SET schema_version=0 WHERE ROWID=%deviceid;

