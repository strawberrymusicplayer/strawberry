# Android music access and retained folder permissions

## Why audio permission is not enough

Android's `READ_MEDIA_AUDIO` permission is based on MediaStore's classification, not on whether Strawberry can decode a file.
A valid WavPack file with a `.wv` extension can be classified as `application/octet-stream`.
Opening its normal filesystem path is then denied, although Strawberry's WavPack decoder supports its contents.
Other uncommon extensions can encounter the same restriction.
Renaming files is not a reliable solution and is not required by this implementation.

Android's Storage Access Framework (SAF) provides separate access to documents selected through the system picker.
A selected folder grants access to its descendants, regardless of their MIME types.
This permission belongs to a `content://` document URI: converting that URI to `/storage/...` does not transfer the permission to the filesystem path.

## What users need to do

Select the music folder through the Android picker when adding a Collection directory, using Add folder, or opening Files > Choose folder.
Individual local external-storage and Downloads documents selected through Add file or Files > Choose files also retain their own read grant.
Previously persisted document and folder grants are loaded automatically when Strawberry starts; existing Collection entries and playlist URLs do not need migration.
On Android, the Files tab offers system-picker buttons rather than the desktop filesystem browser.
Cancelling the picker does not grant access or change the playlist.
If uncommon files remain unavailable in an existing Collection, select that folder again through the picker, then update the Collection.

Grants survive ordinary app restarts, device restarts and in-place APK updates.
They do not survive clearing app data or uninstalling the app, and should not be assumed to transfer through backup to another device.
Select the folders again on the new installation.
If a grant is revoked, a document is moved or removed, or removable storage is disconnected, reads can fail.
Reconnect the storage or reselect the folder; use a Collection update where necessary.
Retaining permission is not a promise that a file remains present.

## Implementation

### Persisting and resolving grants

[`AndroidStorage`](src/core/androidstorage.cpp) retains **read** access using `ContentResolver.takePersistableUriPermission()`.
Android's persisted URI permissions are the authoritative registry; URI permissions are not reconstructed from settings strings.
The registry is loaded before Collection startup and refreshed after successful picker selections.
Failures to retain a selected grant are logged and surfaced through the existing selection-error UI.

This bridge supports local documents provided by `com.android.externalstorage.documents`, including primary storage and removable-volume document IDs.
It maps the provider's volume-relative document IDs to existing local paths.
Folder matching is component-boundary aware and uses the longest matching grant.
Cleaned paths outside the chosen folder are not covered; similarly named sibling directories are not covered.
An individual document grant covers only that file.
Document URIs are constructed with `DocumentsContract.buildDocumentUriUsingTree()`, preserving encoded spaces, percent signs, Unicode and other special characters.

Local individual documents from `com.android.providers.downloads.documents` are supported too.
For opaque IDs such as `msf:`, the resolver briefly opens the authorized document and reads its actual backing path through `/proc/self/fd/`.
Only regular local backing files are accepted; pipes, missing paths and deleted targets are rejected.
The descriptor is closed immediately; future reads use the original retained document URI.
Existing `raw:` Downloads IDs retain their explicit path mapping and now retain read grants too.
Opaque Downloads tree IDs are not resolved as folders.
At startup, individual Downloads grants are reopened to rebuild their path mappings.
Unavailable documents are logged and skipped without preventing other grants from loading.

The bridge does not add a generic cloud-provider filesystem or download remote documents.
Other existing picker-to-path conversions remain subject to their provider and Android permissions.
An unresolved item produces a selection error rather than being silently imported.

### Scanning, metadata and identity

[`androidstoragefileengine.cpp`](src/core/androidstoragefileengine.cpp) installs an Android-only Qt file-engine handler.
For paths covered by retained grants, file information, directory enumeration, seeks and reads delegate to Qt's document-URI engine.
The adapter exposes the original local filenames and directory hierarchy, so Collection identity, playlist URLs, relative CUE/playlist references and cache keys remain stable.
It neither copies the library into app-private storage nor stores temporary descriptor names as song URLs.

The adapter enumerates all document children before the outer Qt directory iterator applies its filters.
Otherwise a files-only recursive iterator would never see the intermediate directories.
Qt also reuses file engines while traversing subdirectories, so `setFileName()` must resolve the new directory's URI rather than reuse the parent's URI.
The bridge is read-only and does not permit tag editing, deletion or renaming through local aliases.
Paths not covered by a grant keep their normal filesystem handling.
Explicit document-URI save flows remain separate.
On the emulator, the temporary Now playing playlist was exported through the editor's Save button and Android's native create-document picker to an XSPF document in Downloads.
The exported document contained the original absolute song path and metadata; reopening it through Files > Choose files started playback, including after an app-process restart.
Cancelling a subsequent save left the exported file unchanged.

TagLib normally opens paths using the C runtime rather than `QFile`.
[`taglibfilestream_android.cpp`](src/tagreader/taglibfilestream_android.cpp) opens the authorized `QFile`, duplicates its descriptor and transfers ownership of that duplicate to TagLib's read-only `FileStream`.
It preserves the original filename for extension-based format selection.
Closing TagLib's stream releases the duplicate; no permanent descriptor is kept for a track.

### Playback, seeking and visualizations

[`gstqfilesrc.cpp`](src/engine/gstqfilesrc.cpp) registers an Android-only, higher-ranked GStreamer source for local `file:` URIs.
It reads through `QFile`, so the same grant bridge serves playback, discovery, moodbar and waveform pipelines.
HTTP and other network protocols are unchanged.
Ordinary local files also use this source on Android; desktop builds do not register it.

The source derives from `GstBaseSrc` and implements size queries and byte-offset reads.
Each requested offset is explicitly sought before reading.
This supports both pull-mode decoders and accurate seeking; a sequential-only wrapper around a descriptor is insufficient.
The source closes its file on pipeline shutdown and reports open, seek and read failures as GStreamer resource errors.
Only seekable provider documents are supported.

### Platform and build boundaries

Android-only sources are added under `if(ANDROID)` in the main build.
Shared startup and TagLib factory code contain guarded entry points; desktop readers and registration behavior remain unchanged.
The path adapter requires the **matching Qt Core private headers** (`Qt6::CorePrivate`), because Qt's public API does not expose custom file-engine registration.
This is a deliberate private-API dependency: rebuild against matching headers and rerun the bridge regressions when upgrading Qt.

## Limits and related audit decisions

- Access does not add codecs. WavPack is supported by the bundled decoder; WMA, APE and Musepack remain unsupported by the current Android decoder bundle.
- Granting read access does not enable local tag/lyrics editing, organizing or deletion. Feature audit point 9 remains in force.
- Automatic Collection monitoring still uses the filesystem watcher. The bridge does not add polling or guarantee background-process survival.
- Moodbar and waveform data can be cached privately without writing beside shared-storage music.
- App-private settings still contain credentials. Feature audit point 18 excludes that file from Android backups; this storage bridge does not encrypt credentials or bypass Android's sandbox.
- Grant revocation and removable-storage behavior depend on Android and the document provider. No all-files permission is requested.

## Regression checks

Build and run the focused native tests:

```bash
cmake --build build/native --target androidstoragefileengine_test gstqfilesrc_test \
  collectionwatcher_test songloader_test moodbarpipeline_test waveformpipeline_test --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build/native \
  -R '^(androidstoragefileengine_test|gstqfilesrc_test|collectionwatcher_test|songloader_test|moodbarpipeline_test|waveformpipeline_test)$' \
  --output-on-failure
```

The native bridge test uses a mock document provider; it does not prove Android permission behavior.
An emulator check through DocumentsUI's dedicated Downloads entry verified an `msf:` MP3 selection with metadata and playback.
A separate Downloads WavPack document classified as `application/octet-stream` was selected once.
After installing and starting the clean non-debuggable release, an M3U referring to its original local path loaded metadata and started playback without reselecting the WavPack file.
Android reported only its persisted read permission (`owned=0x0`, `persisted=0x1`), with no covering folder grant.
The M3U itself used a Downloads `raw:` document URI.
Removing the owned fixtures and restarting with their stale grants still reached the main window.

For Android verification, use a real picker-granted local folder and an owned `.wv` fixture:

1. Confirm MediaStore classifies the fixture as non-audio and an ordinary native-path open under the app UID is denied.
2. Verify Collection discovery and TagLib metadata without renaming the file.
3. Play the track and seek to a measurable position.
4. Restart the app without selecting the folder again and repeat playback.
5. Test moodbar and waveform generation from fresh private-cache entries.
6. Exercise nested folders and names containing spaces, `%`, `#` and Unicode.
7. Remove only the owned fixtures and their test records; restore seekbar mode and the non-debuggable release APK.

Do not treat mock-provider tests as proof of real cloud-provider support, grant revocation handling on every device, or write access.
