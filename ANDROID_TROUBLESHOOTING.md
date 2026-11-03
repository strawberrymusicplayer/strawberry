# Strawberry Android Build - Troubleshooting Guide

This document captures real-world issues encountered while setting up Strawberry Android builds and their solutions.

## CMake Configuration Issues

### "Could NOT find Qt6"

**Error Message:**
```
CMake Error at CMakeLists.txt:322 (find_package):
  By not providing "FindQt6.cmake" in CMAKE_MODULE_PATH this project has
  asked CMake to find a package configuration file provided by "Qt6"
```

**Root Cause:** 
Qt6 CMake config files are not in the search path. This happens when using the NDK CMake toolchain directly without Qt's wrapper.

**Solution:**
Use the Qt-provided `qt-cmake` wrapper instead of `cmake`:
```bash
# Wrong:
cmake -DCMAKE_TOOLCHAIN_FILE=...

# Right:
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake -DCMAKE_TOOLCHAIN_FILE=...
```

---

### "Could not locate Android SDK build tools under /opt/android/sdk/build-tools"

**Error Message:**
```
CMake Error at /home/wouter/Qt/6.7.3/android_arm64_v8a/lib/cmake/Qt6Core/Qt6AndroidMacros.cmake:15 (message):
  Could not locate Android SDK build tools under "/opt/android/sdk/build-tools"
```

**Root Cause:**
Qt's CMake files have a hardcoded path `/opt/android/sdk/` but Android SDK is installed at `~/Android/Sdk/`.

**Solution:**
Override the Android SDK root directory using the `-DANDROID_SDK_ROOT` CMake variable:
```bash
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake \
  -DANDROID_SDK_ROOT=~/Android/Sdk \
  ...
```

---

### "To use a cross-compiled Qt, please set QT_HOST_PATH"

**Error Message:**
```
CMake Error at /home/wouter/Qt/6.7.3/android_arm64_v8a/lib/cmake/Qt6/QtPublicDependencyHelpers.cmake:258 (message):
  To use a cross-compiled Qt, please set the QT_HOST_PATH cache variable to
  the location of your host Qt installation.
```

**Root Cause:**
Cross-compiling with Qt requires both the target (Android) and host (Linux) Qt installations. Qt uses the host tools to generate code during the build process.

**Solution:**
Provide the host Qt path:
```bash
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake \
  -DQT_HOST_PATH=~/Qt/6.7.3/gcc_64 \
  ...
```

The `gcc_64` directory contains the Linux x86_64 Qt build with tools like `moc`, `rcc`, and `uic`.

---

### "Checking for module 'glib-2.0' ... Package 'glib-2.0', required by 'virtual:world', not found"

**Error Message:**
```
CMake Error at /usr/share/cmake-3.28/Modules/FindPkgConfig.cmake:619 (message):
  The following required packages were not found:
   - glib-2.0
```

**Root Cause:**
pkg-config (library discovery tool) searches system paths but GStreamer libraries are in a non-standard location. The PKG_CONFIG_PATH environment variable isn't set.

**Solution:**
Set PKG_CONFIG_PATH before running CMake:
```bash
export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64
export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Then run qt-cmake
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake ...
```

---

### "Could NOT find OpenSSL"

**Error Message:**
```
CMake Error at /usr/share/cmake-3.28/Modules/FindOpenSSL.cmake:668 (find_package_handle_standard_args):
  Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the
  system variable OPENSSL_ROOT_DIR
```

**Root Cause:**
OpenSSL CMake finder looks for .so (dynamic libraries), but GStreamer SDK only provides .a (static libraries). Standard CMake OpenSSL finder doesn't find them.

**Solution:**
Explicitly provide the paths to GStreamer's OpenSSL libraries:
```bash
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake \
  -DOPENSSL_ROOT_DIR="${GSTREAMER_ROOT_ANDROID}" \
  -DOPENSSL_INCLUDE_DIR="${GSTREAMER_ROOT_ANDROID}/include" \
  -DOPENSSL_CRYPTO_LIBRARY="${GSTREAMER_ROOT_ANDROID}/lib/libcrypto.a" \
  ...
```

---

### "Package 'libsparsehash', required by 'virtual:world', not found"

**Error Message:**
```
CMake Error at CMakeLists.txt:XXXX:
  Checking for module 'libsparsehash'
  Package 'libsparsehash', required by 'virtual:world', not found
```

**Root Cause:**
libsparsehash is not available in the GStreamer Android SDK and is not critical for Android builds.

**Solution:**
This is not fatal. The build will continue but some optional features (stream tagreader) will be disabled. Check the CMake output for the summary:
```
The following components WILL NOT be built:
   Stream tagreader (missing sparsehash)
```

---

## Build Errors

### "undefined reference to `getgrgid_r`" or other POSIX symbols

**Error Message:**
```
undefined reference to symbol 'getgrgid_r'
ld.lld: error: undefined symbol: getgrgid_r
```

**Root Cause:**
Static libraries (.a files) were compiled with glibc (Linux) instead of bionic (Android). This happens if:
1. You downloaded the wrong GStreamer package (e.g., for Linux instead of Android)
2. The download was incomplete

**Solution:**
1. Verify you're using the correct GStreamer Android SDK:
   ```bash
   file ~/Android/gstreamer-1.28.8/arm64/lib/libglib-2.0.a
   # Should show: "current ar archive" (not "ELF 64-bit")
   
   # Count static libraries
   ls ~/Android/gstreamer-1.28.8/arm64/lib/*.a | wc -l
   # Should be 100+
   ```

2. If there are few files, re-download:
   ```bash
   rm -rf ~/Android/gstreamer-1.28.8
   cd ~/Android/Downloads
   wget "https://gstreamer.freedesktop.org/data/pkg/android/1.28.8/gstreamer-1.0-android-universal-1.28.8.tar.xz"
   
   # Verify download is complete (should be 969 MB)
   ls -lh gstreamer-1.0-android-universal-1.28.8.tar.xz
   
   # Extract
   cd ~/Android
   tar -xf Downloads/gstreamer-1.0-android-universal-1.28.8.tar.xz
   ```

---

### "error: linker command failed with exit code 1"

**Error Message:**
```
clang++: error: linker command failed with exit code 1 (use -v to see invocation)
[100%] Built target ...
ninja: build stopped: subcommand failed.
```

**Root Cause:**
Generic linker error - usually missing libraries or incompatible architectures. Could be:
1. GStreamer SDK architecture mismatch
2. Missing required .a file
3. NDK compatibility issue

**Solution:**

Step 1: Check NDK version:
```bash
cat ~/Android/Sdk/ndk/27.2.12479018/source.properties
# Should show: Pkg.Revision = 27.2.12479018
```

Step 2: Verify GStreamer completeness:
```bash
# Check for essential libraries
ls -1 ~/Android/gstreamer-1.28.8/arm64/lib/libgst*.a | head -20
# Should see: libgstapp-1.0.a, libgstpbutils-1.0.a, libgstreamer-1.0.a, etc.

# Count total
ls ~/Android/gstreamer-1.28.8/arm64/lib/*.a | wc -l
```

Step 3: Check architecture consistency:
```bash
# All should be "current ar archive"
file ~/Android/gstreamer-1.28.8/arm64/lib/libgst*.a | head -5
```

Step 4: Re-run CMake with verbose output:
```bash
cd build
cmake --build . --target strawberry_make_apk --verbose 2>&1 | tail -100
```

---

## Runtime Errors on Emulator

### App Crashes on Startup

**Log Output:**
```
dlopen failed: cannot locate symbol "__system_property_get" referenced by "/data/app/.../libgstreamer_android.so"
```

**Root Cause:**
libgstreamer_android.so is incompatible with the Android version or NDK version used to build it.

**Solution:**
1. Try extracting prebuilt .so from a known working APK:
   ```bash
   adb pull /data/app/org.qtproject.example.strawberry-*/base.apk old.apk
   unzip old.apk "lib/arm64-v8a/libgstreamer_android.so"
   cp lib/arm64-v8a/libgstreamer_android.so ~/Android/gstreamer-1.28.8/arm64/lib/
   ```

2. Rebuild APK with prebuilt .so

---

### "GStreamer library libgstcoreelements not found" (at runtime)

**Error Message:**
```
GStreamer library libgstcoreelements not found
Unable to load plugin '/data/data/org.qtproject.example.strawberry/lib/libgstcoreelements.so'
```

**Root Cause:**
GStreamer plugins aren't being bundled into the APK.

**Solution:**
This is typically handled by the Qt Android deployment system. Check that CMake generated the deployment settings:
```bash
cat build/android-strawberry-deployment-settings.json | grep -i "lib-path"
```

If missing, reconfigure:
```bash
cd build
cmake ..
```

---

### Audio/Video Not Working

**Symptoms:**
- App plays but no sound/video
- No errors in logcat

**Solution:**
1. Check if GStreamer plugins are loaded:
   ```bash
   adb logcat | grep -i gstreamer
   ```

2. Look for missing plugins:
   ```bash
   adb logcat | grep -i "plugin\|element"
   ```

3. Verify GStreamer was initialized:
   ```bash
   adb logcat | grep -E "GStreamer|gstreamer"
   ```

### Crackling or Clicking Audio on the Emulator

**Symptoms:**
- Playback sounds like fast soft clicks instead of a clean tone.

**Cause:** The emulator's audio path to the host, not Strawberry.
The same file sounds identical in another Android player, and `adb shell dumpsys media.audio_flinger` reports no underruns for the Strawberry track.
Test audio quality on a real device.

---

### Widgets Don't Update Until Something Else Repaints

**Symptoms:**
- Rows added to or removed from a playlist only appear after tapping Play, opening a menu or switching tabs.
- Other parts of the window, such as the track count, do update.

**Cause:** Before Qt 6.9.2, `QRhiBackingStore::flush()` scales the flush region by the device pixel ratio twice (QTBUG-128794), so partial updates are uploaded to the wrong part of the window texture.
`src/main.cpp` sets `QT_WIDGETS_RHI=1` on Android for these Qt versions, which flushes through QRhi from the widget repaint manager with the correct coordinates.
Don't remove it unless the Android Qt kit is 6.9.2 or newer.

---

### Tapping a Positioned Dialog Activates the Wrong Button

**Symptoms:**
- A small native `QDialog` is drawn in the expected position, but tapping one button activates another or does nothing.
- Reproduced with playlist song actions on the Qt 6.7.3 Android emulator: tapping Remove could activate Cancel.

**Workaround:**
Use a full-screen modal action page at the screen origin instead of a positioned native popup.
Keep confirmation controls on that same page.
`PlaylistView::ShowAndroidSongActions()` uses this approach, with touch targets at least 48 logical pixels high.
Validate actual touch input; a screenshot alone cannot establish that the hit regions match the displayed buttons.
The action page also explicitly activates and focuses its window after opening; without this, Back and Escape can be ignored even though touch input works.

---

## Deployment Issues

### "adb: command not found"

**Error Message:**
```
bash: adb: command not found
```

**Solution:**
Add Android SDK tools to PATH:
```bash
export PATH=$PATH:~/Android/Sdk/platform-tools

# Add to ~/.bashrc for permanent installation
echo 'export PATH=$PATH:~/Android/Sdk/platform-tools' >> ~/.bashrc
source ~/.bashrc
```

---

### "emulator: command not found"

**Error Message:**
```
bash: emulator: command not found
```

**Solution:**
Add Android emulator to PATH:
```bash
export PATH=$PATH:~/Android/Sdk/emulator

# Or use full path
~/Android/Sdk/emulator/emulator -avd <name>
```

---

### "adb devices" shows "offline" or "unauthorized"

**Solutions:**

If offline:
```bash
# Restart adb daemon
adb kill-server
adb start-server
adb devices
```

If unauthorized:
```bash
# Accept USB debugging authorization on device
# Click "OK" on the authorization dialog that appears
adb devices
```

For emulator:
```bash
# Emulator should reconnect automatically
# If not, restart emulator
emulator -avd <name> &
sleep 10
adb devices
```

---

### "Failed to find Platform SDK with path: platforms;android-37"

**Error Message:**
```
FAILURE: Build failed with an exception.

* What went wrong:
Could not determine the dependencies of task ':lintVitalReportRelease'.
> Failed to find Platform SDK with path: platforms;android-37
```

**Root Cause:**
The Android SDK is missing platform version 37, but `gradle.properties` is configured to use it. This typically happens when:
1. After modifying `build/android-build/gradle.properties`, the file gets regenerated by Qt's `androiddeployqt` tool during the build
2. The `androidCompileSdkVersion` in gradle.properties is set to a version not installed in the Android SDK
3. The gradle.properties and the actual CMake cache are out of sync

**Solution (Multi-step):**

Step 1: Fix gradle.properties with the version you have installed
```bash
# Check what Android platforms are actually installed
ls ~/Android/Sdk/platforms/

# If only android-34 exists, ensure gradle.properties uses it:
sed -i 's/androidCompileSdkVersion=android-.*/androidCompileSdkVersion=android-34/' \
  build/android-build/gradle.properties

# Verify the change
grep "androidCompileSdk" build/android-build/gradle.properties
```

Step 2: Add suppressUnsupportedCompileSdk flag to gradle.properties
```bash
echo "android.suppressUnsupportedCompileSdk=34" >> build/android-build/gradle.properties
```

Step 3: Stop Gradle daemon (it caches old configuration)
```bash
cd build/android-build && ./gradlew --stop && sleep 2
```

Step 4: Clean and rebuild
```bash
cd build
rm -f CMakeCache.txt  # Forces CMake reconfiguration
cmake --build . --parallel $(nproc)
```

**Important Notes:**
- The `gradle.properties` file may be regenerated during the build by Qt's `androiddeployqt` tool
- If the build fails again with the same error, the issue may be that the deployment settings JSON has a hardcoded platform version
- Check `/media/wouter/3a319dff-96cd-442c-ab47-5eafe7e4efe8/source/repos/strawberry/build/android-strawberry-deployment-settings.json` for platform references
- The gradle.properties changes must be made AFTER the initial CMake configuration but are often overwritten during the APK build phase

---

## Environment Setup Issues

### "GSTREAMER_ROOT_ANDROID not set"

**Symptom:**
cmake configure fails to find GStreamer

**Solution:**
```bash
# Set for current session
export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64

# Or add to ~/.bashrc for permanent setup
echo 'export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64' >> ~/.bashrc
echo 'export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"' >> ~/.bashrc
source ~/.bashrc
```

---

### Build takes 30+ minutes or system becomes unresponsive

**Cause:**
Compiling with all CPU cores on limited-RAM systems

**Solution:**
Reduce parallel jobs:
```bash
# Instead of default (usually 8 cores)
cmake --build . --target strawberry_make_apk -j 4

# Or for 2 cores on 8GB RAM systems
cmake --build . --target strawberry_make_apk -j 2
```

---

## Verifying Setup

Run this checklist to ensure everything is correct:

```bash
# Qt Android installation
test -d ~/Qt/6.7.3/android_arm64_v8a && echo "✓ Qt Android" || echo "✗ Qt Android"

# Qt Host tools
test -d ~/Qt/6.7.3/gcc_64 && echo "✓ Qt Host" || echo "✗ Qt Host"

# Android SDK
test -d ~/Android/Sdk && echo "✓ Android SDK" || echo "✗ Android SDK"

# Android NDK
test -d ~/Android/Sdk/ndk/27.2.12479018 && echo "✓ Android NDK" || echo "✗ Android NDK"

# GStreamer SDK
test -d ~/Android/gstreamer-1.28.8/arm64 && echo "✓ GStreamer" || echo "✗ GStreamer"

# GStreamer libraries
test $(ls ~/Android/gstreamer-1.28.8/arm64/lib/*.a 2>/dev/null | wc -l) -gt 50 && echo "✓ GStreamer libs" || echo "✗ GStreamer libs"

# ADB and emulator
which adb > /dev/null && echo "✓ adb" || echo "✗ adb"
which emulator > /dev/null && echo "✓ emulator" || echo "✗ emulator"
```

All checks should show ✓ before attempting to build.

---

## Getting Help

If you encounter an issue not covered here:

1. **Check the build logs:**
   ```bash
   # Last 100 lines of build
   tail -100 build/CMakeOutput.log
   
   # Search for error
   grep -i "error\|failed" build/CMakeOutput.log
   ```

2. **Check emulator logs:**
   ```bash
   adb logcat | grep -E "strawberry|GStreamer|FATAL|ERROR"
   ```

3. **Report on GitHub:**
   - Include CMake version: `cmake --version`
   - Include full error message
   - Include output of verification checklist above
   - Reference the Strawberry issue tracker: https://github.com/strawberrymusicplayer/strawberry/issues

---

**Last Updated:** October 2026
