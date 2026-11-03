# Strawberry Android Roadmap

Planned Android UI changes, in priority order.
Each item lists the assessment of feasibility and fit with Android design, and the planned approach.

Status values: Planned, In progress, Done.

## 1. Mute button shows the current state

**Status:** Done

**Problem:** The Android mute button in the player controls always shows the muted icon (`QStyle::SP_MediaVolumeMuted`, a speaker with a red cross), even when playback isn't muted.
This suggests the player is muted when it isn't.

**Goal:** Show a normal speaker icon when not muted and the muted icon when muted.

**Assessment:**
- Feasible, small change.
  `MainWindow::MuteChanged()` already receives every mute state change; on Android it only needs to swap the icon of `action_mute`.
- `QStyle::SP_MediaVolume` is the matching unmuted icon from the same style, so both states look consistent.
- Fits Android design: Material media controls show the current state (`volume_up` / `volume_off`).
- Also update the accessible name ("Mute" / "Unmute") so screen readers announce the action the button performs.
- Initialization order matters: the constructor calls `MuteChanged()` with the initial state before `SetupAndroidLayout()` creates the Android mute button.
  `SetupAndroidLayout()` must therefore initialize the new button from the player's current mute state.

**Approach:** In `MuteChanged()` (Android only), set `SP_MediaVolume` or `SP_MediaVolumeMuted` and the matching accessible name.
In `SetupAndroidLayout()`, initialize the icon and accessible name from the player's current mute state.

**Validation (2026-11-02):** Built, deployed and tested on the Android emulator.
The icon switches correctly in both directions.
Captured PCM from the emulator's own host audio stream while playing test tones: unmuted output measured about -25 dBFS RMS, while muted output measured below -102 dBFS RMS; unmuting restored the original level.
Android AudioFlinger's signal power history retains older non-silent entries during mute, so those entries alone must not be interpreted as current audio output.

## 2. Long press on a song to remove it or clear the playlist

**Status:** Done

**Problem:** On Android, there is no touch way to remove songs from a playlist; desktop uses the Delete key or the context menu.

**Goal:** Long-pressing a song opens an action page with "Remove song", "Clear playlist" and "Cancel".
Both actions can be undone and redone when removing at most 500 songs.

**Behavior:**
- If the pressed song is part of a selection of several songs, "Remove" removes all selected songs (shown as "Remove N songs"); otherwise it removes only the pressed song.
- Removing the song that is playing keeps it playing until it ends, as on desktop, after which playback continues with the next song in the playlist.
- One Undo restores everything that one dialog action removed.

**Assessment:**
- Feasible.
  Long press isn't delivered as a context-menu event to widgets on Android, so `PlaylistView` needs its own detection: start a timer on press, cancel it when the finger moves beyond the touch slop or is released, and open the dialog when it fires.
  Use the system long-press timeout (`ViewConfiguration.getLongPressTimeout()`, 400 ms by default on current Android and adjustable in the accessibility settings) instead of a hardcoded value.
- The press must not trigger the playlist's other touch behavior, set up in `playlistcontainer.ui` and `PlaylistView::ReloadSettings()`:
  - Dragging rows (`dragEnabled`, `DragDrop`): moving slightly during the press must not start a drag once the long press has fired.
  - Inline editing (`SelectedClicked` edit trigger, only enabled when the playlist setting "Edit metadata inline" is on; off by default): releasing on an already selected row must not open the editor.
  - Selection (`ExtendedSelection`): the long press must not change the existing selection, so "Remove N songs" applies to what the user selected.
  - Scrolling and select-and-play.
- Undo and redo are mostly supported already: removing rows (`Playlist::removeRows()`) and clearing (`Playlist::Clear()`) push `PlaylistUndoCommandRemoveItems` onto the playlist's undo stack.
  Two gaps need handling:
  - Existing removal commands merge, including across separate user actions.
    Wrap each action in `QUndoStack::beginMacro()` / `endMacro()` so one Undo restores exactly that action, including a selection with gaps.
  - Removing or clearing more than 500 songs (`kUndoItemLimit`) can't be undone and clears the undo history.
    Confirm before either action and enforce the limit against the total selection, not each contiguous range.
- Fit with Android design: good.
  Long press for item actions is a standard Android gesture.
  Show the song title (or the number of selected songs) and keep the pressed row highlighted, so it is clear what the action applies to.
- Taps on `QMenu` items don't trigger actions on Android (see item 3), so the dialog must use buttons, not a menu.
  Emulator testing also found offset touch coordinates in a small positioned native `QDialog`: tapping Remove could activate Cancel.
  Use a full-screen modal action page at the screen origin, with buttons at least 48 logical pixels high and oversized-removal confirmation on the same page.
- Long press is a hidden gesture; Material advises not making it the only way to reach an action.
  After item 3 removes the toolbar menu, it is the only touch path to Clear playlist; consider a later swipe-to-remove or a selection mode as a discoverable alternative.

**Approach:**
- Long-press handling in `PlaylistView`, which doesn't change any existing touch behavior because Qt's drag, selection collapse and edit trigger only act on move or release, not on press (see `QAbstractItemViewPrivate::extendedSelectionCommand()`):
  1. On press, start the long-press timer and pass the press on to `QTreeView` as before.
     A press on an unselected row selects it immediately; a press on an already selected row leaves the selection unchanged until release.
  2. If the finger moves beyond the touch slop or is released before the timeout, cancel the timer; the press then behaves exactly as it does now (tap, drag-select, drag and drop, double tap to play).
  3. When the timer fires, swallow the remaining move and release events of that press instead of passing them to `QTreeView`, reset the view with `setState(QAbstractItemView::NoState)`, and open the dialog.
     Because the release never reaches `QTreeView`, a multi-selection isn't collapsed to the pressed row, no drag starts and the `SelectedClicked` editor doesn't open.
- Verify on the emulator whether the release is still delivered to the view once the modal dialog is open; resetting the view state covers the case where it isn't.
- Skip long-press detection on the rating column, which rates on press; the column is hidden on Android by default.
- Put the long-press detection in a small helper class that is built on all platforms but only used by `PlaylistView` on Android, so its timing and cancellation can be tested by the desktop test suite (`tests/src/`).
- Cancel pending detection on scrolling, interrupted touches, app deactivation, hiding, or model changes.
  Dismiss the action page if the playlist or selection changes so its actions cannot apply to stale rows.
- Show a full-screen modal `QDialog` with buttons that removes the selection (one undo macro) or calls `Playlist::Clear()` (also one undo macro when undoable).
  Activate and focus the page after opening it so keyboard navigation reaches the modal window.
  Back or Escape dismisses the action page; from an oversized-removal confirmation it returns one level to the action choices without changing the playlist.

**Validation (2026-11-02):**
- Built and packaged the Android app, signed it, and deployed it to the emulator.
- All eight focused long-press helper tests pass, covering the configured timeout, short taps, movement within/beyond touch slop, cancellation, one-shot firing, and release suppression.
- Verified Remove, Clear, Cancel, Undo and Redo with real emulator touch/key input; removing several selected songs is restored by one Undo.
- Verified Back and Escape dismiss the action page, Back from an oversized-removal confirmation returns to the choices without modifying the playlist, and switching away from the app cancels the open page.
- Verified the exact undo boundary: removing 500 songs needs no warning and can be undone; Remove and Clear both warn for 501 songs, and Cancel preserves the playlist.
  Confirmed both Remove and Clear of 501 songs empty the playlist and clear undo history.
- With a fresh three-song playlist using local filesystem URLs, captured the emulator's own audio stream and analyzed power at the fixture tone frequencies.
  The 440 Hz song remained dominant before and immediately after removing its playing row, then the expected next song's 660 Hz tone became dominant after the track boundary.
  Stopped playback and restored the removed song afterward.
- Added playlist-view regression tests for separate undo steps and the total undo limit for scattered selections.
  Both changed test sources pass a syntax check with the Android build toolchain.
  The full native playlist suite could not be run because native CMake configuration is blocked by missing ICU development dependencies; the two playlist-view regression tests remain unexecuted.

**Native follow-up (2026-11-03):** Resolved the native dependency blockers and configured the separate Linux Debug build in `build/native/`.
Both test binaries built successfully; all eight long-press tests and 14 focused playlist removal regressions passed, including both previously unexecuted playlist-view tests.
See `CLAUDE.md` for the native build location, isolated KDSingleApplication dependency, configuration options, and test commands.
This was a focused run, not the full native test suite.
**Confirmation:** Item 2 is complete and confirmed by emulator interaction, captured playback audio, and the 22 focused native tests above.
It has not yet been verified on a physical Android device.

## 3. Remove the wrench button; undo and redo buttons take its place

**Status:** Done

**Problem:** The wrench ("More") button in the playlist toolbar opens a menu with Clear playlist, Undo and Redo.
Tapping an item in this menu closes it without running the action, so these actions are unreachable by touch.

**Goal:**
- Delete the wrench ("More") button and its menu from the playlist view on Android; no menu button remains in the playlist toolbar.
- Show Undo and Redo as icon buttons in the playlist toolbar instead.

**Assessment:**
- Feasible.
  `PlaylistContainer` already creates the `undo` and `redo` tool buttons and their actions from the playlist's `QUndoStack`; Android currently hides them and adds the actions to the menu instead.
  The undo/redo actions are recreated per playlist, which the existing buttons already handle through `setDefaultAction()`.
- Disabled buttons give immediate feedback when there is nothing to undo or redo, which the menu didn't.
- Depends on item 2: Clear playlist is only reachable by touch through this menu (it isn't on the Tools page), and moves to the long-press dialog.
- Fit with Android design: acceptable, with one point of attention.
  The toolbar then has six 48 dp buttons (New, Load, Save, Add, Undo, Redo); Material guidance recommends a few actions in a bar with the rest in an overflow menu.
  The playlist pane is about 336 dp wide on a typical phone, so six buttons fit, but leave little room on narrow screens.
  Verify on a small screen, with a large system font size, and with the largest playlist button icon size from the appearance settings; if they don't fit, wrap the row rather than reintroducing a menu.
- The underlying bug (touch taps on `QMenu` items don't trigger actions) remains and can affect other menus, such as the playlist context menu.
  Investigate it separately.

**Approach:** On Android, stop creating `toolbar_menu_` and the menu button, move `ui_->undo` and `ui_->redo` into the Android button row with a 48 dp minimum size, and remove the actions that were added to the menu in `SetActions()` and `SetViewModel()`.
Use an adaptive grid to wrap the six icon-only buttons when the available toolbar width cannot accommodate one row.
Recalculate the columns on toolbar resize, action replacement, and appearance settings reload.
Keep the existing per-playlist `QUndoStack` actions, tooltips, shortcuts, and enabled states; add stable accessible names for Undo and Redo.
Desktop toolbar behavior is unchanged.

**Validation (2026-11-03):**
- Android APK and native test binaries built successfully; all 22 focused native playlist removal and long-press regression tests passed.
- Deployed to the emulator and confirmed that the wrench/menu button is gone and Undo/Redo are visible.
  Both are disabled for a freshly loaded empty playlist.
- Real touch input verified Remove -> Undo -> Redo and Clear -> Undo -> Redo, restoring the three-track fixture afterward.
  Switching playlists and returning preserves the correct playlist's history and rebinds the buttons.
- At 320 dp screen width, with Android system font scale 2.0 and the maximum playlist icon size of 64 dp, the buttons wrap into two rows and remain visible.
  Touch Undo and Redo also work in this configuration, and the wrapped layout persists after app restart.
  Restored the original display and icon settings afterward.
- At an extreme 274 dp width, the whole application window clips horizontally, including the footer and playlist columns.
  This is a separate whole-window minimum-width limitation; the adaptive toolbar does not remove that limitation.
- Not yet verified on a physical Android device.

## 4. Fix the equalizer view and make the equalizer work

**Status:** Done

**Problem:** The equalizer view (Tools > Equalizer) doesn't display properly on Android, and it is unknown whether the equalizer has any effect on playback.

**Goal:** A usable equalizer page on a phone screen, and an equalizer that audibly changes playback.

**Implemented:**
- Added the missing Android GStreamer `equalizer`, `audiofx`, and `replaygain` plugins.
  The generated registration source now includes all three plugins.
- Replaced the clipped vertical-slider dialog on Android with a full-page, scrollable editor containing horizontal sliders, a breadcrumb, and a Back button.
  Use the main window's usable geometry rather than `WindowFullScreen`: the latter extends the editor beneath the native activity bars and makes the final controls unreachable.
  Slider endpoints are inset from Android's edge-Back gesture region; otherwise dragging from the left endpoint closes the editor.
  Gain labels wrap and use bold theme-colored text.
- Preset choices, saving, and deletion use full-page subscreens rather than popup dialogs.
  Choices retain checked state and the theme's selection colors; preset values use bold theme-colored text.
  Opening the choices scrolls the selected preset into view.
  Preset deletion uses a bin icon, and Custom cannot be deleted because it holds live edits.
- With the user's approval, slider edits are saved automatically in Custom without overwriting named presets.
  Equalizer edits remain live; they do not use the Settings dialog's Apply/Cancel semantics.
- Corrected the preamp gain label to match the engine's existing linear multiplier.
  Its endpoints are mute and approximately +6 dB, not the bands' -24/+12 dB range.
  The engine's actual gain mapping is unchanged.

**Verified:**
- Built, installed, and opened the APK on the ARM64 Android emulator.
- Scrolled to the 16 kHz and balance controls, and reached and selected the final Zero preset.
- Captured the emulator's own audio while playing a generated stereo 600 Hz tone.
  On the final APK after an application restart, changing from Classical to Full Treble reduced RMS from approximately 796 to 329, a 7.66 dB reduction.
  Returning to Classical restored approximately 796 RMS, within 0.01 dB of the baseline, without restarting playback.
- Verified actual stereo balancing with touch drags.
  Full left and full right produced approximately 1,500-1,600 RMS in the selected channel and less than 0.2 RMS in the other, exceeding 40 dB channel separation.
  Returning near the center restored approximately equal left/right output, and the editor stayed open across both endpoint drags.
- At 320 dp width with Android system font scale set to 2.0, the final band and balance controls remain reachable, and the complete preset-choice flow remains touch usable.
  Restored the original display size and font scale afterward.
- Verified visible empty-name validation, named-preset saving, Back cancellation of an unsaved name, and cancellation of preset deletion.
  Custom selection and values survived an application restart, and the saved named preset retained its original values after further edits.
  Confirmed deletion of the temporary named preset and verified that Custom's disabled delete button cannot open the deletion flow.
- Four native equalizer regression tests pass, covering live signals, a complete preset update, preamp labels, and enabled/balance setting persistence.
  The 22 focused playlist/long-press regression tests also pass.

- Emptied only the temporary test playlists and removed both generated device WAV files and the temporary named preset.
  Restored flat Custom gains with equalizer and stereo balancing disabled.
  Verified the restored settings after a fresh launch and installed the final APK without incremental-install dependencies.

**Validation notes and limits:**
- Some early coordinate-driven audio checks were near-silent, so they were not accepted as evidence.
  Restarted the emulator with the user's approval, checked the generated file against its device copy, and repeated the final tests with a fresh fixture and explicit F7/F6 Stop/Play controls.
  The controlled final-APK tests above confirm repeatable playback and the live equalizer effect after application restart.
- ReplayGain linkage and generated registration were checked; tagged-track loudness behavior was not part of this equalizer test.
- Not yet verified on a physical Android device.

## Future items

The following items are initial assessments that need further investigation.

## 5. Explore playing a song outside a playlist

**Status:** To explore

**Question:** Can a song be played directly, for example from the Collection or Files tab, without it being added to a playlist?

**First findings:**
- Strawberry's `Player` always plays from the current playlist: the current track, Next, Previous, repeat and shuffle all come from the playlist.
  Playing a song without any playlist would need a change to that core design and is probably not worth it.
- Existing behavior settings (Settings > Behavior) already cover similar needs: double-clicking a song in the collection can append it, add it to the queue, replace the current playlist (`AddBehaviour::Load`) or open it in a new playlist, and optionally start playing it (`PlayBehaviour`).
- A feasible approach that fits Android is a temporary "Now playing" playlist that songs are played from when tapped outside a playlist, kept separate from the user's own playlists, similar to how Android music apps play a tapped song in the context of its album or list.

**Next steps:** Decide what the user experience should be (what happens with Next and Previous, and whether the user's playlists stay untouched), check how the current settings behave on Android, and then decide between configuring existing behavior and a temporary playlist.

## 6. Create a playlist from a folder or album in one go

**Status:** To investigate

**Goal:** Turn an album from the Collection tab or a folder from the Files tab into a new playlist (named after the album or folder) with one action, and optionally start playing it.
This covers most of what item 5 asks for without changing how the player works.

**First findings:**
- Albums: already supported, through a setting.
  Double-tapping an album or artist in the Collection tab adds all its songs (`AutoExpandingTreeView::ItemDoubleClicked()`), and the double-click behavior settings decide what happens: with "Open in new playlist" (`AddBehaviour::OpenInNew`) it creates a new playlist named after the album (`Song::GetNameForNewPlaylist()`), and the play setting can start playback right away.
  Needs verification on Android: the Behavior settings page, double-tap in the collection tree, and that the collection can be filled (adding a collection directory through the system picker, fixed together with Add file).
- Folders: only partly supported.
  In the Files tab, double-tapping a folder only opens it; adding a whole folder as a new playlist ("Open in new playlist", named after the folder) is only available in the context menu, which isn't reachable by touch (taps on `QMenu` items don't work, see item 3, and long press doesn't open context menus).
  The playlist toolbar's Add folder action (`MainWindow::AddFolder()`) isn't shown on Android and always appends to the current playlist.
- Feasible approach that fits Android: reuse the long-press helper and button dialog of item 2 for folders in the Files tab (and optionally albums in the Collection tab), with "Open in new playlist", "Append to current playlist" and "Replace current playlist".
  The existing `FileViewList` slots already implement these actions, so only the touch trigger and dialog are new.

**Next steps:** Verify the album flow on the emulator with "Open in new playlist" selected, then add the long-press dialog for folders after item 2 is done.

## 7. Rename a playlist by tapping its name in the playlist view

**Status:** Planned

**Goal:** Tapping the playlist name above the playlist view lets the user edit it in place; confirming saves the new name.

**Assessment:**
- Feasible, small to medium change.
  The name is a `QLabel` created in `MainWindow::SetupAndroidLayout()`, and renaming already exists: `PlaylistManager::Rename()` stores the name in the database and emits `PlaylistRenamed`, which already updates this label and the Playlists overview.
- Fits Android design: editing a title in place is a common pattern.
  Prefer inline editing over a dialog: replace the label with a `QLineEdit` (same font, text selected) while editing, which opens the on-screen keyboard and keeps the user in context.
  This also avoids depending on dialog taps, which still need verification (see item 2).
- A plain label doesn't look tappable.
  Add a visual hint, such as a small rename (pencil) icon next to the name, and set an accessible name ("Rename playlist") so the action is discoverable and announced by screen readers.
- Editing behavior:
  - The keyboard's Done/Enter key saves; Back first hides the keyboard, as usual on Android, and leaving the field without confirming cancels the edit.
  - An empty or whitespace-only name is rejected and restores the previous name.
  - The previous name is shown again if the playlist changes while editing (for example when another playlist is opened).
- Renaming isn't part of the playlist's undo stack (the undo stack covers the songs in a playlist, not its name), so it can't be undone with the undo button of item 3; typing the old name back is the way to revert.

**Approach:** Replace the title `QLabel` with a small widget that switches between a tappable name (with rename icon) and a `QLineEdit`, calls `PlaylistManager::Rename()` for the current playlist on confirm, and keeps updating from `CurrentChanged` and `PlaylistRenamed` as the label does now.

## 8. Fix playlist deletion and use bin icons for delete buttons

**Status:** To investigate

**Problem:** The playlist delete button (currently a red cross) is reported not to work on Android.
Delete buttons should consistently use a bin icon rather than a cross.

**Goal:**
- Make the playlist delete button work reliably with touch input.
- Replace its red cross with a bin icon and audit all delete buttons across the app for the same icon convention.
- Keep close, cancel, and clear-search controls distinct: a cross that only closes a view or dismisses input is not a delete button.

**Next steps:**
- Reproduce the playlist deletion failure on the emulator and trace the selected playlist, action connection, and persistence.
  Establish whether the existing action deletes the playlist or only removes it from favorites, and ensure the label and behavior agree.
- Reuse the app's existing themed bin icon where available, keeping tooltips and accessible names explicit about what is deleted.
- Verify deletion, cancellation of any confirmation, selection changes, and persistence after restarting the app.
  Confirm that deleting a playlist does not delete its music files.
- Inventory the remaining delete buttons and replace cross icons with bins while preserving their existing behavior.

## 9. Verify Android changes on a physical device

**Status:** Planned

**Goal:** Confirm the emulator-verified changes on a physical Android phone before considering hardware validation complete.

**Next steps:**
- Verify mute, playlist long-press actions, cancellation, Undo/Redo, and toolbar usability.
- Verify equalizer scrolling, preset selection, saving, deletion, Custom persistence, and one-level Back navigation.
  Check slider endpoint drags with system gesture navigation enabled.
- Confirm the equalizer changes actual audio output, restores the baseline when reverted, and balances left/right channels correctly.
  Record the device, Android version, and audio output route used.
- Check narrow-screen and large-font usability, and persistence after an application restart.
- Record results and any device-specific issues here; emulator results alone do not close this item.
