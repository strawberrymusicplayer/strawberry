# Android Emulator Testing Guide

Quick reference for building, deploying, and testing the Strawberry Android app on the emulator.

## Prerequisites

- Android emulator running (e.g., Android 16)
- APK built successfully
- `adb` installed and working
- Android SDK build-tools available (e.g., `~/Android/Sdk/build-tools/35.0.0/`)

## Step-by-Step Launch Process

### 1. Build the APK

```bash
cd /media/wouter/3a319dff-96cd-442c-ab47-5eafe7e4efe8/source/repos/strawberry
cmake --build build --parallel 4
```

Build typically takes ~6-8 minutes.

### 2. Sign the APK

The build produces an **unsigned APK** that must be signed before installation.

```bash
cd build/android-build

# Define paths
APK_INPUT="build/outputs/apk/release/android-build-release-unsigned.apk"
APK_ALIGNED="/tmp/strawberry-aligned.apk"
APK_SIGNED="/tmp/strawberry-signed.apk"

# Step 1: Align the APK (required for optimal performance)
~/Android/Sdk/build-tools/35.0.0/zipalign -v 4 "$APK_INPUT" "$APK_ALIGNED"

# Step 2: Sign the APK with debug keystore
~/Android/Sdk/build-tools/35.0.0/apksigner sign \
  --ks ~/.android/debug.keystore \
  --ks-pass pass:android \
  --out "$APK_SIGNED" \
  "$APK_ALIGNED"
```

### 3. Install to Emulator

```bash
adb install -r "$APK_SIGNED"
```

Expected output:
```
Success
Install command complete in [time] ms
```

### 4. Launch the App

The app package name is: **`org.qtproject.example.strawberry`**

The simplest way to launch it is with monkey, which doesn't need the activity name:

```bash
adb shell monkey -p org.qtproject.example.strawberry 1
```

`am start` also works, but only with Qt's activity class (there is no `.MainActivity`):

```bash
adb shell am start -n org.qtproject.example.strawberry/org.qtproject.qt.android.bindings.QtActivity
```

Then wait 20-30 seconds for the app to fully load and render the UI.

### 5. Take Screenshots

Once the app is running:

```bash
# Take a screenshot
adb shell screencap -p /sdcard/screenshot.png

# Pull it to your machine
adb pull /sdcard/screenshot.png /tmp/screenshot.png

# View it
file /tmp/screenshot.png  # Verify it's a real PNG
```

### 6. Interact with the App

#### Open the Tools Tab or Settings

The footer no longer has a menu button.
Tap the **Tools** tab in the left sidebar to reach Cover Manager, Equalizer, Transcoder, and the collection actions.
Tap the **Settings** tab for the settings overview, including the **Help** page with About Strawberry and About Qt.
See [Key Coordinates](#key-coordinates) for where to tap:

```bash
adb shell input tap 90 1740   # Tools
adb shell input tap 90 1915   # Settings
```

#### Take Screenshot of the Tools Page

```bash
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png /tmp/tools-screenshot.png
```

## Complete One-Liner Build → Deploy → Test

```bash
cd /media/wouter/3a319dff-96cd-442c-ab47-5eafe7e4efe8/source/repos/strawberry && \
cmake --build build --parallel 4 && \
cd build/android-build && \
APK_INPUT="build/outputs/apk/release/android-build-release-unsigned.apk" && \
APK_ALIGNED="/tmp/strawberry-aligned.apk" && \
APK_SIGNED="/tmp/strawberry-signed.apk" && \
~/Android/Sdk/build-tools/35.0.0/zipalign -v 4 "$APK_INPUT" "$APK_ALIGNED" && \
~/Android/Sdk/build-tools/35.0.0/apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android --out "$APK_SIGNED" "$APK_ALIGNED" && \
adb install -r "$APK_SIGNED" && \
adb shell monkey -p org.qtproject.example.strawberry 1 && \
sleep 30 && \
adb shell screencap -p /sdcard/screenshot.png && \
adb pull /sdcard/screenshot.png /tmp/strawberry-screenshot.png && \
echo "✅ APK deployed and screenshot saved to /tmp/strawberry-screenshot.png"
```

## Troubleshooting

### APK Installation Fails with Certificate Error

**Problem:** 
```
Failure [INSTALL_PARSE_FAILED_NO_CERTIFICATES: Failed to collect certificates...]
```

**Solution:** The APK is unsigned. Use the signing steps above (zipalign + apksigner).

### App Doesn't Launch with `adb shell am start`

**Problem:**
```
Error: Activity class {org.qtproject.example.strawberry/.MainActivity} does not exist.
```

**Solution:** The app has no `MainActivity`; Qt's activity is `org.qtproject.qt.android.bindings.QtActivity`.
Use `adb shell monkey -p org.qtproject.example.strawberry 1`, or `am start` with the full Qt activity name shown in step 4.
`adb shell cmd package resolve-activity --brief org.qtproject.example.strawberry` prints the launchable activity.

### Blank Screen After Launch

**Problem:** App starts but shows only a blank grey/white screen.

**Solution:** Wait longer. Startup on the emulator can take 20-30 seconds, and the layout may briefly look different (extra sidebar tabs, missing player controls) before it settles.

### Emulator Crashes or Restarts

**Problem:** Emulator becomes unresponsive when launching the app.

**Solution:** 
- Restart the emulator from Android Studio or `emulator -avd <name> &`
- Verify emulator has enough RAM allocated (typically 2-4 GB minimum)
- Check: `adb devices -l` to see if device still appears

## Key Coordinates

Approximate touchscreen coordinates for the Android 16 emulator at 1080x2400 (check with `adb shell wm size`):

- **Previous button:** `62 2200`
- **Play/Pause button:** `192 2200`
- **Stop button:** `310 2200`
- **Next button:** `468 2200`
- **Mute button:** `888 2200`
- **Love button:** `1016 2200`
- **Tools sidebar tab:** `90 1740`
- **Settings sidebar tab:** `90 1915`

Sidebar positions shift when streaming service tabs are enabled.
`adb input tap` uses device pixels; if your image viewer scales screenshots down, scale the measured coordinates back up (for example ×1.2 for a 900 px wide preview).

## Player Footer Buttons

The Android player footer includes:

1. ⏮ Back/Previous
2. ▶ Play/Pause
3. ⏹ Stop
4. ⏭ Forward/Next
5. 🔊 Volume/Mute
6. ❤️ Love/Favorite

## Tools Sidebar Tab

The footer menu has been replaced by a **Tools** sidebar tab, which shows these buttons on one page:

### Tools
- Cover Manager
- Equalizer
- Transcode Music

### Collection
- Update changed collection folders
- Do a full collection rescan
- Stop collection scan

## Settings Help Page

The Settings overview (Settings sidebar tab) has a **Help** button below the categories.
It opens **Settings > Help** with About Strawberry and About Qt; Back returns to the overview.

Add file is in the playlist editor toolbar, and Quit is omitted on Android.

## Performance Notes

- **Build time:** ~6-8 minutes (with `-j 4` parallelism)
- **APK size:** 28 MB
- **Installation time:** ~2-5 seconds
- **App startup time:** 20-30 seconds until the UI has settled
- **Signing + alignment:** ~1 second

## Next Steps for Improvements

- [ ] Document how to add breakpoints and debug via Android Studio
- [ ] Test on a real Android 14+ device (native code is built for API 34)
- [ ] Profile memory usage on low-RAM devices
- [ ] Test with actual music library (add test music files to emulator)
- [ ] Verify all Tools and Help actions work correctly on a real device
