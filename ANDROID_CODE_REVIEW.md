# Android Code Review

Code-quality review of all Android changes in this fork, compared with the upstream merge base (`82309fc6b`), for roadmap item 18.
The review covered the build files, manifest, and all 74 changed source files (about 5,200 added lines).

Each point is resolved separately: fix, build (Android and native), test, and commit.
Status values: Open, Done, Won't fix (with a reason).

## Correctness and safety

### 1. Check JNI exceptions and consolidate JNI calls

**Status:** Open

- `AndroidStorage`, `GstStartup::Initialize()` (`GStreamer.init`), `PlaylistView`, and `PlaylistActionHandler` call Java methods without checking for exceptions.
  A pending Java exception (for example a `SecurityException` from a content resolver query, or `GStreamer.init` failing) aborts the app on the next JNI call.
- The `ViewConfiguration` long-press timeout and touch-slop lookup is duplicated in `PlaylistView` and `PlaylistActionHandler`.
- Fix: check and clear exceptions with `QJniEnvironment::checkAndClearExceptions()` and log failures; move the `ViewConfiguration` lookup into one Android helper.

### 2. Share the content-URI picker resolution

**Status:** Open

- `MainWindow::AddFile()`, `MainWindow::AddFolder()`, `FileView::SelectFiles()`, `FileView::SelectFolder()`, and `CollectionSettingsPage::AddDirectory()` each repeat the loop that maps picker URLs to local paths and collects unresolved ones.
- Their error reporting differs (`ShowErrorDialog`, `QMessageBox::warning`, `QMessageBox::Critical`) and so does the wording.
- Fix: add one `AndroidStorage` helper that resolves a list of URLs, returning the paths and the unresolved URLs, and use consistent messages.

### 3. Remove unreachable code and preprocessor slips

**Status:** Open

- `Equalizer::closeEvent()` and `FileView::showEvent()` use `#ifdef Q_OS_ANDROID return; #endif`, leaving unreachable code on Android; use `#ifndef`/`#else` instead.
- `SettingsDialog::showEvent()` has misindented `#ifdef` blocks around the settings-load guard.
- `transliterate.cpp` includes `config.h` twice.

### 4. Correct copyright headers of new files

**Status:** Open

- `longpresshandler`, `playlistactionhandler`, and their tests were written in this fork but name the upstream maintainer as copyright holder.
- `androidstorage` and `androidsettingspage` have no copyright line.
- Fix: use the correct author for all new files.

### 5. Keep desktop behavior unchanged

**Status:** Open

- The playlist toolbar now shows an Add file button on all platforms (`playlistcontainer.ui`), and `action_add_file` uses the `list-add` icon everywhere.
- `mainwindow.ui` adds the Remove duplicates and Remove unavailable actions to the playlist widget on all platforms.
- Fix: guard these Android adaptations, as the project rules require, unless the desktop change is intentional.

## Build configuration

### 6. Clean up the Android CMake configuration

**Status:** Open

- The GStreamer SDK root is resolved twice, as `GStreamer_ROOT_DIR` and `ANDROID_GSTREAMER_ROOT`, from the same environment variable.
- `androidsettingspage.cpp`/`.h` are built on every platform although they are entirely wrapped in `#ifdef Q_OS_ANDROID`; `androidstorage.cpp` is correctly added only for Android.
- `-Wl,--allow-multiple-definition` hides duplicate symbols; document why it is needed or remove it if the build no longer requires it.

## Structure and duplication

### 7. Name the equalizer settings keys and modes

**Status:** Open

- The settings keys `android_current_params`, `android_foldout_mode`, and `android_section_%1` are inline literals; the section keys depend on section order.
- The foldout mode is a bare `0`/`1`/`2` integer, and its display names are listed twice.
- `QT_TRANSLATE_NOOP("Equalizer", "Custom")` is repeated five times.
- Fix: named constants, an enum for the foldout mode, and stable section keys (keeping existing saved values readable).

### 8. Simplify the equalizer gain control

**Status:** Open

- `EqualizerSlider::OnValueChanged()` duplicates the dB conversion of `gain_db()`.
- `gain_editor_` is stored as `QDoubleSpinBox *` and cast with `static_cast<AndroidGainSpinBox *>`; store the actual type.
- The gain colors are scattered hex literals; give them named constants without changing the requested appearance.

### 9. Stop rebuilding the equalizer preset button

**Status:** Open

- `Equalizer::UpdateAndroidPreset()` deletes and recreates the button layout and label on every gain change; `UpdateAndroidFoldouts()` finds its label with `findChild`.
- Fix: keep the value labels as members and update their text.

### 10. Share the Android page header

**Status:** Open

- The breadcrumb title (bold, 18 pt) with a Back button (32-pixel `go-previous` icon, 48-pixel height) is built separately in `MainWindow::SetupAndroidLayout()`, `Equalizer::SetupAndroidLayout()`, and the `SettingsDialog` constructor.
- Fix: one shared factory or widget, with shared touch-size constants.

### 11. Share the large Android navigation buttons

**Status:** Open

- Left-aligned buttons with 115% text and 24-pixel padding are built separately in the Tools page, the Files page, and the Settings categories and Help page.
- The scroll area with a window-colored background is repeated three times in `SettingsDialog`.
- Fix: shared helpers next to the page header.

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
