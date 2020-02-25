DROP TABLE IF EXISTS tidal_artists_songs;

DROP TABLE IF EXISTS tidal_albums_songs;

DROP TABLE IF EXISTS tidal_songs;

DROP TABLE IF EXISTS qobuz_artists_songs;

DROP TABLE IF EXISTS qobuz_albums_songs;

DROP TABLE IF EXISTS qobuz_songs;

DROP TABLE IF EXISTS tidal_artists_songs_fts;

DROP TABLE IF EXISTS tidal_albums_songs_fts;

DROP TABLE IF EXISTS tidal_songs_fts;

DROP TABLE IF EXISTS qobuz_artists_songs_fts;

DROP TABLE IF EXISTS qobuz_albums_songs_fts;

DROP TABLE IF EXISTS qobuz_songs_fts;

UPDATE schema_version SET version=10;
