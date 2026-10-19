UPDATE plex_songs SET art_automatic = substr(art_automatic, 1, instr(art_automatic, '?') - 1) WHERE art_automatic LIKE '%X-Plex-Token=%' AND instr(art_automatic, '?') > 0;

UPDATE playlist_items SET art_automatic = substr(art_automatic, 1, instr(art_automatic, '?') - 1) WHERE source = 13 AND art_automatic LIKE '%X-Plex-Token=%' AND instr(art_automatic, '?') > 0;

UPDATE schema_version SET version=30;
