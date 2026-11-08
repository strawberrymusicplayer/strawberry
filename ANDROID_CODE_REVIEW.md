# Android Code Review

Code-quality review of all Android changes in this fork, compared with the upstream merge base (`82309fc6b`), for roadmap item 18.
The review covered the build files, manifest, and all 74 changed source files (about 5,200 added lines).

Each point is resolved separately: fix, build (Android and native), test, and commit.
Status values: Open, Done, Won't fix (with a reason).

## Resuming work

Handoff notes for continuing in a new session:

- Next: points 25, 26, and 27 (found during testing); the decision in point 20 is still open.
  Points 1–19, 21–24 are done.
- A physical phone may be connected over USB as well; set `ANDROID_SERIAL=emulator-5554` so `adb` commands only reach the emulator.
- Build Android with `cmake --build build --target strawberry_make_apk --parallel 4` (the plain `all` target can race in the APK copy step).
- Build and run native tests in `build/native` (see `CLAUDE.md`).
- Deploy: zipalign and sign with the debug keystore, `adb install -r`, launch with `adb shell monkey -p org.qtproject.example.strawberry 1` (see `ANDROID_EMULATOR_TESTING.md`).
- Start the emulator with `~/Android/Sdk/emulator/emulator -avd Medium_Phone_API_36.1 -no-snapshot-load -no-boot-anim -memory 4096 -gpu host -crash-report-mode never -no-metrics`; `-gpu swiftshader_indirect` is too slow, and a pending crash-report dialog blocks booting.
  The host GPU emulation can still segfault after a long session (`null ctx` errors in the log); just start it again.
- Write each screenshot to a new file name and view it only after the capture command has finished; viewing it in parallel shows the previous screenshot, which looks like display lag.
- Screenshots are shown at 900×2000 while the screen is 1080×2400; multiply coordinates read from a screenshot by 1.2 for `adb shell input tap`.
- Keep screenshots to a minimum: check only the screens a change affects, and use `adb` or log output instead where that confirms the result.
  Every screenshot stays in the session context, and after many visual checks the requests grow too large and fail with `408 Timed out reading request body`; start a new session if that happens.
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

**Status:** Done

- Compact buttons with a bold, link-colored value and full-page choice rows with the highlight color are implemented separately in `Equalizer`, `BehaviourSettingsPage`, and `AndroidSettingsPage`, with slightly different styling code.

**Resolved:**
- `AndroidWidgets` now provides `LayoutButton` (formerly `AndroidSettingsButton`), `ValueButton` with an optional "title >" caption and `SetValue()`, `AddButtonLabel()`, `StyleChoiceRow()`, and `CreateChoiceRow()`; all three use them.
- At the owner's request, selected choice rows are bold everywhere, as on the generic settings pages, and the equalizer keeps its titles as labels above value-only buttons.
- The Android APK builds and the native equalizer test passes.
  On the emulator, the equalizer Preset and Collapse behaviour values are bold and link-colored, and its choice rows are highlighted and bold at the 48 dp row height used by Settings.
  The Behavior summaries, On startup and combo choice pages, and Backend summaries look unchanged; a changed On startup choice updated the summary and Cancel restored it.

### 13. Use one wrapping layout

**Status:** Done

- `AndroidEqualizerBandLayout` and `PlaylistContainer::UpdateAndroidToolbar()` both implement a wrapping grid by hand.

**Resolved:**
- Added `GridFlowLayout` in `src/widgets/`, built on all platforms: equally sized cells, as many columns as fit (optionally limited), stretched to fill each row, with the height following from the width; hidden items are skipped.
- The equalizer bands use it with 4-pixel spacing and at most four columns; the playlist toolbar uses it without spacing, which removes `UpdateAndroidToolbar()`, its resize event filter, and the stored column count.
- Added `GridFlowLayoutTest` (7 tests); the native grid, equalizer, and playlist tests pass, and the Android APK and native desktop build succeed.
  On the emulator, the Bass bands fill three columns and the Treble bands four, as before, and the Now playing toolbar shows its six buttons in one evenly spaced row.

### 14. Extract the Android playlist header from MainWindow

**Status:** Done

- `MainWindow::SetupAndroidLayout()` builds the playlist header and rename editor with many lambdas, tracks state in an `editingPlaylistId` dynamic property, and finds the playlist page stack with `findChild` by object name.

**Resolved:**
- Added `PlaylistTitleHeader` in `src/widgets/` (upstream already has a `PlaylistHeader` for the column header), built on all platforms so it can be tested natively; the Android title font and Back button stay behind a guard.
  It knows nothing about `PlaylistManager`: `MainWindow` passes the current playlist with `SetPlaylist()`, connects `RenameRequested` to `PlaylistManager::Rename()`, and `BackClicked` to the page stack, which is now the member `android_playlist_pages_`.
  `SetupAndroidLayout()` shrinks by about 80 lines, and the unused `QLineEdit` include is gone.
- Added `PlaylistTitleHeaderTest` (8 tests): trimmed rename, empty and unchanged names, cancelling by leaving the editor or switching playlists, and Back.
  The Android APK and the native desktop build succeed.
- On the emulator, the header looks unchanged; renaming Playlist 9 updated the title and overview, an unconfirmed edit followed by Back was discarded and returned to the overview with the playlist still selected, and the original name was restored afterwards.

### 15. Share the mute button state update

**Status:** Done

- The mute icon and accessible name are set in both `SetupAndroidLayout()` and `MuteChanged()`; use one helper.

**Resolved:**
- Added `MainWindow::UpdateAndroidMuteButton()`, used by both.
  Both calls stay because the first `MuteChanged()` in the constructor runs before `SetupAndroidLayout()` creates the button; the helper returns early until then.
- The Android APK and the native desktop build succeed.
  On the emulator, the button starts with the volume icon (upstream does not persist mute), switches to the muted icon when tapped, and back when tapped again.

### 16. Share the Android action and confirmation dialogs

**Status:** Done

- The playlist deletion overlay in `PlaylistListContainer::Delete()` (about 100 lines, including custom-painted icons) and the full-screen action dialogs in `PlaylistView` and `PlaylistActionHandler` each implement full-screen layout, Back-key handling, and 48-pixel buttons.
- Fix: one reusable dialog class for these flows.

**Resolved:**
- Added `ActionDialog` in `src/widgets/`, built on all platforms so it can be tested natively.
  It is a frameless, application-modal window that covers the parent's window and follows its geometry, deletes itself when closed, and handles Back and Escape.
  It has two presentations: an action page with 48-pixel buttons (title at the top or above the buttons), and the approved compact confirmation with the red cross and green checkmark over a translucent backdrop.
- `AskConfirmation()` provides the second step of the song actions (more than `kUndoItemLimit` songs): it replaces the actions with the question and a confirm button, and Back returns to the actions.
- `PlaylistDeleteDialog` and the three hand-built layouts are gone; `PlaylistView`, `PlaylistActionHandler`, and `PlaylistListContainer` only add their buttons and connect their actions.
- The action pages now cover the app window below the status bar instead of the whole screen, like the deletion confirmation; touch input stays correct because the window is still at the window origin.
- Found during testing: Back after a long-press did nothing, also with the previous APK, because releasing the finger reactivated the main window.
  The dialog now reactivates itself while it is visible, and Back closes the song actions right after the long-press.
- Added `ActionDialogTest` (13 tests); the native action dialog, playlist action handler, and playlist tests pass, and the Android APK and native desktop build succeed.
- On the emulator, the song actions open on long-press, Back and Cancel keep the song, and Remove song removes it (restored with Undo).
  The playlist deletion confirmation looks unchanged, blocks background taps, Back and the cross cancel it, and the checkmark deleted a temporary folder.
  The collection actions were not checked on the emulator because its collection is empty; the native handler tests cover them.

### 17. Remove redundant rendering workarounds

**Status:** Done

- `SettingsDialog::event()` flushes the whole window on every update request, and `SetAndroidNavigationPage()` calls `repaint()` three times.
  Both work around the double-scaled flush region that `main.cpp` now avoids with `QT_WIDGETS_RHI=1`.
- Fix: verify on the emulator whether they are still needed and remove them if not.

**Resolved:**
- Both were added before `QT_WIDGETS_RHI=1` (October 29 and 30 versus November 1); removed the `event()` override, the three `repaint()` calls, and the `QBackingStore` and `QEvent` includes.
- The Android APK and the native desktop build succeed.
- On the emulator, the Settings overview, the General and User interface categories, and the Behavior, On startup, and Appearance pages render completely without stale rows.
  Choosing an On startup value updated the Behavior summary immediately; the Back key returned one level at a time with the parent's button still focused; and Cancel and reopening Settings showed a clean overview without the dialog buttons.

### 18. Evaluate the Behaviour page's separate Android adapter

**Status:** Done

- `BehaviourSettingsPage` has its own Android adaptation of about 130 lines alongside the shared `AndroidSettingsPage` adapter.
- Fix: move the remaining page-specific parts onto the shared adapter where behavior stays identical; otherwise document why the page differs.

**Resolved:**
- The shared adapter would change what the page shows, so at the owner's request the page keeps its own layout:
  - A group with one combo box shows its choices directly on its subpage and returns on selection; the adapter shows the combo box there and opens a second subpage with the choices.
  - Summaries show only the values; the adapter shows "Title: value", at most three lines, and "x of y enabled" for check boxes.
  - The double-tap group is replaced by an explanation of the Android behavior, the language label moves to the top, and the two double-click choice lists get headings.
- The Android block moved from the constructor into `BehaviourSettingsPage::SetupAndroidLayout()`, which documents these differences; `SettingsDialog::AddPage()` refers to it where it skips the adapter.
- The combo choice rows moved to the shared `AndroidWidgets::AddComboChoiceRows()`, which keeps the rows and the combo box in sync.
- The unused includes added for Android are removed, and the Android-only includes are guarded.
- The Android APK and the native desktop build succeed.
  On the emulator, the Behavior page looks unchanged; choosing "Restart song" for Previous closed the subpage and updated the summary, Cancel restored the original choice, and Back returned without errors.

### 19. Small playlist-document cleanups

**Status:** Done

- The `content` scheme check is repeated in `PlaylistManager::SaveWithUI()` and `PlaylistParser::Save()`.
- `PlaylistParser::Save()` checks `flush()` but not the result of closing a document.

**Resolved:**
- Added `PlaylistParser::IsDocumentUrl()`, used by both.
- The document save checks the write errors before closing, because a successful `QFileDevice::close()` clears an earlier error, and then checks the close; failures are logged and reported like those of the file save.
- The Android APK and the native desktop build succeed, and the native playlist and M3U parser tests pass.
  On the emulator, saving Playlist 9 through the system picker wrote the complete XSPF document to the test folder without errors; the file was removed afterwards.

### 23. Move large Android blocks into `*_android.cpp` files

**Status:** Done

- Large `#ifdef Q_OS_ANDROID` blocks in shared files make them harder to read and cause more conflicts when merging upstream.
  Approximate lines inside Android guards: `equalizer.cpp` 580, `settingsdialog.cpp` 380, `mainwindow.cpp` 330, `playlistview.cpp` 210.
  Upstream commits to these files since 2023 (up to the merge base): `mainwindow.cpp` 152, `playlistview.cpp` 49, `settingsdialog.cpp` 24, `equalizer.cpp` 15.
- Do this after points 10–19, so the code that gets moved has already been cleaned up.

**Plan:**
1. Do one file at a time, ordered by how often upstream changes the file, because that is where moving the code saves the most conflicts: `mainwindow.cpp`, `playlistview.cpp`, `settingsdialog.cpp`, `equalizer.cpp`.
   - Move whole Android-only functions and large blocks into `mainwindow_android.cpp` (and similarly named files) as private member functions or free helpers.
   - In the shared file, leave a single guarded call, for example `#ifdef Q_OS_ANDROID` `SetupAndroidUi();` `#endif`.
   - Add the new files under an Android condition in `CMakeLists.txt`, in the same way the existing platform files (`_win.cpp`, `.mm`) are selected.
   - Leave small one-line guards where they are; moving them only adds more indirection.
2. Check each step: build the Android APK and `build/native`, run the focused native tests, and spot-check on the emulator.
   The main thing to confirm is that the desktop build is unchanged in behavior.
3. Afterwards, merge upstream to confirm that the shared files conflict less.

**Progress:**
- `mainwindow.cpp`: done.
  `SetupAndroidLayout()`, `CreateAndroidToolsPage()`, and `UpdateAndroidMuteButton()` moved unchanged to `src/core/mainwindow_android.cpp`, which `CMakeLists.txt` adds only for Android.
  The Settings tab code moved there too, as `CreateAndroidSettingsTab()`, `SetupAndroidSettingsTab()`, `EmbedAndroidSettingsDialog()`, and `ShowAndroidSettingsDialog()`; each is called from a single guarded line at its original place, so the startup order is unchanged.
  The includes only the Android code used are removed from `mainwindow.cpp`, which has about 180 fewer lines.
  The short `#else` branches (file pickers, sidebar, double-click handling, status bar) stay in place.
  The Android APK, the native desktop build, and the native playlist, long-press, and tab widget tests succeed.
  On the emulator, the layout, the Tools page, and the Settings tab (overview with Help, Back returning to the previous tab) work as before.
- `playlistview.cpp`: done.
  `UpdatePortraitColumns()`, `ShowAndroidSongActions()`, and `CancelAndroidSongActions()` moved unchanged to `src/playlist/playlistview_android.cpp`.
  The long-press setup from the constructor, the model connections from `setModel()`, and the long-press start from `mousePressEvent()` moved there as `SetupAndroidLongPress()`, `DisconnectAndroidSongActions()`, `ConnectAndroidSongActions()`, and `PressAndroidLongPress()`.
  The short guards with an early return (mouse move and release, event filter) and the one-line calls stay in place; `playlistview.cpp` keeps only the long-press handler include.
  The Android APK, the native desktop build, and the native playlist, long-press, and tab widget tests succeed.
  On the emulator, a playlist shows the portrait columns, and long-pressing a song in Now playing opened the song actions; Remove song removed it with Undo available.
- `settingsdialog.cpp`: done.
  The navigation setup from the constructor moved to `SetupAndroidNavigation()` in `src/settings/settingsdialog_android.cpp`, together with the Android-only functions (`OpenOverview()`, `AddAndroidHelpCategory()`, the subpage functions, `keyPressEvent()`, `AndroidBack()`, `SetAndroidNavigationPage()`, and `SetAndroidTitle()`), unchanged.
  The short guards in `OpenAtPage()`, `showEvent()`, `accept()`, `reject()`, `LoadGeometry()`, `AddPage()`, and `CurrentItemChanged()` stay in place.
  The includes only the moved code used are removed, and the Android headers that remain are guarded.
  The Android APK, the native desktop build, and the native playlist, long-press, and tab widget tests succeed.
  On the emulator, Settings > General > Behavior and its Previous subpage show the right breadcrumbs, and hardware Back returned one level at a time to the overview and then to the previous tab.
- `equalizer.cpp`: done.
  `SetupAndroidLayout()` and the other Android-only functions moved unchanged to `src/equalizer/equalizer_android.cpp`, with the constants and helpers only they use.
  `kAndroidCurrentParams` and `kCustomPreset`, which both files use, are now private class constants in `equalizer.h`; the Android file names the `Equalizer` settings group itself, so upstream's constant is untouched.
  The short guards in the constructor, `ReloadSettings()`, the preset functions, `Save()`, and `closeEvent()` stay in place; `equalizer.cpp` has about 380 fewer lines.
  The Android APK, the native desktop build, and the native equalizer, playlist, long-press, and tab widget tests succeed.
  On the emulator, the equalizer looks unchanged; choosing "All expanded" for the collapse behaviour persisted after closing and reopening it (restored afterwards).
- Merge check: a trial merge with `git merge-tree` against `upstream/master` (45 commits after the merge base, including changes to `mainwindow.cpp`, `playlistview.cpp`, and `CMakeLists.txt`) gives the same result before and after this point: only an unrelated conflict in `tests/CMakeLists.txt`.
  These upstream changes do not touch the moved code, so the reduction in conflicts will only show with later upstream changes near the Android code.

## Needs a decision

### 20. Application ID and backup settings

**Status:** Open

- The manifest still uses the Qt template ID `org.qtproject.example.strawberry`, and `android:allowBackup="true"` backs up the settings and database.
- Changing the ID makes the app install separately from existing installations; this needs the owner's decision before changing it.

## Found during testing

### 21. Hide the dialog buttons on the Settings overview after Cancel

**Status:** Done

- After Cancel (or OK) from a settings page, reopening Settings shows the overview with the Apply, Cancel, and OK buttons still visible.
  `SettingsDialog::OpenOverview()` does not hide the button box, unlike the other overview and category paths.

**Resolved:**
- `OpenOverview()` hides the button box.
- The Android APK and the native build succeed.
  On the emulator, the bug reproduced with the previous APK; with the fix, reopening Settings after Cancel and after OK from Behavior shows the overview without the buttons.

### 22. Show the equalizer values in the link color

**Status:** Done

- The Android equalizer's Preset and Collapse behaviour of sections buttons show their values in bold black, while Settings shows values in the bold link color, as the Android guidelines require.
  Resolve together with point 12.

**Resolved:**
- The equalizer set a palette color on its value labels, which showed black under the page's style sheet; the shared `ValueButton` uses the `Link` foreground role like Settings, and the emulator now shows the values in the link color.

### 24. Keep pages beside the left tab bar

**Status:** Done

- Some pages cover the left tab bar instead of sharing the screen with it like the sidebar tabs (Settings, Tools, Playlists).
  These pages open as separate top-level windows sized to the whole main window or screen:
  - Equalizer: `MainWindow::ShowEqualizer()` sets its geometry to the main window's.
  - Song and collection action page (with Remove song): `ActionDialog::SetupPage()` uses `Qt::WindowFullScreen`.
  - Probably also the cover manager and transcoder, opened from Tools as separate windows; check on the emulator.
- While these pages are open, the tab bar is hidden or unreachable.
  Show them in the content area beside the bar instead, either as pages in the tab's content stack (as Settings does with `android_settings_tab_`) or with geometry limited to the content area.
  Keep Back returning exactly one level, and leave the confirmation overlay from point 16 covering the full window, since it is modal.
- Afterwards, check that Back, the Android keyboard, and rotation still work on each page.

**Resolved:**
- Limiting the windows' geometry to the content area was not used: Android delivers offset touches to positioned windows (see point 16), and a separate window would still have to follow tab changes.
- The Tools tab is a stack of the tool list and the opened tool.
  `MainWindow::ShowAndroidTool()` (in `mainwindow_android.cpp`) embeds the cover manager, equalizer, and transcoder there as child widgets; the cover manager and transcoder get a "Tools > …" breadcrumb with a Back button, while the equalizer keeps its own header.
  Closing a tool (Back, its Back or Close button) returns to the list, and tapping the Tools tab closes the open tool, like the Playlists tab returns to its list.
  `ShowCoverManager()`, `ShowEqualizer()`, and `ShowTranscodeDialog()` call it from a single guarded line; the equalizer is no longer application-modal, and `~MainWindow()` detaches it again because the player shares its ownership.
  The cover manager and transcoder only restore and center their geometry while they are windows.
- An `ActionDialog` action page inside a tab is now a child of that tab's page, covering only the content beside the tab bar; switching tabs cancels it.
  Outside a tab, and for the compact confirmation from point 16, it still covers the whole window as a modal window.
  Added `ActionDialogTest.PageInsideATabCoversOnlyTheTabPage`, `BackRejectsThePageInsideATab`, and `ConfirmationInsideATabStillCoversTheWindow`.
- The Android APK, the native desktop build, and the native action dialog, equalizer, playlist, long-press, and tab widget tests succeed.
  On the emulator, the three tools and the song actions in Now playing show beside the tab bar.
  Back returned one level at a time (equalizer preset list, equalizer, Tools list), Close and the header Back buttons returned to the list, tapping the Tools tab closed an open tool, switching tabs closed the song actions, and Remove song still worked.
  In landscape the cover manager stays beside the tab bar, and its search field receives keyboard input.
- With the emulator's hardware keyboard, Gboard shows a floating toolbar instead of the keyboard, and while it is shown Back does not reach the app; the equalizer's existing preset name field behaves the same.
  Check Back with the on-screen keyboard on a phone.

### 25. Lay out the Settings value buttons like the equalizer's

**Status:** Open

- The equalizer shows each setting's title as a word-wrapped label above the button, and the button shows only the bold, link-colored current value (`Equalizer::SetupAndroidLayout()`: `ValueButton(QString(), ...)` with a buddy `QLabel`).
  Settings puts the "title >" caption and the value together inside the button (`ValueButton(title, ...)` in `AndroidSettingsPage` and `BehaviourSettingsPage`).
- At the owner's request, the Settings value buttons must follow the equalizer: title label above the button, current value inside it.
  Apply this to every button that shows a value, on the Behavior page and on the pages using the shared `AndroidSettingsPage` adapter.
- Put the label and button pairing in `AndroidWidgets`, for example as a helper that creates the label, sets it as the button's buddy, and sets the button's accessible name, so the equalizer, Behavior, and the adapter share it.
  Keep the label hidden whenever the button is hidden, for example for the Language group.
- Navigation-only buttons without a value, such as the Settings overview categories, keep their title in the button.
- Afterwards, check on the emulator that long titles and values wrap on narrow screens, that summaries still update after edits and Cancel, and that the parent's scroll position is still restored on return.

### 26. Ignored first tab tap after a cold start

**Status:** Open

- After a cold start, the first tap on the Settings tab does nothing; the next tap opens Settings.
  Tapping Tools first can also be ignored when tapped about 15 seconds after the launch, but works after 25 seconds.
- Found while testing point 23; the APK from point 16 behaves the same in a side-by-side check (cold start, wait 25 seconds, one tap), so it is not caused by moving the code.
- Likely causes to check: the lazy creation of `SettingsDialog` on the first `OpenSettingsDialog()` call returning to the previous tab, or touches arriving while startup work still blocks the event loop.

### 27. Back in the playlist editor quits the app

**Status:** Open

- In the Playlists tab, the hardware Back key in an open playlist quits the app instead of returning to the playlist list, as the header's Back button does.
  Nothing handles `Qt::Key_Back` there, so it reaches the main window, which closes (the process exits cleanly).
- Found while testing point 24; the playlist editor code was not changed by it.
- Check the other sidebar tabs for the same problem: at their root, Back should leave the app only as the Android convention expects.
