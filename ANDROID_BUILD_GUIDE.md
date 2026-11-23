# Strawberry Android Build Guide

This guide provides step-by-step instructions for building Strawberry for Android from source. It documents the complete toolchain setup, dependency configuration, and build process.

## Prerequisites

### System Requirements
- Linux host system (tested on Ubuntu/Kubuntu)
- CMake 3.28+
- Ninja build system
- Git

### Required Software Installations

#### 1. Android NDK
Download and install Android NDK 27.2 (or compatible version):
```bash
# Download from https://developer.android.com/ndk/downloads
# Extract to: ~/Android/Sdk/ndk/27.2.12479018/
```

**Important:** The NDK contains the cross-compiler toolchain (`clang`/`clang++`) and CMake build tools for Android.

#### 2. Android SDK
Install Android SDK and build-tools:
```bash
# Install via Android Studio or command line
# Location: ~/Android/Sdk/
# Required: build-tools/33.0.0+ (or any recent version)
# Required: platforms/android-28+ (or target API level)
```

#### 3. Qt 6.7.3+ for Android
Download and install Qt for Android arm64-v8a:
```bash
# Download from https://www.qt.io/download
# Install location: ~/Qt/6.7.3/
# Required variant: android_arm64_v8a
# Also required: gcc_64 (host tools for cross-compilation)
```

### Important Qt Configuration
Qt requires both the Android cross-compile variant AND the host compiler tools:
- **Android target:** `~/Qt/6.7.3/android_arm64_v8a/` (for ARM64 Android builds)
- **Host tools:** `~/Qt/6.7.3/gcc_64/` (Linux host for generating code during cross-compile)

## Android text readability

On Android, Context lyrics use a minimum 20-point font for phone readability; larger configured fonts are retained.
Other Context text and desktop typography are unchanged.
Collection artist, album and song labels also use at least 20 points, with rows at least 48 device-independent pixels high.
Tap a container row to expand or collapse it; using the small branch arrow is not required.

On Android, tapping a letter divider hides or reveals all artists in that group without changing the expansion of their albums and songs.
An arrow beside the divider indicates whether the group is open or closed.
Settings > General > Collection > Display options > Allow collapsing letter groups controls this behavior and defaults to enabled.
It requires Show dividers; disabling it immediately reveals hidden groups.
As with all Android settings, the change is saved immediately.
Groups start collapsed after a collection reset or app restart, including letter groups added during scanning.
Opening a group keeps it open while browsing and reapplying settings; these temporary folds are not saved.
Desktop Collection behavior is unchanged.

## Experimental Qobuz browser authentication

Qobuz browser login is disabled by default pending roadmap item 24's provider verification.
To build a test APK using the existing Android build directory:

```bash
cmake -S . -B build -DENABLE_ANDROID_QOBUZ_BROWSER_AUTH=ON
```

Follow the normal APK packaging steps below, including `androiddeployqt`, so the new Java service and manifest are copied into the package.
Running Gradle alone against an older package directory does not update those sources.
This option does not enable Tidal, Last.fm, ListenBrainz, Genius or Spotify browser login.

The test flow uses a separate Android `shortService`, not the playback service, to keep a stopped player's local callback server running.
On Android, the Qobuz settings page saves changes automatically: choices are saved when changed, text fields when editing finishes, and fetched API credentials immediately.
Like every Android settings page, it has no Apply, OK or Cancel buttons; Back returns to the parent without discarding those changes.
Finish login within two minutes.
Return to the Qobuz settings page to cancel; success, failure and timeout also stop the service.
On Android 13 and newer, notification visibility depends on the system's notification settings, so use Recents to return if no notification is shown.
If Android kills the process, the old listener cannot be resumed: the next Qobuz editor explains the interruption and offers a fresh attempt.
Only a pending-status flag is stored for that recovery, not an authorization code or browser URL.

Verify actual login/token exchange, denial, cancellation/retry, delayed return with playback stopped, restart persistence and logout before enabling the option by default.
Enter credentials directly in the browser, never in chat or test logs.
Portable callback and session regressions run with:

```bash
cmake --build build/native --target qobuzservice_test oauthenticator_test localredirectserver_test --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build/native \
  -R '^(qobuzservice_test|oauthenticator_test|localredirectserver_test)$' --output-on-failure
```

## Experimental Last.fm browser authentication

Last.fm browser login is disabled by default while roadmap item 24 is being validated. Enable it for a test build with:

```bash
cmake -S . -B build -DENABLE_ANDROID_LASTFM_BROWSER_AUTH=ON
```

Repackage the APK using the normal Android build steps. In Settings > General > Scrobbler > Last.fm, start login and return within two minutes to cancel or retry.
The flow uses a separate foreground service and a fresh loopback callback path; it does not depend on playback.
If the build has no bundled Last.fm credentials, follow the link on the page to [register an app with Last.fm](https://www.last.fm/api/account/create).
Enter its API key as Client ID and its shared secret as Client secret. These identify the app, not the listener.
Settings are saved as they change, and Login saves credential edits that are still being typed, so no Apply step is needed.
An interrupted process cannot resume its old listener and must start a fresh login.
Provider login and session persistence were confirmed on the FP5. Denial, delayed return and process recreation during login remain unverified. Enter credentials only in the app or browser, not in chat or test logs.
Portable Last.fm callback and session regressions can be run with:

```bash
cmake --build build/native --target lastfmscrobbler_test localredirectserver_test --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build/native -R '^(lastfmscrobbler_test|localredirectserver_test)$' --output-on-failure
```

## Experimental Spotify plugin registration

The default-off registration probe accepts a separately cross-built `gst-plugin-spotify` static archive for the configured Android ABI:

```bash
cmake -S . -B build -DENABLE_ANDROID_SPOTIFY_PLUGIN_PROBE=ON \
  -DANDROID_SPOTIFY_PLUGIN_ARCHIVE=/absolute/path/to/libgstspotify.a
cmake --build build --target strawberry --parallel 2
```

The archive must target the same Android ABI and GStreamer SDK as the app.
Build it with [dist/scripts/build-android-gst-plugin-spotify.sh](dist/scripts/build-android-gst-plugin-spotify.sh), which pins the gst-plugins-rs 0.15.4+fixup source archive by SHA-256, requires Rust 1.92.0 and cargo-c 0.10.13, and builds with Cargo's `--locked` lockfile:

```bash
rustup toolchain install 1.92.0
rustup target add --toolchain 1.92.0 aarch64-linux-android
cargo +1.92.0 install --locked cargo-c --version 0.10.13
dist/scripts/build-android-gst-plugin-spotify.sh \
  --ndk ~/Android/Sdk/ndk/27.2.12479018 \
  --gstreamer /path/to/gstreamer-1.28.8/arm64 \
  --output /path/to/spotify-arm64
```

The script uses two parallel jobs by default (`--jobs N`), and the first build downloads the crates and takes about ten minutes on two cores.
Besides `libgstspotify.a`, the output directory contains `gst-plugin-spotify-build.txt`, recording the source, toolchain, ABI and archive checksum, and a `licenses/` directory.
That directory lists the crates linked into the archive with their declared licences in `gst-plugin-spotify-dependencies.txt`, and holds the licence and notice files of the plugin and every linked crate, preserving nested paths for bundled third-party notices.
Where a published crate omits its licence file (governor, librespot and Symphonia), the script downloads it from the upstream commit recorded in the crate, pinned by SHA-256; it fails if any linked crate is left without a licence text.
It also creates `gst-plugin-spotify-source.tar.gz`, containing the checksum-pinned upstream source archive, the complete source directories for every crate in the target-specific linked dependency tree, and the generated licence inventory and texts.
For a release, make this source bundle available to recipients alongside the APK or through a durable download URL, and retain the generated build manifest with it; the APK's licence texts alone are not a substitute.
With the probe enabled, CMake requires that directory next to the archive and copies it into the APK under `assets/licenses/gst-plugin-spotify/`.
The release build omits debug information and remaps local build paths.
Bit-for-bit identical archives across machines have not been verified.
A release arm64 archive built with this script registered `spotifyaudiosrc` in the app on the emulator; the probe APK was about 39 MB.
After Android GStreamer initialization, the probe checks the `CA_CERTIFICATES` bundle for readable PEM certificates and sets `SSL_CERT_FILE` to that path before registering `spotifyaudiosrc`.
If `SSL_CERT_FILE` or `SSL_CERT_DIR` is already set, the probe preserves the override rather than checking its contents; an empty override is reported as an error.
Missing or unreadable bundled certificates and setup failures are logged under `StrawberrySpotifyProbe`, and prevent probe registration.
The probe does not enable the Spotify service or login, and does not itself verify TLS or playback.
An isolated Android test of the pinned Rust TLS dependencies found no default roots on the emulator, but loaded 141 roots from a copy of the packaged GStreamer CA bundle when supplied as `SSL_CERT_FILE`.
The app's GLib `CA_CERTIFICATES` setting does not by itself configure Rust.
In the app process on the emulator, this setup let the plugin complete Spotify's HTTPS access-point lookup; Spotify then rejected a deliberately invalid token.
Without usable roots, librespot's HTTP client setup fails before connecting.
The experimental service below authenticated on the FP5, but playback with plugin 0.15.4 still failed with "track is not available".
Both plugin 0.14.4 and 0.15.4 use librespot 0.8.0, which already includes the extended-metadata endpoint change; upgrading the plugin alone did not resolve this failure.
The generic error does not establish whether the cause is an account-specific audio-key restriction or another track-loading failure.
Playback, seeking, track transitions, and background playback remain unverified; do not distribute probe builds as Spotify-enabled releases.
To restore the ordinary Android build, configure with `-DENABLE_ANDROID_SPOTIFY_PLUGIN_PROBE=OFF` and rebuild.

### Experimental Spotify login and playback on the FP5

For a local test build, enable `-DENABLE_ANDROID_SPOTIFY=ON` in addition to the plugin probe and its archive. Both options are off by default. To turn the probe off again, also set `-DENABLE_ANDROID_SPOTIFY=OFF`.
Register a Web API app in the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard), and add an accepted loopback Redirect URI, for example `http://127.0.0.1:8080/`.
Strawberry binds an available loopback port and includes it in the authorization request; Spotify permits dynamic ports for loopback redirects.
Use a Premium account authorized for the app in Development Mode. On the phone, enter the app's Client ID in Settings > Streaming > Spotify, then tap Login. Do not enter a client secret; Android uses PKCE.
Complete authorization within two minutes, then switch back to Strawberry. This build is for verifying the callback and playback, not for public distribution.

### Spotify plugin distribution licensing

The pinned `gst-plugin-spotify` 0.15.4 manifest declares MPL-2.0. The linked Symphonia 0.5.5 crates also declare MPL-2.0, while librespot 0.8.0 declares MIT; do not treat librespot as MPL-covered.
Under [MPL-2.0 sections 3.1–3.4](https://www.mozilla.org/en-US/MPL/2.0/), distributing the plugin in executable form requires making its corresponding source form available and telling recipients how to obtain it by reasonable means in a timely manner. Source-form distributions must be under MPL-2.0, and the covered software's license notices must not be removed or altered.
Including licence texts in the APK does not, by itself, provide the corresponding source or explain how to obtain it. Before any Spotify-enabled release:

- Publish or otherwise provide the generated source bundle corresponding to the shipped archive, including any modifications, under the applicable terms; identify that source and provide recipients a practical way to obtain it.
- Preserve the applicable MPL notices and include the MIT notice for librespot. Keep the generated licence bundle with the release.
- Review every entry in `gst-plugin-spotify-dependencies.txt` and its declared licence, as well as the actual files and notices shipped by the build. The generated inventory and licence texts are evidence for review, not an automated compatibility determination.
- Record the source revision, archive and source-bundle checksums, licence inventory and source-delivery location for each released ABI.

The pinned arm64 release build was technically reviewed: the inventory has 231 entries and 25 distinct declared expressions. It includes the plugin and eight Symphonia crates under MPL-2.0; `priority-queue` is offered under LGPL-3.0-or-later or MPL-2.0, so the MPL option is available. The ICU data crates use Unicode-3.0, `webpki-roots` uses CDLA-Permissive-2.0, and `webpki` has no Cargo license field but its source contains an ISC-style license plus nested Chromium and BetterTLS notices. Those notices are now included at their original relative paths. The remaining expressions are permissive MIT, Apache, BSD, ISC, BSL, Zlib, 0BSD or Unlicense terms and their combinations; the inventory has no GPL-only expression.

The inventory review is a technical check, not legal advice or distribution approval. The source bundle can now be generated, but no release source-delivery location or distribution has been set up; Spotify-enabled distribution remains blocked until recipients can obtain the exact source and the release process preserves the required notices.

## NAS client dependency

Android builds enable `ENABLE_NAS` by default.
CMake fetches the SHA-256-pinned libsmb2 6.3 source archive and builds a position-independent static SMB2/SMB3 client with NTLM authentication, without Kerberos or example programs.
The first configuration needs access to GitHub's source archive service.
For an offline build, provide the extracted source directory through `-DFETCHCONTENT_SOURCE_DIR_STRAWBERRY_SMB2=/path/to/libsmb2-source`.
Use `-DENABLE_NAS=OFF` to omit the dependency and Android NAS configuration UI.
Desktop builds leave this option off by default.

The dependency's copyright notice and LGPL 2.1 text are packaged under the APK's `assets/licenses/` directory.
The pinned source URL and checksum are in [cmake/Nas.cmake](cmake/Nas.cmake); the source and build scripts must remain available when distributing a statically linked APK.

NAS locations use read-only virtual paths for the existing collection scanner, Qt cover reads, TagLib metadata reads and GStreamer source.
On Android, credentials and location details are encrypted with an Android Keystore AES-GCM key and saved atomically in the app's no-backup directory, not the regular settings file.
Removing app data also removes those locations and their key.
NAS folders are rescanned through collection startup/manual scans and, while collection monitoring is enabled, through SMB change notifications; native filesystem change notifications are not available for SMB.
A notification rescans only the folders containing the changed paths; an overflow, a reconnect or a notification without usable paths reconciles the whole NAS folder.
Nothing is monitored while the app is not in the foreground: Android stops the NAS monitors when the app is hidden or suspended and starts them again when it is active, then they scan the folders whose modification time changed meanwhile, like the update on startup.
The monitors and a running NAS scan are stopped at once, scans already requested are skipped, and the monitors start again within seconds.
An in-place tag edit made while the app is hidden, which doesn't change its folder's modification time, is found by the next full reconcile, after an overflow, a changed location, a reconnect or a manual full scan.
Monitor stops and starts are logged with the `StrawberryNas` logcat tag.
The scanner first counts the files, then publishes newly discovered tracks after each completed folder so the collection grows during a large scan.
Changes to existing tracks, removals/unavailability, folder timestamps, and expiry are deferred until the entire scan succeeds.
Cancelling a scan or losing the connection keeps new tracks from completed folders; pending changes to existing collection entries are discarded.
Android uses a single app-owned top title bar instead of the native Android action bar.
It shows the app name on the left, centered scan status and a thin progress bar, without a Stop button that could be tapped accidentally.
Use **Tools > Collection > Stop scan**, alongside **Update collection** and **Full collection scan**, to stop during counting or reading metadata.
The idle title bar shows only the app name.
Scan messages no longer appear in the footer, which continues to report other background tasks.
Counting reports the number of entries found, not a percentage or remaining time: the total is not known yet.
On a NAS, counting still traverses the folder tree and performs network metadata operations, so it is not necessarily fast for a large collection.
The SMB client now reuses the type, size and timestamps returned in each directory listing instead of opening a new authenticated connection to inspect every listed entry.
This metadata is scoped to the active directory iterator, or the scan of that folder, and its thread, and is ignored after NAS location settings change.
It is not a persistent or time-based cache: a later listing/scan obtains fresh information, and failed connections still block destructive scan changes.
The counting pass also reuses Qt's iterator-provided file information.
In the loopback regression, inspecting 128 files takes two connection negotiations and two QUERY_INFO requests instead of 128 of each.
That measures metadata-request reduction, not a promise of equivalent total scan speedup: directory traversal, file opens and tag reads still require network traffic.
After counting, the display reports processed/total entries and estimates remaining scan time only after at least five seconds and three processed entries.
The estimate excludes the counting phase and is approximate: file formats, network delays, folder sizes and final database/UI work can change completion time.
Stopping requests cooperative cancellation; an in-flight SMB operation must return or time out before the scan finishes stopping.
The status shows a stopping message rather than claiming the operation has already stopped; repeated stop requests do not start a new cancellation.
Another manual scan can be started afterward.
Configure a location in Settings > General > Collection > Music folders > NAS locations.
Enter the server's local name/IP address, share, optional relative folder, and NAS account, then use **Check and use folder**.
A successful check saves the location and adds it to the collection immediately.
Remove a location using the normal **Remove folder** control; it asks for confirmation first, and the removal is saved immediately once confirmed.
After editing an existing location's server/share/folder, run **Full collection scan** to refresh its contents; its virtual identifier and existing playlist URLs remain unchanged.
Use a read-only NAS account as an additional server-side safeguard; SMB signing is required, but a locally signed SMB connection is not a promise of transport encryption.
The `(i)` information page explains separately configured VPN access; the app does not set up remote access.

For optional host SMB2 protocol tests, configure the separate native build with `-DENABLE_NAS=ON`, build `nasfileengine_test`, and run it with `STRAWBERRY_NAS_TEST_PYTHON` pointing to an interpreter containing `impacket`.
The fixture binds only to loopback, uses generated files and dummy credentials, and is stopped by the test.
It normalizes Impacket's command-specific error bodies to the SMB2 ERROR format required by MS-SMB2 2.2.2, preserving the original error status and normal packet signing.
Without that interpreter setting, the protocol cases explicitly skip; validation-only cases still run.
This fixture does not replace the real-NAS/Synology test that remains open under roadmap item 25.

## Android alarm clock

Tools > Alarm clock and Tools > Clock are Android only (roadmap item 28).
The Qt pages are in `src/alarm/*_android.cpp`; the Java side is in `android/java/nl/dewitweb/strawberryflavor/Alarm*.java`, `ClockActivity.java`, `ClockView.java`, `AnalogClockView.java` and `ClockFace.java`.

**Manifest requirements:**
- Permissions: `POST_NOTIFICATIONS`, `USE_EXACT_ALARM`, `SCHEDULE_EXACT_ALARM` (Android 12 only, `maxSdkVersion` 32), `USE_FULL_SCREEN_INTENT`, `SYSTEM_ALERT_WINDOW` (only requested as a fallback while notifications are off), `RECEIVE_BOOT_COMPLETED`, `WAKE_LOCK` and the media-playback foreground service permissions.
- `AlarmReceiver` receives the `AlarmManager` alarms and starts the `mediaPlayback` foreground service `AlarmService`.
- `AlarmBootReceiver` reschedules on `BOOT_COMPLETED`, `MY_PACKAGE_REPLACED`, `TIME_SET`, `TIMEZONE_CHANGED` and exact alarm permission changes.
- `AlarmActivity` is the alarm screen, shown over the lock screen with `showWhenLocked` and `turnScreenOn`.
- `android:networkSecurityConfig="@xml/network_security_config"` permits plain http for the Java `MediaPlayer` that plays alarm radio streams; Qt and GStreamer networking do not use this policy.

**Persistence:** alarms are JSON in the SharedPreferences file `alarm_clock`, readable by the receivers while the app process is dead.
`AlarmSchedule.java` calculates when alarms go off; `ctest -R alarmschedule_java_test` tests it on a desktop JVM if CMake finds a JDK with `javac` (set `JAVA_HOME` when configuring).
Local copies of the alarm songs are in `files/alarms/<id>/` in the app data directory.
The copy settings are Qt only, in the `AlarmClock` group of `strawberry.conf`: `copy_tracks` (default true), `tracks_per_alarm` (1, 3, 5 or 10; default 5) and `maximum_total_mb` (50, 100, 200, 500 or 1024; default 200).
An alarm's `tracks` list holds either its copies or, with copying off, the original local paths and http(s) URLs, so the Java side does not read these settings.
`ctest -R alarmtracks_test` tests the limits and source rules in `src/alarm/alarmtracks_android.cpp`.
Until the copy job finishes, it stores playable local originals, or the copies made so far, as interim tracks, so a new alarm can play them right away.
`AlarmFallback.java` chooses the default sound reason shown on the alarm screen and in the notification, and lets a random alarm without songs play other alarms' copies; `ctest -R alarmfallback_java_test` tests it on a desktop JVM.
The Tools > Clock settings are in the same SharedPreferences file: `clock_type` (0 digital, 1 analog; default digital), used by `ClockView` for Show clock and the alarm screen, and `clock_brightness` (percent, 0 for the system brightness), applied as window brightness only while `ClockActivity` is resumed.
`ctest -R clockface_java_test` tests the hand angles and the brightness mapping in `ClockFace.java`.
`AlarmPermissions.java` decides which permission warnings the overview shows and when the notification permission dialog may appear; `ctest -R alarmpermissions_java_test` tests it.

**Playback chain:** `AlarmReceiver` and `AlarmService` stay the entry point when an alarm goes off and own the alarm screen, snooze, stop and the automatic stop.
The music, however, preferably plays through the Qt app's player, so NAS songs, streams and live playlist, last played and random sources work without copies:
1. If the Qt app's alarm player is ready (the app is running), `AlarmService` asks it to play the alarm and keeps silent for up to `AlarmHandover.GRACE_MSEC` (3 seconds); if the app has not answered by then, the Java sound starts and keeps sounding until the app answers.
2. If the app is not running, the alarm screen starts it (`AlarmActivity.onResume()`, once per alarm, not while `KeyguardManager.isDeviceLocked()`) and Java plays the local copy or the default sound meanwhile.
   The app stays in front until its player is ready; `AlarmService` then shows the alarm screen again and asks the app to play.
3. If the app does not play the alarm within `AlarmHandover.TIMEOUT_MSEC` (20 seconds, restarted once the player is ready), or reports that it cannot, Java keeps playing the local copy or the default sound.

Default sound alarms never go to the app.
`AlarmHandover.java` is the state machine (IDLE, WAITING, QT, JAVA) that returns which actions to take, so there is one owner of the sound at a time; `ctest -R alarmhandover_java_test` tests it on a desktop JVM.

**Protocol** (`AlarmBridge.java` and `src/alarm/alarmplayer_android.cpp`, JNI in one process):
- The Qt `AlarmPlayer` registers `nativeStartAlarm` and `nativeStopAlarm` and then calls `AlarmBridge.setPlayerReady(true)` once `MainWindow` is set up; the flag lives only in the process, so it is false after the process ends.
- `AlarmService` calls `startAlarm(token, alarm JSON, fade start, fade duration)` with a new token per attempt.
- The app checks the source first: it collects up to 50 songs and checks that the first 3 can be reached (`AlarmPlayback::kMaximumSongs` and `kCheckedSongs`; once a NAS file is missing, the remaining NAS files are dropped), or the stream URL.
  It then replaces the Now playing playlist, saves volume and mute, plays on the alarm stream (`GstEnginePipeline::SetAndroidStreamType("alarm")`, `USAGE_ALARM`) and answers `alarmClaimed(token)` when the engine plays, or `alarmFailed(token, reason)`.
- Java stops its own sound only after `alarmClaimed`; a later `alarmFailed` (for example the user paused the player) starts the Java sound again.
- Snooze, Stop and the automatic stop call `stopAlarm(token)`; the app stops the player and restores volume and mute.
  Answers with an old token are ignored.
- `AlarmPlaybackSession` (`src/alarm/alarmplayback_android.cpp`) holds the Qt side's states; `ctest -R alarmplayback_test` tests it and the reachability checks.

`QtActivity` has the meta-data `android.app.background_running=true`: without it, Qt stops its event loop while the alarm screen covers the app or the screen is off, so the app could not answer.
Qt startup itself still needs the app in front (Qt queues the window creation while the app is suspended or hidden), which is why the app stays in front until its player is ready.
`AlarmService` cannot start the app by itself: Android blocks activity starts from the background (`BAL_BLOCK`), but allows them from the alarm screen while it is visible.

**Limitations:**
- `setAlarmClock()` alarms go off in Doze and battery saver, but force stopping the app cancels them until it is opened again.
- Without the exact alarm permission (Android 12 and 12L), alarms are inexact and sound through a full-screen notification with the default alarm sound; the music starts when the alarm is opened.
- After a reboot, alarms are rescheduled after the first unlock.
- Without notifications for the app or its Alarm clock category (Android 13+ permission or the user's switches) or the full-screen-intent permission (Android 14+), the overview shows warnings with buttons to the matching dialog or settings page.
- Without notifications, the alarm screen opens only while Strawberry is visible, when display over other apps is allowed, or when the user opens Strawberry; otherwise only the automatic stop, if not switched off, ends the alarm, and the volume and power keys do not stop it.
- The volume keys change the alarm volume (`STREAM_ALARM`) while the alarm screen is shown, for the Java sound and the app's player alike, and never snooze or stop the alarm; without the alarm screen in front, for example with a heads-up notification, they change the media volume.
- While the phone is in use, Android shows a heads-up notification instead of the alarm screen; the app is then started only if the user opens the alarm screen, otherwise Java plays.
- On a phone locked with a PIN, pattern or password, the app cannot show over the lock screen: it would cover the alarm screen without finishing its startup, and the service cannot bring the alarm screen back from the background. The alarm screen therefore does not start the app then; it stays visible and Java plays the local copy or the default sound, so NAS songs without copies are not played.
- When the app plays an alarm, it replaces the Now playing playlist.
- Radio sources need network access; otherwise the default alarm sound plays.
- With copying off, NAS songs play only through the app; if it does not take over, the Java side, which has no SMB client, plays the default alarm sound instead.

**Testing on the emulator** (do not clear app data; it holds NAS credentials):

```bash
export ANDROID_SERIAL=emulator-5554
# Scheduled alarms, shown as "Next alarm clock information"
adb shell dumpsys alarm | grep -A1 "Next alarm clock"
# Scheduling, song count changes, alarms going off, default sound reasons, snooze and stop messages
adb logcat -s StrawberryAlarm
# Stored alarms (debug builds)
adb shell run-as nl.dewitweb.strawberryflavor cat shared_prefs/alarm_clock.xml
# Turn the screen off before an alarm goes off
adb shell input keyevent 26
# Test the permission warnings; restore afterwards with pm grant and appops set ... default
adb shell pm revoke nl.dewitweb.strawberryflavor android.permission.POST_NOTIFICATIONS
adb shell appops set nl.dewitweb.strawberryflavor SYSTEM_ALERT_WINDOW allow
# Whether Android let the alarm screen open from the background
adb logcat -d | grep "START.*AlarmActivity"
# Playback chain: who plays the alarm ("Asking the running app to play the alarm",
# "Starting the app to play the alarm", "The app plays the alarm",
# "The app does not play the alarm: <reason>", "The app did not play the alarm in time")
adb logcat -s StrawberryAlarm
# Which player sounds: the app's player shows as "OpenSL ES" with USAGE_ALARM
adb shell dumpsys audio | grep "AudioPlaybackConfiguration piid" | grep -v SoundPool
# While the app plays, pausing it makes Java take over
adb shell input keyevent KEYCODE_MEDIA_PAUSE
```

To test the chain, (a) keep the app running, turn the screen off and let an alarm go off: the app should play it within a few seconds.
(b) End the process with `run-as ... kill -9`, turn the screen off and let an alarm go off: the alarm screen starts the app, which plays the alarm once ready, or Java keeps playing after the time-out.
In both cases Snooze and Stop on the alarm screen or in the notification must silence every player.

`adb shell am force-stop` cancels the alarms; to test with the app closed, end the process with `adb shell run-as nl.dewitweb.strawberryflavor kill -9 <pid>` instead.
Seed `shared_prefs/alarm_clock.xml` only while the app process is not running; a running process keeps its loaded preferences and overwrites the file.
The Qt app does not finish starting while it is not in front (see the playback chain above), so wake the screen before launching it to reschedule.
On the emulator the arm64 app runs under translation and takes about 20 seconds to start.

## GStreamer Android SDK Setup

### 1. Download GStreamer Android SDK 1.28.8

The official GStreamer Android SDK includes all audio/video libraries needed:

```bash
# Create storage directory
mkdir -p ~/Android/Downloads

# Download (969 MB)
cd ~/Android/Downloads
wget "https://gstreamer.freedesktop.org/data/pkg/android/1.28.8/gstreamer-1.0-android-universal-1.28.8.tar.xz"

# Verify file (should be ~969 MB)
ls -lh gstreamer-1.0-android-universal-1.28.8.tar.xz
```

**Note:** The full GStreamer SDK is 969 MB. If you see a file of only 9.6 MB, the download is incomplete and must be retried.

### 2. Extract GStreamer SDK

```bash
# Extract to standard location (recommended)
cd ~/Android/
tar -xf ~/Android/Downloads/gstreamer-1.0-android-universal-1.28.8.tar.xz

# This creates the directory structure:
# ~/Android/gstreamer-1.28.8/
#   ├── arm64/     (ARM64 v8a architecture - used for most devices)
#   ├── armv7/     (ARMv7 architecture)
#   ├── x86/       (Intel x86)
#   └── x86_64/    (Intel x86_64)

ls -la ~/Android/gstreamer-1.28.8/
```

The GStreamer SDK contains:
- **Static libraries** (.a files): Object code for GStreamer components, glib, openssl, etc.
- **Header files** (.h): C/C++ interfaces for all libraries
- **Plugins** (.so): Runtime loadable codec/format plugins
- **.pc files** (pkg-config): Library metadata for CMake discovery

### 3. Prebuilt libgstreamer_android.so (Optional)

If you have an existing working APK, you can extract its prebuilt shared library to speed up builds:

```bash
# Pull APK from emulator
adb shell pm path nl.dewitweb.strawberryflavor
# Note the path returned: package:/data/app/...

adb pull "/data/app/~~.../nl.dewitweb.strawberryflavor-.../base.apk" strawberry-working.apk

# Extract libgstreamer_android.so
unzip strawberry-working.apk "lib/arm64-v8a/libgstreamer_android.so"

# Copy to GStreamer SDK
cp lib/arm64-v8a/libgstreamer_android.so ~/Android/gstreamer-1.28.8/arm64/lib/

# Verify
file ~/Android/gstreamer-1.28.8/arm64/lib/libgstreamer_android.so
# Should show: "ELF 64-bit LSB shared object, ARM aarch64"
```

### 4. OpenSSL for Qt

Qt's TLS backend loads `libssl_3.so` and `libcrypto_3.so` at run time; without them every HTTPS request fails (radio lists, covers, lyrics, scrobbling, streaming services).
The GStreamer SDK only contains static OpenSSL libraries, so use Qt's recommended prebuilt libraries from KDAB.
Only the `ssl_3/arm64-v8a` directory is needed:

```bash
cd ~/Android/
git clone --depth 1 --filter=blob:none --sparse https://github.com/KDAB/android_openssl.git
cd android_openssl
git sparse-checkout set --no-cone '/*.cmake' '/*.md' '/LICENSE*' '/ssl_3/arm64-v8a/' '/ssl_3/include/'
```

Pass the checkout as `ANDROID_OPENSSL_ROOT` when configuring; CMake warns if the libraries are not found.

## Building Strawberry for Android

### 1. Clone Strawberry Repository

```bash
cd ~/source/repos/
git clone https://github.com/strawberrymusicplayer/strawberry.git
cd strawberry
```

### 2. Configure CMake Build

The CMake configuration requires multiple environment variables and cache variables for proper Android cross-compilation:

```bash
# Set up environment
export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64
export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Clean previous builds
rm -rf build

# Configure CMake
/home/wouter/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_STL=c++_shared \
  -DQT_CHAINLOAD_TOOLCHAIN_FILE=~/Android/Sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_ABI=arm64-v8a \
  -DQT_HOST_PATH=~/Qt/6.7.3/gcc_64 \
  -DANDROID_SDK_ROOT=~/Android/Sdk \
  -DOPENSSL_ROOT_DIR="${GSTREAMER_ROOT_ANDROID}" \
  -DOPENSSL_INCLUDE_DIR="${GSTREAMER_ROOT_ANDROID}/include" \
  -DOPENSSL_CRYPTO_LIBRARY="${GSTREAMER_ROOT_ANDROID}/lib/libcrypto.a" \
  -DANDROID_OPENSSL_ROOT=~/Android/android_openssl \
  -S . -B build
```

**Key Configuration Parameters Explained:**

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `CMAKE_BUILD_TYPE` | `Release` | Optimized build (use `Debug` for debugging) |
| `ANDROID_STL` | `c++_shared` | Use C++ standard library from NDK |
| `QT_CHAINLOAD_TOOLCHAIN_FILE` | NDK cmake/android.toolchain.cmake | Cross-compiler configuration |
| `ANDROID_PLATFORM` | `android-28` | Minimum Android API level (adjust as needed) |
| `ANDROID_ABI` | `arm64-v8a` | Target architecture (ARM 64-bit) |
| `QT_HOST_PATH` | Qt gcc_64 | Host tools for code generation during cross-compile |
| `ANDROID_SDK_ROOT` | Android Sdk path | SDK location (override Qt's hardcoded path) |
| `GSTREAMER_ROOT_ANDROID` | GStreamer arm64 path | GStreamer libraries and headers |
| `ANDROID_OPENSSL_ROOT` | KDAB `android_openssl` checkout | Shared OpenSSL libraries bundled for Qt's HTTPS support (also read from the environment) |

### 3. Expected CMake Output

Successful configuration produces:

```
-- Checking for module 'gstreamer-1.0'
--   Found gstreamer-1.0, version 1.28.8
-- Found GStreamerMobile: ... found components: api_base api_audio api_app ...
-- Configuring 'strawberry' for the following Android ABIs: arm64-v8a (default)
-- Found android platform plugin at: ...
-- Configuring done (8.7s)
-- Generating done (0.3s)
-- Build files have been written to: .../build
```

### 4. Build the Android APK

The Android GStreamer bundle includes `spectrum` for moodbar generation and `app` for waveform decoding through `appsink`.
The app and pbutils API archives must remain linked into `GStreamerMobile`, not separately into Strawberry: duplicate static app API copies produce incompatible `GstAppSink` types at runtime.
The `api_app` component alone does not register the `app` plugin.

AAC radio uses the Android-only `strawberryadtsdecoder` wrapper: `aacparse` converts MPEG-2/MPEG-4 ADTS to raw AAC with `codec_data`, a caps adapter presents MPEG-2 AAC as MPEG-4, and the system AAC decoder produces PCM.
Keep `audioparsers` and `androidmedia` bundled for this path.
Raw AAC from M4A bypasses the wrapper, and the APK does not need libav.
AAC HLS additionally needs `adaptivedemux2` (streams-aware HLS for `playbin3`) and `mpegtsdemux` (MPEG-TS segments); retain `hls` for legacy discovery.
Bundling just `hls` is insufficient for the app's playback path.

The package source directory also supplies Android backup XML rules.
Keep the manifest references and both rule files together: Android 11 and earlier use `fullBackupContent`, while Android 12 and later use `dataExtractionRules` for cloud backup and device transfer.
All exclude `files/strawberry/strawberry.conf` (the `file` domain's `strawberry/strawberry.conf` path), which contains credentials as well as ordinary preferences.
Other app data remains eligible for backup.
Update both rule files if the settings location changes.

Android document access also requires matching Qt Core private headers (`Qt6::CorePrivate`).
The read-only path bridge preserves picker-granted access for formats that Android does not classify as audio.
See [Android storage access](ANDROID_STORAGE.md) for implementation details, permission lifecycle and regression checks.

```bash
cd build

# Build using all available CPU cores
cmake --build . --target strawberry_make_apk --parallel $(nproc)
```

**Build Output Locations:**
- APK file: `build/android-build/build/outputs/apk/release/android-build-release-unsigned.apk` (unsigned)
- Deployment settings: `build/android-strawberry-deployment-settings.json`
- Native library: `build/libstrawberry_arm64-v8a.so`
- Deployment bundle: `build/android-build/` (Gradle project with the APK and its dependencies)

**Typical Build Time:** 10-30 minutes depending on CPU cores and disk speed

**Android package sources:** `android/` is the Qt package source directory (`QT_ANDROID_PACKAGE_SOURCE_DIR`) and is copied into `build/android-build/`.
Put the app's Java sources in `android/java/`, never in `android/src/`: when the package source directory has a `src/` directory, `androiddeployqt` replaces `build/android-build/src/`, which removes the GStreamer Java classes copied there and makes the app crash at startup ("Could not retrieve class org.freedesktop.gstreamer.GStreamer").

**Android SDK platform:** `androiddeployqt` compiles against the highest platform installed in the SDK.
Android Gradle plugin 7.4.1, which Qt 6.7 uses, cannot resolve a platform installed as `android-37.0` (Gradle reports that it cannot find `platforms;android-37`).
Either remove that platform, or build the `strawberry` target and run the `androiddeployqt` command from `build/CMakeFiles/strawberry_make_apk.dir/build.make` yourself, adding `--android-platform android-34`.
Run the `cmake -E copy` command just above it first, or copy `build/libstrawberry_arm64-v8a.so` to `build/android-build/libs/arm64-v8a/` yourself; otherwise the APK silently keeps the previous library.
Gradle uses one worker per CPU core; set `GRADLE_OPTS="-Dorg.gradle.workers.max=2"` to limit it.

### 5. Expected Build Output

The build produces an Android APK file ready for deployment:

```
[100%] Built target strawberry_make_apk
...
Android package built successfully ...
  -- File: .../build/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

## Deploying to Emulator or Device

### 1. Start Android Emulator

```bash
# List available emulators
emulator -list-avds

# Start emulator
emulator -avd <emulator_name> &
```

### 2. Wait for Emulator to Be Ready

```bash
# Check device connection
adb devices

# Wait for "device" status (not "offline" or "connecting")
# Take 10-30 seconds after starting emulator
```

### 3. Install APK

```bash
# The build produces an unsigned release APK; sign it first (zipalign + apksigner, see ANDROID_EMULATOR_TESTING.md step 2)
# Install the signed APK (overwrites previous version)
adb install -r /tmp/strawberry-signed.apk

# Launch app (the activity is org.qtproject.qt.android.bindings.QtActivity, not .MainActivity)
adb shell monkey -p nl.dewitweb.strawberryflavor 1
```

### 4. View Application Logs

```bash
# Real-time log output
adb logcat | grep -E "strawberry|GStreamer|AndroidRuntime"
```

## Troubleshooting

### CMake Configuration Errors

#### Error: "Could NOT find Qt6"
**Cause:** Qt6 CMake config not found
**Solution:** Ensure `-DQT_HOST_PATH` and Qt installation path are correct

#### Error: "Could not locate Android SDK build tools"
**Cause:** Qt toolchain looks for Android SDK at hardcoded path `/opt/android/sdk/`
**Solution:** Override with `-DANDROID_SDK_ROOT=~/Android/Sdk`

#### Error: "Could NOT find GStreamer"
**Cause:** `GSTREAMER_ROOT_ANDROID` environment variable not set
**Solution:** Export before cmake: `export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64`

#### Error: "Package 'glib-2.0' not found"
**Cause:** pkg-config path doesn't include GStreamer libraries
**Solution:** Set PKG_CONFIG_PATH before cmake: `export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"`

### Build Errors

#### Error: "undefined reference to `getgrgid_r`"
**Cause:** Static libraries built with glibc instead of Android bionic
**Solution:** Ensure you downloaded the correct GStreamer Android SDK (not the host version)

#### Error: "linker command failed with exit code 1"
**Cause:** Usually indicates missing or incompatible GStreamer libraries
**Solution:** 
1. Verify GStreamer SDK is extracted correctly: `ls ~/Android/gstreamer-1.28.8/arm64/lib/libgst*.a | wc -l` (should be 100+)
2. Check NDK version compatibility: `cat ~/Android/Sdk/ndk/27.2.12479018/source.properties`

### Runtime Errors on Emulator

#### App Crashes with "dlopen failed"
**Cause:** Missing or incompatible libgstreamer_android.so
**Solution:** Extract prebuilt .so from working APK and verify with `adb shell file /data/app/.../lib/arm64-v8a/libgstreamer_android.so`

#### Audio Not Working
**Cause:** GStreamer plugins not loaded or missing audio backend
**Solution:** Check logcat for GStreamer plugin warnings: `adb logcat | grep -i gstreamer`

## Build Optimization Tips

### Faster Rebuilds After Code Changes

If you only modified Strawberry C++ code (not CMakeLists.txt or GStreamer config):

```bash
cd build
cmake --build . --target strawberry_make_apk
```

This reuses CMake configuration and only recompiles changed files.

### Parallel Build Control

Adjust parallel build jobs if system becomes unresponsive:

```bash
# Use only 4 cores
cmake --build . --target strawberry_make_apk -j 4

# Use only 2 cores (good for 8GB RAM systems)
cmake --build . --target strawberry_make_apk -j 2
```

### Incremental Builds

After first successful build, subsequent builds are much faster:
- First build: 20-30 minutes
- Incremental builds: 2-5 minutes (for code-only changes)

## Advanced: Build Variables Reference

### Qt-specific Variables
- `QT_HOST_PATH`: Path to host Qt installation with gcc_64 (required for cross-compile)
- `QT_CHAINLOAD_TOOLCHAIN_FILE`: Android NDK CMake toolchain file

### Android-specific Variables
- `ANDROID_PLATFORM`: Target Android API level (android-28, android-29, etc.)
- `ANDROID_ABI`: Target architecture (arm64-v8a, armeabi-v7a, x86, x86_64)
- `ANDROID_STL`: C++ standard library (c++_shared or c++_static)
- `ANDROID_SDK_ROOT`: Path to Android SDK root directory

### GStreamer-specific Variables
- `GSTREAMER_ROOT_ANDROID`: Path to extracted GStreamer Android SDK arm64 directory
- `OPENSSL_ROOT_DIR`: Path to OpenSSL (usually same as GSTREAMER_ROOT_ANDROID)
- `OPENSSL_CRYPTO_LIBRARY`: Path to libcrypto.a (GSTREAMER_ROOT_ANDROID/lib/libcrypto.a)

## Complete Build Script

For convenience, here's a complete build script that can be saved as `build-android.sh`:

```bash
#!/bin/bash
set -e

# Configuration
REPO_DIR="$HOME/source/repos/strawberry"
QT_PATH="$HOME/Qt/6.7.3"
ANDROID_SDK="$HOME/Android/Sdk"
NDK_PATH="${ANDROID_SDK}/ndk/27.2.12479018"
GSTREAMER_PATH="$HOME/Android/gstreamer-1.28.8"

# Set up environment
export GSTREAMER_ROOT_ANDROID="${GSTREAMER_PATH}/arm64"
export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Navigate to repository
cd "$REPO_DIR"

# Clean previous build
rm -rf build

# Configure CMake
echo "Configuring CMake..."
"${QT_PATH}/android_arm64_v8a/bin/qt-cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_STL=c++_shared \
  -DQT_CHAINLOAD_TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake" \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_ABI=arm64-v8a \
  -DQT_HOST_PATH="${QT_PATH}/gcc_64" \
  -DANDROID_SDK_ROOT="${ANDROID_SDK}" \
  -DOPENSSL_ROOT_DIR="${GSTREAMER_ROOT_ANDROID}" \
  -DOPENSSL_INCLUDE_DIR="${GSTREAMER_ROOT_ANDROID}/include" \
  -DOPENSSL_CRYPTO_LIBRARY="${GSTREAMER_ROOT_ANDROID}/lib/libcrypto.a" \
  -S . -B build

# Build APK
echo "Building APK..."
cd build
cmake --build . --target strawberry_make_apk --parallel $(nproc)

# Output result
echo ""
echo "Build complete!"
find . -name "*.apk" -type f 2>/dev/null | while read apk; do
  echo "APK: $apk ($(du -h "$apk" | cut -f1))"
done
```

Usage:
```bash
chmod +x build-android.sh
./build-android.sh
```

## Additional Resources

- **Strawberry GitHub:** https://github.com/strawberrymusicplayer/strawberry
- **Qt Android Documentation:** https://doc.qt.io/qt-6/android-platform-notes.html
- **Android NDK Documentation:** https://developer.android.com/ndk
- **GStreamer Documentation:** https://gstreamer.freedesktop.org/documentation/

## Contributing

If you encounter issues or have improvements to this guide, please:
1. Document the problem and solution
2. Test with a clean build from scratch
3. Submit a pull request or issue to the Strawberry repository

---

**Last Updated:** October 2026
**Tested Configuration:** Ubuntu/Kubuntu with Qt 6.7.3, NDK 27.2, GStreamer 1.28.8
