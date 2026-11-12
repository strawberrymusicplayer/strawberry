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
NAS folders are rescanned through collection startup/manual scans; native filesystem change notifications are not available for SMB.
The scanner first counts the files, then publishes newly discovered tracks after each completed folder so the collection grows during a large scan.
Changes to existing tracks, removals/unavailability, folder timestamps, and expiry are deferred until the entire scan succeeds.
Cancelling a scan or losing the connection keeps new tracks from completed folders; pending changes to existing collection entries are discarded.
Configure a location in Settings > General > Collection > Music folders > NAS locations.
Enter the server's local name/IP address, share, optional relative folder, and NAS account, then use **Check and use folder**.
The successful check only adds a pending location: Apply/OK saves it, and Cancel discards unapplied changes.
Remove a location using the normal **Remove folder** control.
After editing an existing location's server/share/folder, run **Full collection scan** to refresh its contents; its virtual identifier and existing playlist URLs remain unchanged.
Use a read-only NAS account as an additional server-side safeguard; SMB signing is required, but a locally signed SMB connection is not a promise of transport encryption.
The `(i)` information page explains separately configured VPN access; the app does not set up remote access.

For optional host SMB2 protocol tests, configure the separate native build with `-DENABLE_NAS=ON`, build `nasfileengine_test`, and run it with `STRAWBERRY_NAS_TEST_PYTHON` pointing to an interpreter containing `impacket`.
The fixture binds only to loopback, uses generated files and dummy credentials, and is stopped by the test.
It normalizes Impacket's command-specific error bodies to the SMB2 ERROR format required by MS-SMB2 2.2.2, preserving the original error status and normal packet signing.
Without that interpreter setting, the protocol cases explicitly skip; validation-only cases still run.
This fixture does not replace the real-NAS/Synology test that remains open under roadmap item 25.

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
