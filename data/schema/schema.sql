CREATE TABLE IF NOT EXISTS schema_version (
  version INTEGER NOT NULL
);

DELETE FROM schema_version;

INSERT INTO schema_version (version) VALUES (21);

CREATE TABLE IF NOT EXISTS directories (
  path TEXT NOT NULL,
  subdirs INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS subdirectories (
  directory_id INTEGER NOT NULL,
  path TEXT NOT NULL,
  mtime INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS songs (

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

CREATE TABLE IF NOT EXISTS subsonic_songs (

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

CREATE TABLE IF NOT EXISTS tidal_artists_songs (

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

CREATE TABLE IF NOT EXISTS tidal_albums_songs (

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

CREATE TABLE IF NOT EXISTS tidal_songs (

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

CREATE TABLE IF NOT EXISTS spotify_artists_songs (

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

CREATE TABLE IF NOT EXISTS spotify_albums_songs (

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

CREATE TABLE IF NOT EXISTS spotify_songs (

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

CREATE TABLE IF NOT EXISTS qobuz_artists_songs (

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

CREATE TABLE IF NOT EXISTS qobuz_albums_songs (

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

CREATE TABLE IF NOT EXISTS qobuz_songs (

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

CREATE TABLE IF NOT EXISTS playlists (

  name TEXT NOT NULL,
  last_played INTEGER NOT NULL DEFAULT -1,
  ui_order INTEGER NOT NULL DEFAULT 0,
  special_type TEXT,
  ui_path TEXT,
  is_favorite INTEGER NOT NULL DEFAULT 0,

  dynamic_playlist_type INTEGER,
  dynamic_playlist_backend TEXT,
  dynamic_playlist_data BLOB

);

CREATE TABLE IF NOT EXISTS playlist_items (

  playlist INTEGER NOT NULL,
  type INTEGER NOT NULL DEFAULT 0,
  collection_id INTEGER,
  playlist_url TEXT,

  title TEXT,
  titlesort TEXT,
  album TEXT,
  albumsort TEXT,
  artist TEXT,
  artistsort TEXT,
  albumartist TEXT,
  albumartistsort TEXT,
  track INTEGER,
  disc INTEGER,
  year INTEGER,
  originalyear INTEGER,
  genre TEXT,
  compilation INTEGER DEFAULT 0,
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

  beginning INTEGER,
  length INTEGER,

  bitrate INTEGER,
  samplerate INTEGER,
  bitdepth INTEGER,

  source INTEGER,
  directory_id INTEGER,
  url TEXT NOT NULL,
  filetype INTEGER,
  filesize INTEGER,
  mtime INTEGER,
  ctime INTEGER,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT,

  playcount INTEGER DEFAULT 0,
  skipcount INTEGER DEFAULT 0,
  lastplayed INTEGER DEFAULT -1,
  lastseen INTEGER DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER DEFAULT 0,
  compilation_off INTEGER DEFAULT 0,
  compilation_effective INTEGER DEFAULT 0,

  art_embedded INTEGER DEFAULT 0,
  art_automatic TEXT,
  art_manual TEXT,
  art_unset INTEGER DEFAULT 0,

  effective_albumartist TEXT,
  effective_originalyear INTEGER,

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

CREATE TABLE IF NOT EXISTS devices (
  unique_id TEXT NOT NULL,
  friendly_name TEXT,
  size INTEGER,
  icon TEXT,
  schema_version INTEGER NOT NULL DEFAULT 0,
  transcode_mode NOT NULL DEFAULT 3,
  transcode_format NOT NULL DEFAULT 5
);

CREATE TABLE IF NOT EXISTS radio_channels (
  source INTEGER NOT NULL DEFAULT 0,
  name TEXT,
  url TEXT NOT NULL,
  thumbnail_url TEXT
);

CREATE INDEX IF NOT EXISTS idx_url ON songs (url);

CREATE INDEX IF NOT EXISTS idx_comp_artist ON songs (compilation_effective, artist);

CREATE INDEX IF NOT EXISTS idx_albumartist ON songs (albumartist);

CREATE INDEX IF NOT EXISTS idx_albumartistsort ON songs (albumartistsort);

CREATE INDEX IF NOT EXISTS idx_artist ON songs (artist);

CREATE INDEX IF NOT EXISTS idx_artistsort ON songs (artistsort);

CREATE INDEX IF NOT EXISTS idx_album ON songs (album);

CREATE INDEX IF NOT EXISTS idx_albumsort ON songs (album);

CREATE INDEX IF NOT EXISTS idx_title ON songs (title);

CREATE INDEX IF NOT EXISTS idx_titlesort ON songs (title);

CREATE INDEX IF NOT EXISTS idx_composersort ON songs (title);

CREATE INDEX IF NOT EXISTS idx_performersort ON songs (title);

CREATE VIEW IF NOT EXISTS duplicated_songs as select artist dup_artist, album dup_album, title dup_title from songs as inner_songs where artist != '' and album != '' and title != '' and unavailable = 0 group by artist, album , title having count(*) > 1;
