# Strawberry Android Build - Quick Start

For the impatient: minimum steps to build and deploy Strawberry for Android.

## Prerequisites Checklist

```bash
# Verify all required components are installed
ls ~/Qt/6.7.3/android_arm64_v8a/               # Qt Android
ls ~/Qt/6.7.3/gcc_64/                          # Qt Host tools
ls ~/Android/Sdk/ndk/27.2.12479018/            # Android NDK
ls ~/Android/Sdk/build-tools/                  # Android build-tools
ls ~/Android/gstreamer-1.28.8/arm64/lib/       # GStreamer SDK
```

If any of these fail, see `ANDROID_BUILD_GUIDE.md` for setup instructions.

## 5-Minute Build

```bash
cd ~/source/repos/strawberry

# Set environment
export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64
export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Clean and configure
rm -rf build
~/Qt/6.7.3/android_arm64_v8a/bin/qt-cmake \
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

# Build (10-30 minutes depending on CPU)
cd build
cmake --build . --target strawberry_make_apk --parallel $(nproc)
```

## Deploy to Emulator

```bash
# Start emulator
emulator -avd <name> &

# The build produces an unsigned release APK; sign it first (zipalign + apksigner, see ANDROID_EMULATOR_TESTING.md step 2)
# Wait for boot, then install the signed APK
adb install -r /tmp/strawberry-signed.apk

# Launch
adb shell monkey -p nl.dewitweb.strawberryflavor 1
```

## Common Issues

### CMake can't find GStreamer
```bash
# Did you set the environment variable?
echo $GSTREAMER_ROOT_ANDROID
# Should show: /home/username/Android/gstreamer-1.28.8/arm64

# If empty, run:
export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64
```

### Linker errors with undefined symbols
```bash
# Wrong GStreamer download?
ls ~/Android/gstreamer-1.28.8/arm64/lib/libgst*.a | wc -l
# Should be 100+

# Download again if less than 50 files
# See ANDROID_BUILD_GUIDE.md section "Download GStreamer Android SDK"
```

### ⚠️ "Failed to find Platform SDK with path: platforms;android-XX"

**THIS IS A TRAP - READ CAREFULLY**

```bash
# Symptom: Build fails with:
# > Failed to find Platform SDK with path: platforms;android-37

# Root cause: gradle.properties is set to a platform version that's not installed,
# and the Gradle daemon is caching old configuration

# Solution:
cd build

# Step 1: Stop Gradle daemon (CRITICAL - clears cached config)
./android-build/gradlew --stop

# Step 2: Wait a moment
sleep 2

# Step 3: Check what platforms you actually have
ls ~/Android/Sdk/platforms/
# If only android-34 is shown, use that version

# Step 4: Fix gradle.properties to match installed version
sed -i 's/androidCompileSdkVersion=android-.*/androidCompileSdkVersion=android-34/' \
  android-build/gradle.properties

# Step 5: Add suppress flag
echo "android.suppressUnsupportedCompileSdk=34" >> android-build/gradle.properties

# Step 6: Rebuild
cd ..
cmake --build build --parallel $(nproc)
```

**Remember:** Always stop Gradle daemon with `./gradlew --stop` before rebuilds!

### APK not found after build
```bash
# Find it
find build -name "*.apk" -type f

# Usually at (unsigned):
# build/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

## Useful Commands

```bash
# View last 50 lines of build output
cmake --build build --target strawberry_make_apk 2>&1 | tail -50

# Watch app logs live
adb logcat | grep strawberry

# Check app version on device
adb shell pm dump nl.dewitweb.strawberryflavor | grep version

# Uninstall app from device
adb uninstall nl.dewitweb.strawberryflavor
```

## Performance Tips

```bash
# Faster rebuilds after code changes
cd build
cmake --build . --target strawberry_make_apk

# Use fewer cores if computer lags
cmake --build . --target strawberry_make_apk -j 4

# Monitor build progress
watch -n 5 'ps aux | grep cmake'
```

---

For detailed information, see `ANDROID_BUILD_GUIDE.md`
