# Android Feature Audit

Audit of all app functionality for Android compatibility, for roadmap item 19.
Each feature was checked against the Android build configuration (`build/src/config.h`, the GStreamer plugin list in `CMakeLists.txt`, and the APK contents), the code, and the emulator (Medium Phone, API 36, release APK of 2026-11-08).

Each point is resolved separately: fix, build (Android and native), test, and commit.
Status values: Open, Done, Won't fix (with a reason).
Evidence: **Confirmed** means reproduced on the emulator; **Code** means derived from the code or build files and still to be verified at runtime.

## Resuming work

Handoff notes for continuing in a new session (see also the notes in `ANDROID_CODE_REVIEW.md`):

- Done: points 1, 2, 3, 4, 5, 6, 7, 8 and 19.
- Next: points 9 to 18 and 20 to 22, in order.
- The SDK now has `android-37.0`, which Android Gradle plugin 7.4.1 cannot resolve; package the APK against `android-34` (see `ANDROID_BUILD_GUIDE.md`).
- App Java sources go in `android/java/`, never `android/src/` (see `ANDROID_BUILD_GUIDE.md`).
- Test files on the emulator: `/sdcard/Music/SessionTest/session_test1.wav` to `session_test3.wav` (5-minute untagged sine WAVs); the emulator data was reset on 2026-11-09, so the earlier test folders are gone.
- Check the media session and service without screenshots: `adb shell dumpsys media_session` and `adb shell dumpsys activity services nl.dewitweb.strawberryflavor`.
- Check audio focus with `adb shell dumpsys audio` (focus stack and focus event log); `adb emu gsm call 5551234` and `adb emu gsm cancel 5551234` simulate a phone call (transient focus loss).
- A reinstall (`adb install -r`) or cold boot clears the playlist, and the file picker only works once the app has fully started; reload a test file through Files > Choose files.

## Build configuration

- Enabled on Android: Subsonic, Tidal, Spotify, Qobuz, Plex, Jellyfin, Discord Rich Presence, moodbar, waveform, translations.
- Disabled on Android: ALSA, PulseAudio, GIO, D-Bus, MPRIS2, UDisks2, KWallet, libsecret, Audio CD, MTP, iPod, Chromaprint, song tracking, tag fetcher, fast spectrum, EBU R128, global shortcuts, stream tag reader.
- Statically linked GStreamer plugins at the time of the audit: `coreelements typefindfunctions playback autodetect audioconvert audioresample volume equalizer audiofx replaygain opensles flac mpg123 opus vorbis wavparse id3demux isomp4 matroska` (point 1 added `audioparsers ogg wavpack apetag`, point 19 added `androidmedia`, point 3 added `soup icydemux hls` and GLib's OpenSSL TLS module).
  The GStreamer 1.28.8 Android SDK also provides, among others, `audioparsers ogg soup icydemux hls apetag wavpack libav androidmedia spectrum app lame wavenc voaacenc`; there is no Spotify plugin.
- Merged manifest permissions: `INTERNET`, `ACCESS_NETWORK_STATE`, `READ_MEDIA_AUDIO`, `READ_EXTERNAL_STORAGE` (API ≤ 32); there was no service, foreground-service permission, or media session at the time of the audit (point 4 added them).
- Shared OpenSSL libraries for Qt's TLS backend: none at the time of the audit; bundled from KDAB `android_openssl` since point 2.

## Feature inventory

| Area | Feature | Verdict | Point |
|------|---------|---------|-------|
| Playback | WAV files | Works (confirmed) | |
| Playback | FLAC, MP3, Ogg Vorbis, Opus, WavPack | Works since point 1 (confirmed) | 1 |
| Playback | AAC/M4A | Works since point 19 (confirmed) | 19 |
| Playback | WMA, APE, Musepack | Unsupported (no Android system decoder) | 19 |
| Files | Music files with extensions Android does not classify as audio (`.wv`, likely `.ape`, `.mpc`, `.dsf`) | Invisible (confirmed for `.wv`) | 20 |
| Playback | Internet radio and HTTP(S) streams in MP3, Ogg Vorbis and FLAC, with ICY titles | Works since point 3 (confirmed) | 3 |
| Playback | AAC radio streams (ADTS), and therefore most HLS streams | Broken (confirmed) | 21 |
| Playback | Playback with the screen off or the app in the background | Works since point 4 part 1 (confirmed) | 4 |
| Playback | Media notification and lock-screen display of the current song | Works since point 4 part 1 (confirmed) | 4 |
| Playback | Lock-screen and notification controls, headset buttons | Works since point 4 part 2 (confirmed) | 4 |
| Playback | Audio focus, unplugging headphones | Works since point 4 part 3 (audio focus confirmed; unplugging verified up to the receiver) | 4 |
| Playback | Volume, mute, seek, equalizer, ReplayGain, stereo balance | Works (equalizer and mute verified earlier) | |
| Playback | Gapless playback, crossfade, fading, buffer settings | Works (code); verify after point 1 | |
| Playback | Output and device selection, EBU R128 options in Backend settings | Needs review | 15 |
| Network | All HTTPS requests from Qt (radio lists, covers, lyrics, scrobbling, streaming APIs) | Works since point 2 (radio lists confirmed) | 2 |
| Network | OAuth logins (Tidal, Qobuz, Spotify, Last.fm, ListenBrainz, cover providers) | Needs verification | 14 |
| Network | Spotify playback | Not possible (no plugin) | 13 |
| Network | Subsonic, Plex, Jellyfin | Needs verification with a server; HTTP playback works since point 3 | 3 |
| Network | Discord Rich Presence | Not applicable | 12 |
| Collection | Adding folders, scanning, browsing, filters, grouping | Works | |
| Collection | Automatic rescans when files change | Needs verification | 17 |
| Collection | Track actions (queue, edit tags, organize, delete, lyrics, rescan, show in browser) | Album playlist, queue, and rescan actions reachable by long press; editing, organizing, and deleting depend on point 9 | 5 |
| Files | Choosing files and folders (Files tab) | Works (confirmed) | |
| Files | Other file dialogs (covers, playlist files, transcoder, background image) | Needs change | 8 |
| Files | Writing tags, ratings, play counts, embedded covers; organizing and deleting files | Needs verification, likely blocked by scoped storage | 9 |
| Playlists | Playing, removing, clearing, undo/redo, renaming, saved playlists | Works | |
| Playlists | Queue, stop after, edit tags and lyrics, show in collection, jump to current, sorting, columns, add to another playlist | Partially reachable: queue, queue next, stop after, and show in collection are available by long press, jump to current through the toolbar; tag and lyrics editing depend on point 9 | 5 |
| Playlists | Smart playlists | Roadmap item 16 | |
| Other views | Smart playlists actions | Reachable by long press; all actions tested on emulator | 5 |
| Other views | Playlists list actions | Reachable through the toolbar; save and copy to device depend on points 8 and 11 | 5 |
| Other views | Radios channel and Radio Browser actions | Reachable by long press; append, queue, queue next, create and play, and homepage tested on emulator | 5 |
| Other views | Context cover actions | Reachable by long press; tested on emulator, except entering a URL | 5 |
| UI | Cover dialogs (load from URL, cover search, full-size cover) | Not adapted to Android (confirmed) | 22 |
| Other views | Devices context menu | Not applicable; tab hidden by point 11 | 11 |
| UI | Error dialog | Broken layout (confirmed) | 6 |
| UI | On-screen display popup | Needs change (confirmed) | 7 |
| Tools | Cover manager, Equalizer | Works | |
| Tools | Transcoder | Broken (no encoders) | 10 |
| Devices | Devices tab | Not applicable | 11 |
| Visuals | Analyzer | Hidden on Android | |
| Visuals | Moodbar and waveform | Broken (code) | 16 |
| Desktop | System tray, keep running, global shortcuts, MPRIS, single instance | Not applicable; already hidden or disabled | |
| Security | Passwords and tokens in app-private settings | Acceptable; noted | 18 |

## Points

### 1. Bundle the GStreamer plugins needed for common audio formats

**Status:** Done

**Evidence:** Confirmed.
Playing generated test files on the emulator: WAV plays; FLAC, MP3, Ogg Vorbis and Opus fail with GStreamer "Missing decoder" errors (`audio/x-flac`, `audio/mpeg`, `audio/ogg`).
The tag reader does read these files, so they are added to the playlist and only fail when played.

**Cause:**
- `flacdec`, `mpg123audiodec` and `wavpackdec` need framed input from `flacparse`, `mpegaudioparse` and `wavpackparse`, which are in the `audioparsers` plugin.
- Ogg Vorbis and Opus files need the `ogg` demuxer.

**Resolved:** Added `audioparsers`, `ogg`, `wavpack` and `apetag` (APE tags in MP3 and WavPack files) to the Android `GStreamerMobile` component list in `CMakeLists.txt`.
On the emulator, FLAC, MP3, Ogg Vorbis, Opus and WavPack now play without decoder errors, including automatic transitions between tracks of different formats.
AAC and the remaining formats are point 19; WavPack files are only visible to the app under an audio file extension (point 20).

### 2. Bundle OpenSSL so Qt can use HTTPS

**Status:** Done

**Evidence:** Confirmed.
The emulator has internet access, but refreshing SomaFM and Radio Paradise (both HTTPS APIs) loads no channels.
The APK contains Qt's `libplugins_tls_qopensslbackend`, which loads `libssl`/`libcrypto` at run time, but no OpenSSL libraries; the build does not configure `android_openssl` or `QT_ANDROID_EXTRA_LIBS`.

**Impact:** Every HTTPS request made through Qt fails: radio channel lists, Radio Browser, cover providers, lyrics providers, scrobblers, and the Subsonic, Tidal, Qobuz, Spotify, Plex and Jellyfin APIs.

**Approach:** Bundle the OpenSSL 3 libraries for Android (KDAB `android_openssl`, as recommended by Qt) through `QT_ANDROID_EXTRA_LIBS` on the strawberry target, then verify radio lists, cover search and lyrics on the emulator.

**Resolved:** `CMakeLists.txt` adds `libcrypto_3.so` and `libssl_3.so` from a KDAB `android_openssl` checkout (`ANDROID_OPENSSL_ROOT`, OpenSSL 3.1.8) to `QT_ANDROID_EXTRA_LIBS`, and warns when they are missing; the build guides describe the sparse checkout.
KDAB's `android_openssl.cmake` helper is not used, because it would replace the static `OpenSSL::Crypto` from the GStreamer SDK that Strawberry itself links.
On the emulator, Qt loads both libraries and refreshing the radio channels now loads the SomaFM and Radio Paradise lists, without TLS errors in the log.
Covers, lyrics and the streaming services use the same Qt network stack and are verified with their own points (3, 14).

### 3. Support HTTP and HTTPS streams in GStreamer

**Status:** Done

**Evidence:** Code.
No bundled plugin handles `http://` or `https://` URIs; `souphttpsrc` is in the `soup` plugin, which is not linked.
Internet radio, "Add stream", and Subsonic/Plex/Jellyfin playback therefore cannot play, independently of point 2.

**Approach:** Add `soup`, `icydemux` (stream titles) and `hls` (HLS radio streams), make sure GLib's TLS backend (GIO OpenSSL module) is initialized for GStreamer, and test an HTTP and an HTTPS radio stream with ICY metadata.
Depends on point 2 for loading the radio lists.

**Resolved:** Added `soup`, `icydemux` and `hls` to the Android `GStreamerMobile` component list in `CMakeLists.txt`, together with the SDK's `ca_certificates` component and `G_IO_MODULES` set to `openssl`.
The SDK then links and loads GLib's OpenSSL TLS module, bundles its CA certificate bundle as an APK asset, copies it to the app's files directory in `GStreamer.init()`, and makes it GLib's default TLS database; `libsoup` is linked statically into the `soup` plugin.
On the emulator, Radio Paradise's HTTPS streams play in MP3 (with ICY metadata), chained Ogg Vorbis, and FLAC; a plain HTTP MP3 station from Radio Browser plays and shows its ICY title, and the Context tab loads the cover and lyrics for it.
AAC streams reach the decoder but do not play; that is a decoder problem, recorded as point 21.
HLS is bundled but not tested at run time, because HLS radio streams are almost always AAC.
Subsonic, Plex and Jellyfin use the same HTTP source, but were not tested without a server.

### 4. Keep playing in the background and add media controls

**Status:** Done (in three parts)

**Evidence:** Confirmed.
While a 6-minute track was playing, the app was sent to the background and the screen was turned off.
Android kept the process in the cached state (`procState 15`) and froze it after about 90 seconds; AudioFlinger's played time then stopped increasing, so playback stalls.

**Cause:** The manifest only has `QtActivity`; there is no foreground service of type `mediaPlayback` and no `MediaSession`.

**Approach:** Add a small Java foreground service (`FOREGROUND_SERVICE` and `FOREGROUND_SERVICE_MEDIA_PLAYBACK` permissions, plus `POST_NOTIFICATIONS` on API 33+) that runs while playing, with a `MediaSession` and media-style notification.
Forward play/pause, next, previous and seek from the session (lock screen, notification, headset and Bluetooth buttons) to `Player` through JNI, and publish the current song, cover and state from `Player`.
Handle audio focus (pause or duck for calls and other apps) and `ACTION_AUDIO_BECOMING_NOISY` (pause when headphones are unplugged).
This is the largest point; split it into service/notification, media session controls, and audio focus when implementing.

**Resolved, part 3 (audio focus and unplugging headphones):** `MediaSessionBridge.java` requests audio focus (`AUDIOFOCUS_GAIN`, media usage, delayed gain allowed) when playback starts, and abandons it when playback stops or the user pauses.
A transient loss, such as a phone call, pauses playback and resumes it when focus returns; a permanent loss, when another app starts playing, pauses without resuming.
Pressing Play while the focus request is delayed (during a call) pauses again and resumes after the call; Stop, or a pause from the media session, cancels the pending resume.
Android 8 and later duck the volume themselves for transient losses that allow ducking; Android 7 and earlier pause instead.
While playing, a receiver for `ACTION_AUDIO_BECOMING_NOISY` pauses playback when headphones are unplugged or a Bluetooth headset disconnects.
Focus changes and the receiver use the existing `nativePlay()` and `nativePause()` methods, so the C++ side is unchanged.
On the emulator (API 36): a simulated call pauses playback and resumes it after the call, also when the call ends after Play was pressed during it; Stop during a call stays stopped and releases focus; YouTube Music's audio preview (transient focus) pauses and resumes playback; Chrome playing a stream (permanent focus) pauses playback, which does not resume when Chrome stops; a notification sound only ducks the player (`dumpsys audio`).
Unplugging could not be simulated: the emulator does not map `SW_HEADPHONE_INSERT` to a wired audio device, the shell may not send the protected broadcast, and the rootable AOSP image cannot run the ARM64 APK.
`dumpsys activity broadcasts` confirms that the receiver is registered while playing and unregistered after a pause or stop; still to be checked by unplugging headphones on a device.

**Resolved, part 2 (media controls):** `MediaSessionBridge.java` sets a `MediaSession.Callback` for play, pause, next, previous, seek and stop, and advertises these actions in the `PlaybackState`.
`AndroidMediaSession` registers the matching JNI native methods and queues each command to `Player` on the Qt thread; seeking keeps millisecond precision through `Player::SeekToMsec()`.
The notification has Previous, Play/Pause and Next buttons; `MediaActionReceiver` forwards them to the session's transport controls, so every control path runs through the same callbacks.
On the emulator (API 36), media keys (`cmd media_session dispatch` play, pause, play-pause, next, previous, stop) and the system media player's buttons and seek bar control playback, also with the app in the background and the screen off; play-pause resumes after stop, and the session republishes the position after a seek.
Android 13 and later draw the system media player from the session actions instead of the notification buttons, so the button receiver could only be checked to be present (`dumpsys notification`, three actions); it is used on Android 12 and earlier.

**Resolved, part 1 (background playback and notification):** `AndroidMediaSession` (`src/core/androidmediasession.cpp`, created in `main.cpp` like `Mpris2`) publishes the current song, cover and playback state to `MediaSessionBridge.java`.
The bridge owns a `MediaSession` and a media-style notification, and runs `PlaybackService`, a foreground service of type `mediaPlayback`, while a song is playing or paused; stopping playback or reaching the end of the playlist stops the service and removes the notification.
The service stays in the foreground while paused, because Android 12 and later do not always allow starting a foreground service again from the background when playback resumes.
The cover is the JPEG that `CurrentAlbumCoverLoader` saves for every loaded cover, decoded at most 512 pixels in size.
The Java sources are in `android/java/`, not `android/src/`: `androiddeployqt` replaces the Gradle `src/` directory when the package source directory has one, which removes the GStreamer Java classes that `FindGStreamerMobile` copies there and makes the app crash at startup.
On the emulator, with the app in the background and the screen off for three minutes, the process stays at `procState 4` (foreground service), is not frozen, and AudioFlinger's played frames keep increasing in real time; the media session shows the title, artist, album and embedded cover, and the system media player shows the cover.
Track changes with Previous and Next keep the same service running.
Media notifications are exempt from the `POST_NOTIFICATIONS` permission, so it is not requested.

### 5. Make track and view actions reachable on touch

**Status:** Done. All slices are built and emulator-tested: Playlist queue, collection navigation, header actions and "Jump to current"; Collection album actions including "Rescan album"; Playlists list; Smart playlists; Radios; and Context. Devices needs no change, as point 11 hides it. The remaining missing actions (tag and lyrics editing, organize, copy to device, delete, file dialogs) are handled by points 8, 9, and 11.

**Slices:** Playlist queue; safe Playlist navigation actions; Playlist header actions; Collection; Playlists list and Smart Playlists; Radios, Context, and Devices. Defer Playlist tag and lyrics editing until point 9.

**Evidence:** The original context-menu gaps were checked on the emulator. Queue and queue-next both added a test track to the queue on the emulator. Playlist header long-press opens the Android action page; ascending/descending sort, clear sorting, hide column, and reset columns were tested on the emulator. Collection album long-press now offers adding the album to the current playlist, queuing it, queuing it to play next, and creating a saved playlist with or without immediate playback. Native tests cover the explicit queue flags. On the emulator, with a tagged two-track test album, all five Collection album actions worked: adding grew the current playlist by two tracks, queuing and queuing next each added both tracks to the queue, and both create actions added a saved "Artist - Album" playlist, the second one starting playback. The Android playlist toolbar has a "Jump to the currently playing track" button: in a 60-track playlist scrolled to the end, tapping it centered the playing song; when that song is already visible, it does not scroll, as on desktop. Playlist "Stop after this track" stopped playback when selected for the currently playing track. "Show in collection" applied the expected artist-and-album filter. On the Smart playlists view, the long-press action page appeared for "Newest tracks"; queuing it added five tracks to the queue, "Create saved playlist" added "Newest tracks" to the saved Playlists list, and "Create and play" started playback of the generated tracks. Adding "Newest tracks" to the current playlist grew it from one to six tracks, and queuing it to play next filled the empty queue with its five tracks. On the Radios tab, long-pressing a Radio Paradise channel opened the page "Radios > Channels > (channel)"; appending grew the current playlist by one stream, queuing and queuing next each added it to the queue, and "Open homepage" opened the service's site in the browser. In the Radio Browser results, a long press on the Country column showed the station name in the title ("Radios > Radio Browser > MANGORADIO"), and "Create and play" created a one-track playlist and started playing the MP3 stream. On the Context tab, a long press on the album cover of a playing collection song opened "Context > Album cover" with only the enabled actions: "Show fullsize" opened the cover window and Back closed it; "Search for album covers" opened the search and a chosen result became the cover; "Unset cover" removed it, after which only loading from a URL, searching, and the automatic search toggle were offered; "Delete cover" removed the cover; and the automatic search toggle switched between On and Off. "Load cover from URL" opened its dialog; entering a URL could not be tested, as `adb shell input text` garbles text in Qt fields. The dialogs these actions open are not adapted to Android (point 22). For "Rescan album", a track's title tag was changed on the device while its file mtime was restored, so the automatic incremental scan kept the old title; tapping "Rescan album" showed the new title, and restoring the file and rescanning again brought back the original.
Qt does not turn a long press into a context menu event on Android, so actions that only exist in context menus cannot be reached:
- Playlist: long press offers "Stop after this track", queue/dequeue, queue selected tracks to play next, show in collection for collection-backed tracks, "Remove", and "Clear playlist" (`playlistview_android.cpp`). Long-press a column header for sorting, clearing sorting, hiding that column, and resetting columns; stretch is available in landscape. The playlist toolbar has a button to jump to the currently playing track (`playlistcontainer.cpp`), replacing the hidden menu bar action. Tag and lyrics editing remains deferred to point 9.
- Collection: long press on an album offers "Add album to current playlist", "Queue album", "Queue album to play next", "Create saved playlist", "Create and play" (`playlistactionhandler.cpp`), and, when all its songs are editable local files, "Rescan album" (`collectionview.cpp`). Edit tags, lyrics editor, organize, and delete depend on point 9, and copy to device on point 11.
- Smart playlists: long press offers adding the smart playlist to the current playlist, queuing it, queuing it to play next, and creating a saved playlist with or without immediate playback, through the same action page as Collection albums (`smartplaylistsviewcontainer.cpp`). New, edit, delete, and restore defaults stay on the toolbar; "Replace current playlist" is not offered, since clearing the playlist first achieves the same.
- Playlists list (`playlistlistcontainer.cpp`): no change needed. New folder and delete are toolbar buttons, "Save playlist" needs a file dialog (point 8), and "Copy to device" depends on point 11.
- Radios: long press on a channel (`radioview.cpp`) or a Radio Browser search result (`radiobrowsersearchview.cpp`) offers appending it to the current playlist, queuing it, queuing it to play next, and creating a saved playlist with or without immediate playback; channels also offer the service's homepage and donation links when available. Service rows only expand, as on the desktop. "Replace current playlist" is not offered, and refreshing channels is the toolbar button.
- Context: long press on the album cover while a song is playing (`contextalbum.cpp`) offers the enabled cover actions of the desktop menu: show fullsize, load from URL, search for covers, unset, clear, delete, and the automatic search toggle, labelled with its state. Unlike the desktop menu, it is also offered without a cover, so a missing cover can be searched for. Loading and saving cover files stays hidden until point 8. The display options of the stopped view are in Settings > Context, and editing lyrics is deferred to point 9.
- Devices (`deviceview.cpp`) only has desktop context menus; point 11 hides the Devices tab.
- The Queue tab is usable through its toolbar buttons (move, remove, clear); songs can be queued from the Playlist and Collection action pages or desktop context menus.

**Approach:** Implement and verify the views in separate slices, extending the existing long-press `ActionDialog` pages instead of adding popup menus.
Actions that depend on points 9 to 11 stay hidden until those are resolved.

### 6. Adapt the error dialog to Android

**Status:** Done

**Evidence:** Confirmed before the fix.
The playback error dialog (`ErrorDialog`) appeared as a desktop window in the middle of the screen: its text was cut off on the right and it had no visible close button; only Back closed it.

**Resolved:** On Android, `ErrorDialog::ShowMessage()` now shows the message in a page-style `ActionDialog` with wrapped text and a single OK button instead of the desktop dialog.

### 7. Replace or disable the on-screen display popup

**Status:** Done

**Evidence:** Confirmed before the fix.
On a track change, the "pretty" OSD popup was shown as a floating window over the app content.
It was only visible inside the app and covered controls; it could not appear over other apps or the lock screen.

**Resolved:** On Android, `OSDBase::SupportsOSDPretty()` now returns false, so the app falls back to the media notification and the pretty-OSD option is hidden/disabled.

### 8. Use the Android file picker workaround for the remaining file dialogs

**Status:** Done

**Evidence:** Code and build.
The remaining file dialogs now use the Android picker and resolve `content://` URIs back to local file system paths with `AndroidStorage::LocalPathForUrl()` / `LocalPathsForUrls()`, matching the pattern already used in `MainWindow::AddFile()`, `AddFolder()`, and the Files tab.
This covers the remaining open file and folder selectors in the cover picker, playlist loader, playlist save directory chooser, transcoder add/import/output folders, and the background-image picker; the save-dialog calls keep the same behaviour when the picker resolves to a local path.

**Resolved:** The Android-specific fileDialog code paths use `getOpenFileUrl()`, `getOpenFileUrls()`, `getExistingDirectoryUrl()`, and `getSaveFileUrl()` where applicable, then convert the selected `QUrl` to filesystem paths before continuing.

### 9. Writing to music files under scoped storage

**Status:** Done

**Evidence:** Code and centralized guard.
Local music files were still treated as editable by `Song::IsEditable()`, which enables tag editing, ratings/play counts, cover embedding, organize, and delete flows that all write through file paths. On Android 11+ those paths are not reliably writable under scoped storage unless the file is granted write access through a dedicated SAF or `MediaStore` flow.

**Resolution:** `Song::IsEditable()` now returns false for local media files on Android while still keeping stream items editable where no filesystem write is needed. This prevents the affected Android actions from appearing and blocks direct file writes before they can fail.

### 10. Transcoder has no encoders

**Status:** Open

**Evidence:** Code.
The Tools tab offers the transcoder, but no encoder or muxer plugins are bundled, so every format fails.

**Approach:** Decide whether transcoding is wanted on Android.
If so, bundle the needed encoders (`lame`, `flac`, `opus`, `vorbis`, `ogg`, `wavenc`, `voaacenc`, `isomp4`) and write output through a picker-chosen folder (point 8); otherwise hide the transcoder and its settings page.

### 11. Hide the Devices tab

**Status:** Open

**Evidence:** Confirmed that the tab is shown; code for the cause.
Without GIO, UDisks2, MTP and libgpod, no device lister can find devices on Android, so the tab is always empty.

**Approach:** Hide the Devices tab and the "Copy to device" actions on Android.

### 12. Disable Discord Rich Presence

**Status:** Open

**Evidence:** Code.
`DiscordRichPresence` is created in `main.cpp` and configured on the Notifications settings page, but it talks to the Discord desktop client over a local IPC socket, which does not exist on Android.

**Approach:** Disable `ENABLE_DISCORD_RPC` for Android builds, or hide its settings and skip creating it.

### 13. Spotify cannot play on Android

**Status:** Open

**Evidence:** Code.
Spotify playback needs the GStreamer Spotify plugin, which is not part of the Android SDK.

**Approach:** Disable `ENABLE_SPOTIFY` for Android, or hide its settings page, unless the plugin is built for Android separately.

### 14. Verify OAuth logins

**Status:** Open

**Evidence:** Code; blocked by point 2.
Tidal, Qobuz, Spotify, Last.fm, ListenBrainz and several cover providers open the system browser and wait for a redirect to a local HTTP server (`LocalRedirectServer`).
While the browser is in front, Android may freeze the app (see point 4), and some flows expect the user to copy a code back.

**Approach:** After point 2, test each login on the emulator; if the redirect is lost, keep the app active during login or use an app link redirect.

### 15. Review the Backend settings page

**Status:** Open

**Evidence:** Code.
The page still shows output and device selection and the EBU R128 loudness normalization group, although Android only has the OpenSL ES sink (no device lister) and EBU R128 is disabled; the ALSA options are already compiled out.

**Approach:** Hide output and device selection and the EBU R128 group on Android; keep ReplayGain, fading, buffer and gapless options.

### 16. Moodbar and waveform cannot be generated

**Status:** Open

**Evidence:** Code.
Without fast spectrum, the moodbar pipeline uses GStreamer's `spectrum` element, and the waveform pipeline uses `appsink`; neither the `spectrum` nor the `app` plugin is bundled.
Both also depend on point 1 to decode the files.

**Approach:** Bundle `spectrum` and `app`, then verify both in the seek bar; otherwise hide their settings and track bar options on Android.

### 17. Verify automatic collection updates

**Status:** Open

**Evidence:** Code.
Collection monitoring uses `QFileSystemWatcher`, which may not report changes for shared storage on Android.

**Approach:** Add and remove a file in a collection folder on the emulator and check whether the collection updates without a manual rescan; if not, rely on rescans when the app starts or comes to the front.

### 18. Credentials are stored in plain text

**Status:** Open

**Evidence:** Code.
Without a keychain backend, passwords and tokens are stored in the app's private settings file.
On Android this file is only readable by the app, except on rooted devices or through backups (`allowBackup="true"`).

**Approach:** At least exclude the settings file from Android backups; storing secrets in the Android Keystore is optional.

### 19. Decode AAC and the remaining formats

**Status:** Done (AAC); WMA, APE and Musepack remain unsupported

**Evidence:** Code.
`isomp4` only demuxes M4A; there is no AAC decoder, and WMA, APE and Musepack have no decoder either.

**Options:**
`androidmedia` uses Android's system codecs and adds little size, but registers its decoders with primary rank, so it would also take over MP3, FLAC and Opus decoding unless the ranks are lowered at startup.
`libav` covers AAC, ALAC, WMA, APE and Musepack with well-tested software decoders, but links all of FFmpeg's codecs and makes the APK considerably larger.

**Resolved:** Chose `androidmedia`.
It is added to the Android plugin list, and `GstStartup::LowerAndroidMediaDecoderRanks()` in `gststartup_android.cpp` lowers its audio decoders from `PRIMARY + 1` to `SECONDARY` after GStreamer is initialized.
On the emulator, an AAC file in M4A plays, and Android's media metrics show that the app only ever created `c2.android.aac.decoder`: FLAC, MP3, Opus and Vorbis still use GStreamer's decoders.
The codec scan adds about 0.2 seconds to startup; it logs harmless "Unable to map AAC profile" and "Unsupported mimetype" messages for codec variants GStreamer does not map.
Android has no system decoders for WMA, APE or Musepack; supporting those would need `libav`.

### 20. Music files that Android does not classify as audio are invisible

**Status:** Open

**Evidence:** Confirmed.
A WavPack file (`.wv`) in a chosen folder is not added to the playlist: MediaStore classifies it as `application/octet-stream`, and with `READ_MEDIA_AUDIO` the app can only access audio files through file paths.
The same file renamed to `.wav` is classified as `audio/x-wav`, is added, and plays.
This likely also affects other extensions Android does not know, such as `.ape`, `.mpc`, `.dsf` and `.dff`.

**Approach:** Read such files through the content URIs of a picker-chosen folder, which grants access regardless of the MIME type, or document the limitation; decide together with point 9.

### 21. AAC radio streams do not play

**Status:** Open

**Evidence:** Confirmed, found while resolving point 3.
Internet radio usually sends AAC as ADTS frames, unlike the raw AAC in M4A files that point 19 verified.
- SomaFM's AAC streams use MPEG-2 ADTS (`mpegversion=2`), which `amcaudiodec` does not accept: it only registers `audio/mpeg, mpegversion=4` (raw or ADTS), so GStreamer reports a missing MPEG-2 AAC decoder.
- Radio Paradise's AAC streams use MPEG-4 ADTS; `amcaudiodec` accepts them and Android's `c2.android.aac.decoder` is created, but it produces no output, and playback stays at 0:00 until it is stopped.
HLS radio streams are almost always AAC, so they are affected as well.

**Approach:** Find out why the MPEG-4 ADTS input stalls (for example by letting `aacparse` convert ADTS to raw AAC with `codec_data`, which works for M4A), and accept MPEG-2 AAC by presenting it as MPEG-4, which is the same AAC bitstream.
If that is not feasible, reconsider `libav` (`avdec_aac` accepts both) from point 19.

### 22. Adapt the cover dialogs to Android

**Status:** Open

**Evidence:** Confirmed, found while resolving point 5.
- "Load cover from URL" (`CoverFromURLDialog`) appears as a desktop dialog across the full width, over the sidebar, with small OK and Cancel buttons.
- "Search for album covers" (`AlbumCoverSearcher`) extends below the bottom of the screen, so its OK and Cancel buttons are not visible; a result can only be chosen by double-tapping it, and Back cancels.
- The full-size cover (`AlbumCoverChoiceController::ShowCover()`) is a non-modal top-level window. Opened from the Context cover actions, Back closes it; opened by double-tapping the cover, it does not get focus, so Back closes the app, and it disappears behind the main window when that repaints, for example at the next track.

**Approach:** Show these dialogs full-page within the current tab, like `ActionDialog`, with touch-sized buttons, and make the full-size cover a modal, focused page that Back closes.
