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

**Important:** Do NOT use standard `adb shell am start` - it often fails to find the activity.

**Instead, use monkey to launch:**

```bash
adb shell monkey -p org.qtproject.example.strawberry 1
```

Then wait 3-5 seconds for the app to fully load and render the UI.

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

#### Tap the Menu Button

The menu button is typically at the bottom-right area of the player controls (around coordinate 625, 1438):

```bash
adb shell input tap 625 1438
```

This opens the footer menu showing:
- Playlist items
- Tools items
- Help items
- Open File (added back)
- Quit (added back)

#### Take Screenshot of Menu

```bash
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png /tmp/menu-screenshot.png
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
sleep 5 && \
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

**Solution:** Use `adb shell monkey` instead - it reliably launches the app even when the activity class name can't be resolved directly.

### Blank Screen After Launch

**Problem:** App starts but shows only a blank grey/white screen.

**Solution:** Wait 3-5 seconds longer. The Qt rendering pipeline on Android can be slow to initialize. The UI should appear after this delay.

### Emulator Crashes or Restarts

**Problem:** Emulator becomes unresponsive when launching the app.

**Solution:** 
- Restart the emulator from Android Studio or `emulator -avd <name> &`
- Verify emulator has enough RAM allocated (typically 2-4 GB minimum)
- Check: `adb devices -l` to see if device still appears

## Key Coordinates

These are approximate touchscreen coordinates for the Android 16 emulator at default resolution:

- **Menu button (player footer):** `625 1438` (bottom-right area)
- **Play button:** `120 1438`
- **Pause button:** `120 1438` (same position as play when paused)
- **Stop button:** `175 1438`
- **Next button:** `285 1438`
- **Love button (♥):** `625 1438` (appears after our recent changes)

To find exact coordinates, take a screenshot and note the pixel position of UI elements.

## Player Footer Buttons (After Recent Changes)

Updated Android player footer now includes:

1. ⏮ Back/Previous
2. ▶ Play/Pause
3. ⏹ Stop
4. ⏭ Forward/Next
5. 🔊 Volume/Mute
6. ❤️ Love/Favorite (newly added)
7. ✏️ Edit/Pencil icon
8. ⋮ Menu/Settings (opens popup menu)

## Menu Contents (Footer Popup)

After tapping the menu button, the following items appear:

### Playlist Section
- Add Folder
- Add Stream
- Shuffle Mode
- Repeat Mode
- New Playlist
- Save Playlist
- Load Playlist
- Save All Playlists
- Jump
- Clear Playlist
- Shuffle
- Remove Duplicates
- Remove Unavailable

### Tools Section
- Cover Manager
- Equalizer
- Transcoder
- Update Collection
- Full Collection Scan
- Settings

### Help Section
- About Strawberry
- About Qt

### Music Section (Newly Added)
- Open File
- Quit

## Performance Notes

- **Build time:** ~6-8 minutes (with `-j 4` parallelism)
- **APK size:** 28 MB
- **Installation time:** ~2-5 seconds
- **App startup time:** 3-5 seconds until UI is visible
- **Signing + alignment:** ~1 second

## Next Steps for Improvements

- [ ] Document how to add breakpoints and debug via Android Studio
- [ ] Test on real Android 12+ device (minSdk=31)
- [ ] Profile memory usage on low-RAM devices
- [ ] Test with actual music library (add test music files to emulator)
- [ ] Verify all menu items work correctly on actual emulator
