# Android Code Review

Code-quality review of all Android changes in this fork, compared with the upstream merge base (`82309fc6b`), for roadmap item 18.
The review covered the build files, manifest, and all 74 changed source files (about 5,200 added lines).

Each point is resolved separately: fix, build (Android and native), test, and commit.
Status values: Open, Done, Won't fix (with a reason).

## Resuming work

Handoff notes for continuing in a new session:

- Next: point 12 (share the value buttons and choice rows; resolve point 22 with it); point 21 was also found during testing.
- Build Android with `cmake --build build --target strawberry_make_apk --parallel 5` (the plain `all` target can race in the APK copy step).
- Build and run native tests in `build/native` (see `CLAUDE.md`).
- Deploy: zipalign and sign with the debug keystore, `adb install -r`, launch with `adb shell monkey -p org.qtproject.example.strawberry 1` (see `ANDROID_EMULATOR_TESTING.md`).
- Start the emulator with `emulator -avd Medium_Phone_API_36.1 -no-snapshot-load -no-boot-anim -memory 4096 -gpu host -crash-report-mode never -no-metrics`; `-gpu swiftshader_indirect` is too slow, and a pending crash-report dialog blocks booting.
- Write each screenshot to a new file name and view it only after the capture command has finished; viewing it in parallel shows the previous screenshot, which looks like display lag.
- Screenshots are shown at 900×2000 while the screen is 1080×2400; multiply coordinates read from a screenshot by 1.2 for `adb shell input tap`.
- Test music is in `/sdcard/Music/StrawberryTest`; the release APK is not debuggable, so check settings through the UI.

## Correctness and safety

### 1. Detect failed JNI calls and consolidate JNI lookups

**Status:** Done

- Qt 6.7's `QJniObject` already clears and prints Java exceptions after each call, so a pending exception cannot abort the app.
  A failed call does, however, return a default value that callers did not distinguish from success.
- `GStreamer.init` failures were only printed as a generic Qt warning, and a failed `checkSelfPermission` returned 0, which equals `PERMISSION_GRANTED`.
- The `ViewConfiguration` long-press timeout and touch-slop lookup was duplicated in `PlaylistView` and `PlaylistActionHandler`.

**Resolved:**
- `GstStartup::Initialize()` and `AndroidStorage::RequestAudioPermission()` call through `QJniEnvironment`, check for exceptions, and log failures; a failed permission check still requests the permission.
- `LongPressHandler::PlatformInterval()` and `PlatformDragDistance()` provide the Android values with a Qt style-hint fallback; both callers use them.
- Added `LongPressHandlerTest.PlatformValuesFallBackToStyleHints`.
  Native long-press, playlist action, and playlist tests pass.
  On the emulator, GStreamer initializes without errors and long-pressing songs in Now playing opens the song actions.

### 2. Share the content-URI picker resolution

**Status:** Done

- `MainWindow::AddFile()`, `MainWindow::AddFolder()`, `FileView::SelectFiles()`, `FileView::SelectFolder()`, and `CollectionSettingsPage::AddDirectory()` each repeated the loop that maps picker URLs to local paths and collects unresolved ones, with different wording.

**Resolved:**
- Added `AndroidStorage::LocalPathsForUrls()`, which returns cleaned paths and the unresolved URLs, and `AndroidStorage::UnresolvedMessage()`; all five callers use them.
  Each caller keeps its existing dialog type.
- `FileView` shares the picker start directory and the Now playing hand-off between its file and folder actions.
- Emulator: Files > Choose files and Choose folder resolved a single file and the test folder (three tracks) and started playback.

### 3. Remove unreachable code and preprocessor slips

**Status:** Done

- `Equalizer::closeEvent()` and `FileView::showEvent()` used `#ifdef Q_OS_ANDROID return; #endif`, leaving unreachable code on Android.
- `SettingsDialog::showEvent()` had misindented `#ifdef` blocks around the settings-load guard.
- `transliterate.cpp` included `config.h` twice.

**Resolved:**
- Both functions use `#ifndef`/`#else` blocks; `SettingsDialog::showEvent()` computes one `load_settings` condition per platform.
- Removed the duplicate include.
- Native equalizer and utilities tests pass.
  On the emulator, an unapplied Behavior edit survived switching to Files and back, and Cancel discarded it.

### 4. Correct copyright headers of new files

**Status:** Done

- `longpresshandler`, `playlistactionhandler`, and their tests were written in this fork but named the upstream maintainer as copyright holder.

**Resolved:**
- At the owner's request, removed the incorrect lines without adding a name; all new files now carry only the GPL notice, like `androidstorage` and `androidsettingspage`.

### 5. Keep desktop behavior unchanged

**Status:** Done

- The playlist toolbar showed an Add file button on all platforms (`playlistcontainer.ui`), and `action_add_file` used the `list-add` icon everywhere.
- `mainwindow.ui` adds the Remove duplicates and Remove unavailable actions to the playlist widget on all platforms.

**Resolved:**
- With the owner's approval, the Add file button and `list-add` icon are Android-only; the desktop again uses the upstream `document-open` icon without the toolbar button.
- The playlist widget actions are kept: they keep the shortcuts working while the Android menu bar is hidden and do not change the desktop, where the same actions are already in the menus.
- Added `PlaylistTest.DesktopPlaylistToolbarHasNoAddFileButton`; the full native playlist test passes.
  The Android playlist editor still shows the Add file button.

## Build configuration

### 6. Clean up the Android CMake configuration

**Status:** Done

- The GStreamer SDK root was resolved twice, as `GStreamer_ROOT_DIR` and `ANDROID_GSTREAMER_ROOT`, with different precedence.
- `androidsettingspage.cpp`/`.h` were built on every platform although they are entirely wrapped in `#ifdef Q_OS_ANDROID`.
- `-Wl,--allow-multiple-definition` hid duplicate symbols when linking `libgstreamer_android.so`.

**Resolved:**
- Only `GStreamer_ROOT_DIR`, the variable used by the GStreamer SDK's CMake modules, remains; an existing `ANDROID_GSTREAMER_ROOT` cache value or `GSTREAMER_ROOT_ANDROID` environment variable still sets it.
- The settings adapter is added to the sources only for Android, like `androidstorage.cpp`.
- Removed the linker flag: `libgstreamer_android.so` links without duplicate-symbol errors, GStreamer initializes on the emulator, and the test folder plays.
  The native desktop build still succeeds.

## Structure and duplication

### 7. Name the equalizer settings keys and modes

**Status:** Done

- The settings keys `android_current_params`, `android_foldout_mode`, and `android_section_%1` were inline literals; the section keys depended on section order.
- The foldout mode was a bare `0`/`1`/`2` integer, and its display names were listed twice.
- `QT_TRANSLATE_NOOP("Equalizer", "Custom")` was repeated in the Android code.

**Resolved:**
- Named constants for the keys and the Custom preset; `AndroidFoldoutMode` and `AndroidSection` enums with fixed values keep the stored settings and keys unchanged.
- One `AndroidFoldoutModeName()` provides the display names; the choice page iterates over the modes.
- Native equalizer tests pass.
  On the emulator, the saved mode loaded, One section at a time closed Bass when Treble opened, and the mode and open section persisted after a restart; the original All collapsed state was restored afterwards.

### 8. Simplify the equalizer gain control

**Status:** Done

- `EqualizerSlider::OnValueChanged()` duplicated the dB conversion of `gain_db()`.
- `gain_editor_` was stored as `QDoubleSpinBox *` and cast with `static_cast<AndroidGainSpinBox *>`.
- The gain colors were scattered hex literals.

**Resolved:**
- `OnValueChanged()` uses `gain_db()`; the Android editor update is shared in `UpdateGainEditor()`; `gain_editor_` has the real type `EqualizerGainSpinBox`; colors and the preamp mute value are named constants.
- Added fractional dB label checks; all 7 native equalizer tests pass and the Android APK builds.
- On the emulator, the Bass up arrow gave +0.96 in green, the down arrow −0.96 in red, and 0.00 stayed blue; disabling the equalizer greyed out the arrows.
  The gains were restored to 0 and the equalizer disabled afterwards.

### 9. Stop rebuilding the equalizer preset button

**Status:** Done

- `Equalizer::UpdateAndroidPreset()` deleted and recreated the button layout and label on every gain change; `UpdateAndroidFoldouts()` found its label with `findChild`.

**Resolved:**
- `AddAndroidValueLabel()` creates the bold, link-colored value label of both buttons once; `android_preset_value_` and `android_foldout_mode_label_` keep them, and the update functions only set the text.
- The Android APK builds and the native equalizer test passes.
  On the emulator, selecting Classical and raising 60 Hz showed "Classical *" right away; selecting Custom and disabling the equalizer restored the original state.

### 10. Share the Android page header

**Status:** Done

- The breadcrumb title (bold, 18 pt) with a Back button (32-pixel `go-previous` icon, 48-pixel height) was built separately in `MainWindow::SetupAndroidLayout()`, `Equalizer::SetupAndroidLayout()`, and the `SettingsDialog` constructor.

**Resolved:**
- Added the Android-only `src/widgets/androidwidgets.cpp` with `AndroidWidgets::SetPageTitleFont()`, `CreateBackButton()`, `CreatePageHeader()`, and the shared `kMinimumTouchSize` and `kNavigationIconSize` constants.
  The equalizer and settings headers use `CreatePageHeader()`; the playlist header, whose title is a rename button, uses the title font and Back button.
- The Android APK builds; the native equalizer and playlist tests pass.
  On the emulator, the Equalizer, Settings, and Now playing headers look unchanged, and the playlist Back button returns to the playlist list.

### 11. Share the large Android navigation buttons

**Status:** Done

- Left-aligned buttons with 115% text and 24-pixel padding are built separately in the Tools page, the Files page, and the Settings categories and Help page.
- The scroll area with a window-colored background is repeated three times in `SettingsDialog`.

**Resolved:**
- Added `AndroidWidgets::StyleNavigationButton()`, which takes the minimum height so each page keeps its button size (Tools 72, Settings 64, Help and Files 56), plus `SetWindowBackground()` and `CreateScrollPage()`.
  The Settings category and Help pages use `CreateScrollPage()`, and the settings page scroll areas use `SetWindowBackground()`.
- The Tools page keeps its own scroll area because it uses the style's default margins without a window-filled background.
- The Android APK and the native desktop build succeed.
  On the emulator, the Tools, Files, Settings overview, General, Help, and Behavior pages look unchanged.

### 12. Share the value buttons and choice rows

**Status:** Open

- Compact buttons with a bold, link-colored value and full-page choice rows with the highlight color are implemented separately in `Equalizer`, `BehaviourSettingsPage`, and `AndroidSettingsPage`, with slightly different styling code.
- Fix: one implementation used by all three, preserving the documented appearance.

### 13. Use one wrapping layout

**Status:** Open

- `AndroidEqualizerBandLayout` and `PlaylistContainer::UpdateAndroidToolbar()` both implement a wrapping grid by hand.
- Fix: one reusable flow layout in `src/widgets/`, built on all platforms and covered by a native unit test.

### 14. Extract the Android playlist header from MainWindow

**Status:** Open

- `MainWindow::SetupAndroidLayout()` builds the playlist header and rename editor with many lambdas, tracks state in an `editingPlaylistId` dynamic property, and finds the playlist page stack with `findChild` by object name.
- Fix: a small widget class for the header and rename editor, with the page stack kept as a member; add a native test for renaming and cancellation.

### 15. Share the mute button state update

**Status:** Open

- The mute icon and accessible name are set in both `SetupAndroidLayout()` and `MuteChanged()`; use one helper.

### 16. Share the Android action and confirmation dialogs

**Status:** Open

- The playlist deletion overlay in `PlaylistListContainer::Delete()` (about 100 lines, including custom-painted icons) and the full-screen action dialogs in `PlaylistView` and `PlaylistActionHandler` each implement full-screen layout, Back-key handling, and 48-pixel buttons.
- Fix: one reusable dialog class for these flows.

### 17. Remove redundant rendering workarounds

**Status:** Open

- `SettingsDialog::event()` flushes the whole window on every update request, and `SetAndroidNavigationPage()` calls `repaint()` three times.
  Both work around the double-scaled flush region that `main.cpp` now avoids with `QT_WIDGETS_RHI=1`.
- Fix: verify on the emulator whether they are still needed and remove them if not.

### 18. Evaluate the Behaviour page's separate Android adapter

**Status:** Open

- `BehaviourSettingsPage` has its own Android adaptation of about 130 lines alongside the shared `AndroidSettingsPage` adapter.
- Fix: move the remaining page-specific parts onto the shared adapter where behavior stays identical; otherwise document why the page differs.

### 19. Small playlist-document cleanups

**Status:** Open

- The `content` scheme check is repeated in `PlaylistManager::SaveWithUI()` and `PlaylistParser::Save()`.
- `PlaylistParser::Save()` checks `flush()` but not the result of closing a document.

## Needs a decision

### 20. Application ID and backup settings

**Status:** Open

- The manifest still uses the Qt template ID `org.qtproject.example.strawberry`, and `android:allowBackup="true"` backs up the settings and database.
- Changing the ID makes the app install separately from existing installations; this needs the owner's decision before changing it.

## Found during testing

### 21. Hide the dialog buttons on the Settings overview after Cancel

**Status:** Open

- After Cancel (or OK) from a settings page, reopening Settings shows the overview with the Apply, Cancel, and OK buttons still visible.
  `SettingsDialog::OpenOverview()` does not hide the button box, unlike the other overview and category paths.

### 22. Show the equalizer values in the link color

**Status:** Open

- The Android equalizer's Preset and Collapse behaviour of sections buttons show their values in bold black, while Settings shows values in the bold link color, as the Android guidelines require.
  Resolve together with point 12.
