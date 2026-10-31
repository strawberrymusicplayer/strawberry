# Strawberry Android Build - Documentation Overview

This directory contains comprehensive documentation for building Strawberry music player for Android devices.

## Documentation Files

### 1. **[ANDROID_QUICK_START.md](ANDROID_QUICK_START.md)** - Start here!
**For:** Developers who have already set up the prerequisites and want to build quickly

**Contains:**
- Prerequisites checklist
- 5-minute build commands
- Quick emulator deployment
- Common issues and fixes
- Performance tips

**Read this if:** You've built Android apps before and just need the exact commands

---

### 2. **[ANDROID_BUILD_GUIDE.md](ANDROID_BUILD_GUIDE.md)** - Complete reference
**For:** First-time builders and those setting up the build environment from scratch

**Contains:**
- Complete prerequisites (Qt 6.7.3, NDK 27.2, Android SDK, GStreamer)
- Step-by-step GStreamer Android SDK setup
- Detailed CMake configuration with explanations
- Complete build and deployment process
- Advanced configuration variables
- Complete build script for automation

**Read this if:** This is your first Android build or you need detailed explanations

---

### 3. **[ANDROID_TROUBLESHOOTING.md](ANDROID_TROUBLESHOOTING.md)** - Problem solver
**For:** Debugging build or runtime issues

**Contains:**
- CMake configuration errors and solutions
- Build linker errors and fixes
- Runtime app crashes and debugging
- Deployment and ADB issues
- Environment variable problems
- Verification checklist

**Read this if:** Your build failed or the app crashes at runtime

---

## Quick Decision Tree

```
First time building Strawberry for Android?
├─ No, I've built it before → ANDROID_QUICK_START.md
├─ Yes, completely new setup → ANDROID_BUILD_GUIDE.md (prerequisites first)
└─ Build/runtime error occurred → ANDROID_TROUBLESHOOTING.md
```

## Prerequisites Overview

Before building, you need:

1. **Qt 6.7.3** for Android arm64-v8a
2. **Android NDK 27.2.12479018**
3. **Android SDK** with build-tools
4. **GStreamer 1.28.8** Android SDK
5. **CMake 3.28+** and **Ninja** build system

Full setup instructions: See **ANDROID_BUILD_GUIDE.md** → Prerequisites section

## Architecture

Strawberry for Android is built for **ARM64 (arm64-v8a)** architecture, which covers:
- Most modern Android phones and tablets (2015+)
- Emulators running Android 8+

**Other architectures** (armv7, x86, x86_64) can be built by modifying `ANDROID_ABI` CMake variable. See **ANDROID_BUILD_GUIDE.md** → Advanced section.

## Build Process at a Glance

```
1. Set environment variables (GSTREAMER_ROOT_ANDROID, PKG_CONFIG_PATH)
2. Run qt-cmake to configure build
3. Run cmake --build to compile
4. Deploy APK to device/emulator
5. Test app
```

**Time needed:** 
- First build: 20-30 minutes
- Incremental builds: 2-5 minutes

## Key Files Generated

After a successful build:

| File | Purpose |
|------|---------|
| `build/android-strawberry/build/outputs/apk/debug/strawberry-debug.apk` | Debug APK (for testing) |
| `build/android-strawberry/build/outputs/apk/release/strawberry-release.apk` | Release APK (for distribution) |
| `build/android-strawberry-deployment-settings.json` | Qt deployment configuration |

## Common Tasks

### Build for the first time
```bash
# See: ANDROID_BUILD_GUIDE.md → Building Strawberry for Android
```

### Rebuild after code changes
```bash
cd build
cmake --build . --target strawberry_make_apk
```

### Install on device
```bash
adb install -r build/android-strawberry/build/outputs/apk/debug/strawberry-debug.apk
```

### View app logs
```bash
adb logcat | grep strawberry
```

### Debug GStreamer issues
```bash
adb logcat | grep -i gstreamer
```

## Environment Variables

Set these before running cmake:

```bash
export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64
export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"
```

Add to `~/.bashrc` to make permanent:
```bash
echo 'export GSTREAMER_ROOT_ANDROID=~/Android/gstreamer-1.28.8/arm64' >> ~/.bashrc
echo 'export PKG_CONFIG_PATH="${GSTREAMER_ROOT_ANDROID}/lib/pkgconfig:${PKG_CONFIG_PATH}"' >> ~/.bashrc
source ~/.bashrc
```

## Important CMake Variables

| Variable | Example Value | Purpose |
|----------|---------------|---------|
| `CMAKE_BUILD_TYPE` | `Release` | Build optimization level |
| `ANDROID_ABI` | `arm64-v8a` | Target processor architecture |
| `ANDROID_PLATFORM` | `android-28` | Minimum Android API level |
| `DQT_CHAINLOAD_TOOLCHAIN_FILE` | NDK toolchain path | Android build tools |
| `DQT_HOST_PATH` | `~/Qt/6.7.3/gcc_64` | Host compiler for code generation |
| `DANDROID_SDK_ROOT` | `~/Android/Sdk` | Android SDK location |
| `GSTREAMER_ROOT_ANDROID` | GStreamer arm64 path | Audio/video libraries |

See **ANDROID_BUILD_GUIDE.md** → Configuration Parameters for full explanations.

## Troubleshooting Quick Links

**CMake configuration fails:**
→ See **ANDROID_TROUBLESHOOTING.md** → CMake Configuration Issues

**Linker errors during build:**
→ See **ANDROID_TROUBLESHOOTING.md** → Build Errors

**App crashes at startup:**
→ See **ANDROID_TROUBLESHOOTING.md** → Runtime Errors on Emulator

**APK won't install:**
→ See **ANDROID_TROUBLESHOOTING.md** → Deployment Issues

## Build Optimization

For faster builds on limited systems:

```bash
# Use fewer CPU cores
cmake --build . --target strawberry_make_apk -j 4

# Or just 2 cores for 8GB RAM systems
cmake --build . --target strawberry_make_apk -j 2
```

See **ANDROID_QUICK_START.md** → Performance Tips for more options.

## Contributing to Android Support

If you:
- Fix an issue not in the troubleshooting guide
- Find a better way to configure the build
- Support additional architectures or API levels
- Improve performance or reduce build time

Please consider:
1. Updating the relevant documentation file
2. Adding your fix to the troubleshooting guide
3. Submitting a pull request to the Strawberry repository

## Related Resources

- **Strawberry Repository:** https://github.com/strawberrymusicplayer/strawberry
- **Strawberry Issues:** https://github.com/strawberrymusicplayer/strawberry/issues
- **Qt Android Docs:** https://doc.qt.io/qt-6/android-platform-notes.html
- **Android NDK:** https://developer.android.com/ndk
- **GStreamer:** https://gstreamer.freedesktop.org/

## Version Information

**Last Updated:** October 31, 2026

**Tested Configuration:**
- **OS:** Ubuntu/Kubuntu
- **Qt:** 6.7.3 with android_arm64_v8a variant
- **Android NDK:** 27.2.12479018
- **Android SDK:** API 28-33
- **GStreamer:** 1.28.8 (universal Android package)
- **CMake:** 3.28+
- **Build Tools:** Ninja

**Known Limitations:**
- ARM64 (arm64-v8a) architecture only in these docs
  - armv7, x86, x86_64 can be built by adjusting ANDROID_ABI
- Minimum Android 9 (API 28) - can lower by adjusting ANDROID_PLATFORM
- GStreamer static libraries cannot be dynamically linked to avoid linker issues
  - Pre-built libgstreamer_android.so must be used or rebuilt with same NDK

---

**Start building:** See [ANDROID_QUICK_START.md](ANDROID_QUICK_START.md) or [ANDROID_BUILD_GUIDE.md](ANDROID_BUILD_GUIDE.md)
