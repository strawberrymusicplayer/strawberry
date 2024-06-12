DROP TABLE IF EXISTS %allsongstables_fts;

DROP TABLE IF EXISTS songs_fts;

DROP TABLE IF EXISTS subsonic_songs_fts;

DROP TABLE IF EXISTS tidal_artists_songs_fts;

DROP TABLE IF EXISTS tidal_albums_songs_fts;

DROP TABLE IF EXISTS tidal_songs_fts;

DROP TABLE IF EXISTS qobuz_artists_songs_fts;

DROP TABLE IF EXISTS qobuz_albums_songs_fts;

DROP TABLE IF EXISTS qobuz_songs_fts;

UPDATE schema_version SET version=19;
