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

  title TEXT,
  titlesort TEXT,
  album TEXT,
  albumsort TEXT,
  artist TEXT,
  artistsort TEXT,
  albumartist TEXT,
  albumartistsort TEXT,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT,
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT,
  composersort TEXT,
  performer TEXT,
  performersort TEXT,
  grouping TEXT,
  comment TEXT,
  lyrics TEXT,

  artist_id TEXT,
  album_id TEXT,
  song_id TEXT,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_embedded INTEGER DEFAULT 0,
  art_automatic TEXT,
  art_manual TEXT,
  art_unset INTEGER DEFAULT 0,

  effective_albumartist TEXT,
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT,

  rating INTEGER DEFAULT -1,

  acoustid_id TEXT,
  acoustid_fingerprint TEXT,

  musicbrainz_album_artist_id TEXT,
  musicbrainz_artist_id TEXT,
  musicbrainz_original_artist_id TEXT,
  musicbrainz_album_id TEXT,
  musicbrainz_original_album_id TEXT,
  musicbrainz_recording_id TEXT,
  musicbrainz_track_id TEXT,
  musicbrainz_disc_id TEXT,
  musicbrainz_release_group_id TEXT,
  musicbrainz_work_id TEXT,

  ebur128_integrated_loudness_lufs REAL,
  ebur128_loudness_range_lu REAL,

  bpm REAL,
  mood TEXT,
  initial_key TEXT

);

CREATE INDEX idx_device_%deviceid_songs_album ON device_%deviceid_songs (album);

CREATE INDEX idx_device_%deviceid_songs_comp_artist ON device_%deviceid_songs (compilation_effective, artist);

UPDATE devices SET schema_version=6 WHERE ROWID=%deviceid;
