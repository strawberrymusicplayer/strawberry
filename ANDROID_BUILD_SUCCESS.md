# Android Build Successfully Configured - November 1, 2024

## Summary

After extensive troubleshooting, the Android build pipeline is now **fully working** and produces a signed APK. The main issue was a mismatch between CMake configuration paths and Qt's Android build expectations.

## The Working Build Command

Use this command to build the Android APK:

```bash
# Set up environment
export GSTREAMER_ROOT_ANDROID="/path/to/gstreamer/arm64"
export ANDROID_SDK_ROOT="/path/to/Android/Sdk"

# Configure CMake
cd build
cmake -S .. -B . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/Qt/6.7.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQt6_DIR=/path/to/Qt/6.7.3/android_arm64_v8a/lib/cmake/Qt6 \
  -DQT_HOST_PATH=/path/to/Qt/6.7.3/gcc_64 \
  -DANDROID_SDK_ROOT=/path/to/Android/Sdk \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-34 \
  -DANDROID_STL=c++_shared \
  -DCMAKE_CXX_FLAGS="-Wno-error=old-style-cast" \
  -DANDROID_NDK_ROOT=/path/to/Android/Sdk/ndk/27.2.12479018

# Build (using 4 parallel jobs to reduce CPU usage)
cmake --build . -j 4

# The signed APK will be at:
# build/android-build/build/outputs/apk/release/strawberry-release.apk
```

## Key Requirements

### Android SDK
- **API Level**: android-34 (Android 14)
- **Build Tools**: 36.1.0
- **NDK**: 27.2.12479018
- **minSdk**: 23 (Android 5.8 / API 23)
- **targetSdk**: 34 (Android 14)

The build compiles with API 34 but runs on Android 12+ (minSdk=31 is set by Qt, allowing Android 12 devices).

### CMake Critical Variables
All of these **must** be set correctly:

| Variable | Purpose | Example |
|----------|---------|---------|
| `GSTREAMER_ROOT_ANDROID` | GStreamer SDK root for Android | `/path/to/gstreamer/arm64` |
| `ANDROID_SDK_ROOT` | Android SDK root directory | `/home/user/Android/Sdk` |
| `QT_HOST_PATH` | Qt host (desktop) installation | `/path/to/Qt/6.7.3/gcc_64` |
| `ANDROID_NDK_ROOT` | Android NDK root | `/path/to/Android/Sdk/ndk/27.2.12479018` |
| `CMAKE_TOOLCHAIN_FILE` | Qt Android toolchain | `${QT_ANDROID}/lib/cmake/Qt6/qt.toolchain.cmake` |

### Compiler Flags
- `-DCMAKE_CXX_FLAGS="-Wno-error=old-style-cast"` — Required because GStreamer headers use C-style casts

## Common Build Problems & Solutions

### Problem: "Failed to find Platform SDK with path: platforms;android-37"
**Root Cause**: android-37 is not available in the official Android SDK (max is android-36).
**Solution**: Use android-34 instead (it's stable and well-tested).

### Problem: "Could not locate Android SDK build tools under /opt/android/sdk/build-tools"
**Root Cause**: Qt's toolchain has hardcoded paths from when it was compiled.
**Solution**: Set `-DANDROID_SDK_ROOT` to override the path.

### Problem: "/opt/android/android-ndk-r26b/build/cmake/android.toolchain.cmake does not exist"
**Root Cause**: Qt was compiled with NDK r26b, but you have r27 installed.
**Solution**: Set `-DANDROID_NDK_ROOT` to point to your installed NDK version.

### Problem: "To use a cross-compiled Qt, please set the QT_HOST_PATH"
**Root Cause**: Qt needs the host (desktop) compiler tools.
**Solution**: Set `-DQT_HOST_PATH=/path/to/Qt/6.7.3/gcc_64`.

### Problem: Gradle says "android.jar" is corrupted or resource linking fails
**Root Cause**: The Android SDK platform JAR is malformed or incompatible with the build tools version.
**Solution**: 
1. Remove the platform: `rm -rf /path/to/Android/Sdk/platforms/android-XX`
2. Reinstall via sdkmanager: `yes | sdkmanager "platforms;android-XX"`
3. Use a lower API level (e.g., android-34 instead of android-36)

### Problem: Build is slow or uses all CPU
**Solution**: Use `-j 4` instead of `-j $(nproc)`:
```bash
cmake --build . -j 4
```

## Deployment

### Manual Installation to Emulator
```bash
# Install the signed APK
adb install -r build/android-build/build/outputs/apk/release/strawberry-release.apk

# Launch the app
adb shell am start -n org.qtproject.example.strawberry/.MainActivity
```

### Get Logs
```bash
adb logcat | grep strawberry
```

## Verification Checklist

- [x] CMake configures without errors
- [x] Build completes successfully
- [x] APK is signed and ready to install
- [x] APK installs to emulator without errors
- [x] App launches and runs
- [x] "Add file" button works in playlist editor
- [x] "Add file" NOT in footer settings menu (feature removed in commit d18157bed)

## File Locations (November 1, 2024)

| File | Purpose |
|------|---------|
| `/media/wouter/3a319dff-96cd-442c-ab47-5eafe7e4efe8/source/repos/strawberry/build/` | Build directory |
| `/home/wouter/Android/Sdk/` | Android SDK root |
| `/home/wouter/Qt/6.7.3/android_arm64_v8a/` | Qt Android tools |
| `/media/wouter/3a319dff-96cd-442c-ab47-5eafe7e4efe8/Android/gstreamer-1.28.8/arm64/` | GStreamer Android SDK |

## Future Work

1. **Gradle Plugin Upgrade**: Current Gradle 7.4.1 is tested only up to API 33. Upgrading to 8.0+ would reduce compatibility warnings.
2. **API 35+ Support**: Once officially released, API 35 could be tested.
3. **Build Caching**: Enable `-Dorg.gradle.caching=true` in gradle.properties for faster rebuilds.

## References

- Qt Android Deployment: https://doc.qt.io/qt-6/android-deploy-qt-application.html
- Android SDK Platform Versions: https://developer.android.com/guide/topics/manifest/uses-sdk-element
- GStreamer for Android: https://gstreamer.freedesktop.org/documentation/installing/on-android.html
