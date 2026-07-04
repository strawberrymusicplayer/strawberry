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

### Optional features

Most features are compile-time optional, gated by `HAVE_*` macros generated into `config.h` (from `src/config.h.in`). In `CMakeLists.txt` they are added with the `optional_source(HAVE_X SOURCES ... HEADERS ...)` helper. Platform/feature code must be guarded with the matching `#ifdef HAVE_X` and CMake guard. D-Bus interfaces/adaptors (MPRIS2, notifications, UDisks2, KGlobalAccel) are generated from XML via `qt_add_dbus_interface` / `qt_add_dbus_adaptor`.

### Platform-specific files

Suffix conventions: `*_win.cpp`, `*-x11.cpp`, `*-macos.mm`/`.mm` (Objective-C++), `*qt.cpp` (Qt-portable fallback). Pick the implementation in `CMakeLists.txt` via platform conditionals.
