# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Strawberry is a music player and music collection organizer, forked from Clementine in 2018. C++17, Qt 6 (6.4+), GStreamer audio backend. Runs on Linux, *BSD, macOS, and Windows.

## Build & Test

```bash
# Configure + build (standard)
cmake -S . -B build
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

Qt Creator build configs already exist under `build/Qt_Sys-{Release,RelWithDebInfo,MinSizeRel,Profile}/` — use those directories with Qt Creator, or a fresh `build/` for command-line work.

Useful CMake options: `-DBUILD_WERROR=ON`, `-DENABLE_DEBUG_OUTPUT=ON`.

On this development machine, build with at most two parallel jobs (`--parallel 2`), for the Android APK and the native build alike, instead of `$(nproc)`.

### Separate native test build

The local Linux Debug test build is in `build/native/`, configured with system GCC, Qt 6.4.2, ICU, and GStreamer.
The existing `build/` remains the Android build; do not reconfigure it for Linux.
Both build directories and the local dependencies below are ignored by Git.

KDSingleApplication 1.1.0 is built statically for Qt 6 and installed only in `build/native-deps/install/`, since Ubuntu 24.04's package is older than the required version.
To recreate that dependency:

```bash
git clone --depth 1 --branch v1.1.0 https://github.com/KDAB/KDSingleApplication.git build/native-deps/KDSingleApplication
cmake -S build/native-deps/KDSingleApplication -B build/native-deps/kdsingleapplication-build \
  -DKDSingleApplication_QT6=ON -DKDSingleApplication_STATIC=ON \
  -DKDSingleApplication_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/native-deps/install"
cmake --build build/native-deps/kdsingleapplication-build --parallel 2
cmake --install build/native-deps/kdsingleapplication-build
```

The native test configuration disables optional integrations whose development dependencies are not installed on this host.
These are local cache settings, not changes to the project's defaults or Android configuration:

```bash
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$PWD/build/native-deps/install" \
  -DENABLE_PULSE=OFF -DENABLE_CHROMAPRINT=OFF -DENABLE_SONGTRACKING=OFF \
  -DENABLE_TAGFETCHER=OFF -DENABLE_LIBSECRET=OFF -DENABLE_LAYERSHELLQT=OFF \
  -DENABLE_AUDIOCD=OFF -DENABLE_MTP=OFF -DENABLE_GPOD=OFF \
  -DENABLE_GSTFASTSPECTRUM=OFF -DENABLE_EBUR128=OFF \
  -DENABLE_QPA_QPLATFORMNATIVEINTERFACE=OFF -DENABLE_STREAMTAGREADER=OFF
cmake --build build/native --target playlist_test longpresshandler_test --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build/native \
  -R '^(playlist_test|longpresshandler_test)$' --output-on-failure
```

Rerun configuration after installing additional optional dependencies and explicitly enable the corresponding features if desired.

Validation on 2026-11-03: both native test binaries built successfully.
All eight long-press tests and 14 playlist removal regressions passed, including both new playlist-view tests.
To rerun that focused selection:

```bash
QT_QPA_PLATFORM=offscreen \
  GTEST_FILTER='LongPressHandlerTest.*:PlaylistTest.ViewRemoval*:PlaylistTest.ScatteredRemovalIsOneUndoStep:PlaylistTest.RemoveRowsAfterRemovedCurrentKeepsPosition:PlaylistTest.RemoveManyRowsKeepsCurrentItem:PlaylistTest.RemoveCurrent*' \
  ctest --test-dir build/native -R '^(playlist_test|longpresshandler_test)$' --output-on-failure
```

### Tests

Tests are built only when GTest, GMock, and Qt6 Test are found (`tests/` is added conditionally). They are `EXCLUDE_FROM_ALL`, so build them explicitly:

```bash
cmake --build build --target build_tests        # build all test binaries
cd build && ctest -V                            # run all tests
cmake --build build --target run_strawberry_tests   # build + run via ctest

# Run a single test: build its target, then run the binary directly
cmake --build build --target utilities_test
./build/tests/utilities_test
```

Test sources live in `tests/src/*_test.cpp` and are registered in `tests/CMakeLists.txt` via `add_test_file(<source> <gui_required>)`. The second argument links `test_gui_main` (needs a GUI/QApplication) vs `test_main`. To add a test, drop the file in `tests/src/` and add an `add_test_file(...)` line. `lyrics_live_tests` hits real network endpoints and is intentionally excluded from the default run.

### Visual checks and screenshots

Keep screenshots to a minimum when verifying UI changes on the Android emulator, the desktop, or in a browser.
Check only the screens a change affects, and prefer `adb`, log output, or tests wherever they can confirm the result.
Every screenshot stays in the session context; after many visual checks the requests grow too large and fail with `408 Timed out reading request body`.
If that happens, start a new session; retrying in the same one keeps failing.

## Code Style

`clang-format` config is in `.clang-format` (LLVM-based, customized). Run it on changed files before committing. Notable: comments use long lines, one sentence per line, no 80-column wrapping.

Commit messages: first line is `ClassName: short explanation` (no trailing period); omit the class name if multiple classes change. Body (after a blank line) is prose explaining what/why. Reference issues with `Fixes #NNNN`. See `CONTRIBUTING.md`.

## Architecture

`src/main.cpp` constructs a single `Application` (`src/core/application.h`), which is the central dependency-injection container. Nearly every subsystem (database, player, collection, cover/lyrics providers, scrobbler, streaming/radio services, task manager, network) is created and owned here and exposed via accessor methods returning `SharedPtr<T>`. When adding a subsystem, wire it through `Application`/`ApplicationImpl` rather than constructing it ad hoc.

Smart-pointer aliases live in `src/includes/`: `SharedPtr<T>` (= `std::shared_ptr`), `ScopedPtr<T>`, `Lazy<T>`. Prefer these over raw `new`/`delete` for owned objects.

### Threading

`Application::MoveToNewThread()` / `MoveToThread()` push QObjects onto dedicated `QThread`s; a GLib main loop runs in its own GThread for GStreamer. Many subsystems (database, tagreader, collection watcher) live off the main thread and communicate via signals/slots. Be careful with object thread affinity and cross-thread signal connections — this is an active area for bugs.

### Audio engine

`src/engine/` — `EngineBase` is the abstract engine; `GstEngine` (+ `GstEnginePipeline`) is the GStreamer implementation and the only backend. `Player` (`src/core/player.h`, implements `PlayerInterface`) drives the engine and owns playback state/queue logic. Device enumeration is per-platform (`*devicefinder.cpp`: ALSA, PulseAudio, CoreAudio, MMDevice, ASIO, etc.).

### Major subsystem directories (`src/`)

- `core/` — Application, Player, Database (SQLite), TaskManager, network, file/storage utilities, MainWindow.
- `collection/` — music library: backend (SQLite-backed), model, watcher, directory scanning.
- `playlist/`, `queue/`, `playlistparsers/`, `smartplaylists/` — playlist management and persistence.
- `tagreader/` — tag reading/writing (TagLib), runs in a separate process/thread via a request/reply protocol.
- `covermanager/`, `lyrics/` — provider frameworks fetching art/lyrics from many web services (one file per provider).
- `streaming/`, `subsonic/`, `tidal/`, `qobuz/`, `spotify/`, `radios/` — streaming service integrations sharing a common `streaming` base.
- `scrobbler/` — Last.fm / ListenBrainz / Subsonic scrobbling.
- `engine/` — audio output, device finders, fingerprinting (Chromaprint), EBU R128, fast spectrum.
- `device/` — USB/MTP/iPod device support (libmtp, libgpod, GIO, UDisks2).
- `settings/`, `dialogs/`, `widgets/`, `context/`, `analyzer/`, `moodbar/`, `waveform/`, `equalizer/` — UI.
- `mpris2/`, `globalshortcuts/`, `osd/`, `systemtrayicon/`, `discord/` — desktop integration (D-Bus, native notifications, etc.).

### Main window sidebar tabs

The sidebar (`ui_->tabs`) is a `FancyTabWidget` (`src/widgets/fancytabwidget.h`). `AddTab()` wraps every content widget in its own page (`FancyTabData::page()`), and that page is what is inserted into the `QTabWidget`. So `widget(index)`, `currentWidget()`, `indexOf(content)` and `setCurrentWidget(content)` never match the content widget passed to `AddTab()`. Always look tabs up with `IndexOfTab(content)` and compare/set by index (`currentIndex() == IndexOfTab(content)`, `setCurrentIndex(IndexOfTab(content))`). Ignoring this broke returning from the Android playlist editor to the playlist list when tapping the active Playlists tab; `FancyTabWidgetTest.ClickingActiveTabFindsContentWidget` in `tests/src/fancytabwidget_test.cpp` guards against it.

On Android the sidebar is not movable; `FancyTabBar::SetTouchScrolling()` makes a drag scroll the tabs, and a tab is only selected when it is tapped without dragging, on release rather than on press.
`TouchScrollingTest` in the same test file covers this.

### Hierarchical navigation

When adding or changing hierarchical menus or navigation flows anywhere in the app:

- Make the current location clear at every level. Use a breadcrumb or an equivalent platform-appropriate hierarchy indicator when the parent is not otherwise obvious (for example, `Settings > General > Behavior`).
- Back returns exactly one level to the parent; it closes or exits the flow only from its root.
- Show only the active level or page. Hide or remove inactive content so controls from a previous level cannot remain visible or interactive behind the current one.
- Preserve the parent context when returning, including selection or scroll position where practical.
- Keep navigation controls discoverable and usable with the input method and screen size of the target platform.

The Android Settings overview and its category/page navigation are an example of this pattern.

### Android build principles

Apply these UI principles when building or changing hierarchical menus and dropdown-style choices anywhere in the Android app, not just Settings. Follow the shared hierarchical navigation rules above as well.

- Use compact navigation buttons instead of dropdown popups: show the setting title as a wrapping label above the button and only the current value inside it, as the equalizer does (`AndroidWidgets::CreateLabeledValueButton()`). Navigation-only buttons without a value keep their title in the button. Keep simple on/off controls directly on the parent page where appropriate.
- Display current values in bold using the theme's link text color; leave titles and button backgrounds unchanged. Summarize related settings compactly, with full details in their editor. Never include passwords or text-entry credentials in summaries.
- Open choices in a full-page subscreen within the existing navigation hierarchy, not a floating dialog. Keep related choices together on one subscreen.
- Show choices as tappable rows without visible radio circles or bullets. Use the theme's highlight background and matching highlighted-text color for the selected row, retaining the checked state for accessibility rather than imposing a fixed green.
- Keep the parent breadcrumb visible and start the fourth-level title on a separate line. Allow titles, values, and choices to wrap on narrow screens.
- Return to the parent after selecting a single choice. For related choices, remain on the subscreen so each can be edited. Back alone must not select a different choice.
- Restore the parent's selection and scroll position when returning, and keep inactive controls hidden and non-interactive.
- Preserve the flow's save semantics. In Settings, edits remain pending until Apply or OK, and Cancel discards unapplied changes.
- Keep these Android adaptations platform-guarded; do not replace existing Windows or Linux controls.

The Android General > Behavior page is the reference implementation.
`SettingsDialog::AddPage()` applies the shared `AndroidSettingsPage` adapter to other settings pages, retaining their original controls, combo models, and load/save logic.
Reuse this adapter instead of creating parallel Android settings forms.
An editor temporarily reparents its controls; use `SettingsPage::IsActive()` rather than `isVisible()` when guarding asynchronous results for the active settings flow.

### Optional features

Most features are compile-time optional, gated by `HAVE_*` macros generated into `config.h` (from `src/config.h.in`). In `CMakeLists.txt` they are added with the `optional_source(HAVE_X SOURCES ... HEADERS ...)` helper. Platform/feature code must be guarded with the matching `#ifdef HAVE_X` and CMake guard. D-Bus interfaces/adaptors (MPRIS2, notifications, UDisks2, KGlobalAccel) are generated from XML via `qt_add_dbus_interface` / `qt_add_dbus_adaptor`.

### Platform-specific files

Suffix conventions: `*_win.cpp`, `*-x11.cpp`, `*-macos.mm`/`.mm` (Objective-C++), `*qt.cpp` (Qt-portable fallback). Pick the implementation in `CMakeLists.txt` via platform conditionals.
