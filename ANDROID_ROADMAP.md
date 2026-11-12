# Strawberry Android Roadmap

Planned Android changes; the order of work is listed under Current priority order.
Each item lists the assessment of feasibility and fit with Android design, and the planned approach.

Status values: Planned, In progress, Done.

## Current priority order

Work through these items in this order.
Each item is completed one point at a time: fix, test, and commit each point separately.

1. **Item 25:** Add local NAS locations (user-requested priority; real-NAS testing remains open).
2. The remaining open items: 13, 15, 16, 17, 12, 20, 21, 22, 23, 24, 26, 27, 28, 29, and finally 9 (physical-device verification, which needs a device).

## 18. Review the Android changes for coding best practices

**Status:** Done (all 27 points in `ANDROID_CODE_REVIEW.md` resolved)

**Goal:** Review every Android-related change in this fork, compared with upstream Strawberry, against coding best practices and identify where it can be improved.

**Approach:**
- Review the complete diff against the upstream merge base: correctness, duplication, platform guards, ownership and thread affinity, Qt conventions, readability, test coverage, and consistency with the project code style.
- Record the improvement points in `ANDROID_CODE_REVIEW.md` and commit that list before changing any code.
- Resolve the points one at a time; build, test, and commit each point separately, and mark it as done in the list.

## 19. Audit all app functionality for Android compatibility

**Status:** Done (all 22 points in `ANDROID_FEATURE_AUDIT.md` resolved within their documented scope; physical-device verification remains item 9; point 14 disables browser login pending roadmap item 24; point 18 excludes settings from backups without implementing Keystore storage)

**Goal:** Check every feature of the app and determine whether it works on Android as currently implemented.

**Approach:**
- Inventory the features per subsystem (playback, collection, playlists, devices, streaming services, scrobbling, covers, lyrics, settings, desktop integration, and so on).
- For each feature, determine whether it works, is not applicable, or needs changes on Android, using the code and emulator testing.
- Record the points that need changes in `ANDROID_FEATURE_AUDIT.md` and commit that list.
- Resolve the points one at a time; test and commit each point separately, and mark it as done in the list.

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

**Status:** Done (Android emulator verified; physical-device testing remains item 9)

**Question:** Can a song be played directly, for example from the Collection or Files tab, without it being added to a playlist?

**First findings:**
- Strawberry's `Player` always plays from the current playlist: the current track, Next, Previous, repeat and shuffle all come from the playlist.
  Playing a song without any playlist would need a change to that core design and is probably not worth it.
- Existing behavior settings (Settings > Behavior) already cover similar needs: double-clicking a song in the collection can append it, add it to the queue, replace the current playlist (`AddBehaviour::Load`) or open it in a new playlist, and optionally start playing it (`PlayBehaviour`).
- A feasible approach that fits Android is a temporary "Now playing" playlist that songs are played from when tapped outside a playlist, kept separate from the user's own playlists, similar to how Android music apps play a tapped song in the context of its album or list.

**Chosen behavior and implementation:**
- Android double-tapping a Collection song or album, or a file in Files, replaces one reusable "Now playing" playlist and starts playback.
  Only the activated song(s) are included; activating a single song does not add its surrounding album or folder.
  Next/Previous, repeat, and shuffle stay within this selection using the existing player.
- A dedicated internal playlist type identifies the session, not its display name.
  An ordinary saved playlist named "Now playing" is not reused or modified.
  The session is created on demand, reused across activations, and recreated if deleted.
- The playlist identity can persist, but its tracks, last-played position, and undo history are not restored after an application restart.
  Session contents are not saved to the playlist database.
  Late file-loader results from a replaced session are ignored.
- Explicit menu actions for adding music to saved playlists retain their existing behavior.
  Desktop playback behavior and stored preferences are unchanged.
- Android Settings > General > Behavior explains this flow instead of offering the now-inapplicable song double-click destination/playback choices.

**Verification of the final Now playing implementation:**
- Built and deployed the Android APK.
  Files activation produced automatic audible output (approximately 374 RMS); repeated activation retained a single Now playing entry and one track.
- Activated an owned, tagged two-track Collection album: the same Now playing entry contained both tracks and automatically produced approximately 345 RMS.
  Activating one song replaced those tracks with just that song and automatically produced approximately 467 RMS.
- Confirmed the Now playing editor is empty after a force-stop/relaunch, while the entry remains available for reuse.
  Removed the generated device files and test Collection directory and restored the initially empty Collection.
- All 85 native playlist tests pass, including four new tests for session persistence, identity reuse and ordinary-playlist preservation, selected-only navigation, name retention, and stale loader results.
  The manager tests cover reopening a closed favorite session and recreation after deletion.
  A navigation tap closed a disposable emulator test playlist during verification; the user confirmed it could be discarded.

**Initial code investigation (before implementation):**
- Collection double-clicks and Files file double-clicks both emit playlist MIME data with `from_doubleclick_` set.
  `MainWindow::AddToPlaylist()` applies the same Behavior settings to both.
- Existing "Open in new playlist" plus "Always start playing" can protect the contents of existing playlists without modifying the playback engine.
  Each activation creates another ordinary database-backed playlist; this is not a temporary playback session.
  Append and Enqueue modify the current playlist; Replace clears it before inserting songs.
- `Player::NextItem()` obtains the next row and repeat state from the active playlist, confirming that direct engine playback alone would bypass required navigation behavior.
- Android Behavior already exposes the playlist action and playback choices as full-page editors using the existing settings controls.
  This was traced in code, not newly verified with collection/files playback on the emulator.
- Recommendation: verify Open in new playlist + Always first.
  If repeated playlist creation is undesirable, use one separately identified, reusable Now playing playlist rather than a playlist-free engine path.
  Decide whether Next/Previous traverse only the selected songs or the surrounding album/folder, and whether the session survives restarting.
  No playback behavior or user settings were changed during this investigation.

**Earlier Android settings verification (before implementation):**
- Opened Settings > General > Behavior > Double clicking songs.
  Selected Open in new playlist and Always start playing, applied the settings, and reopened the page to confirm they persisted.
- Double-tapped an owned generated WAV in the Files tab.
  It created a separate, one-track playlist named after the file path and started playback without a separate Play action.
  Captured the emulator's own audio immediately after a controlled Files activation: approximately 467 RMS, confirming actual tone output.
  An explicit Stop/Play baseline was approximately 461 RMS.
- Repeated activation produced two separate playlists with the same file-path name, confirming that this setting does not reuse a Now playing session.
  Existing playlists remained listed; the initially empty playlist stayed empty.
- Early coordinate-driven captures were silent or near-silent and were not accepted as audio verification.
  The accepted run included before/after screenshots of the Files activation and immediate emulator-stream capture.
- Restored the original Append and Never start playing settings and confirmed them in the Behavior page.
  Stopped playback, emptied both owned test playlists, and removed the generated device WAV.
  The two empty test playlists remain; they were not deleted.
- Subsequently verified Collection playback using an owned two-track WAV album with ID3 artist/album/title tags.
  Double-tapping the album created a two-track playlist named "Strawberry Verification - Android Test Album" and automatically produced approximately 395 RMS audible output.
  Double-tapping Probe 1 created a separate one-track playlist with the same artist/album-derived name and automatically produced approximately 338 RMS.
  No separate Play action was used for either accepted capture.
- Deleted both owned Collection test playlists using the new bin, removed only the test collection directory and generated audio, and restored Append/Never settings.
  Physical-device testing remains item 9.

## 6. Create a playlist from a folder or album in one go

**Status:** Done (Android emulator verified; physical-device testing remains item 9)

**Goal:** Turn an album from the Collection tab or a folder from the Files tab into a new playlist (named after the album or folder) with one action, and optionally start playing it.

**Implemented:**
- Long-press a folder in either Files view mode, or an album in Collection, to open a touch-friendly action page.
  Choose Create saved playlist without starting playback, Create and play, or Back.
- Folder playlists use the folder name; Collection playlists retain the existing artist/album-derived naming.
  Only the held folder or album is used, not other selected rows.
- Reused the existing long-press detector, MIME data, folder loader, and playlist creation logic.
  Explicit playback choices override menu preferences, and these playlists retain their tracks after restarting.
  Temporary Now playing and other playlists are not replaced.
- The page shows its Files/Collection breadcrumb, hides the underlying controls, and supports Android Back without creating anything.
  Scrolling, dragging, hiding the view, and invalidating the source cancel pending actions.
  Android's touch-cancellation event when opening a native window stops the gesture without dismissing the open action page.
  Desktop controls and behavior remain unchanged.

**Verified:**
- Built, installed, and tested the Android APK using owned two-track tagged WAV fixtures.
  Files list creation produced a named two-track playlist without playback (captured audio RMS 0).
- Files tree Create and play produced a separate named two-track playlist and audible output (approximately 467 RMS).
  Collection album Create and play produced the artist/album-named playlist with both tracks and approximately 467 RMS.
- All three saved playlists restored both tracks after force-stop/relaunch; Now playing remained empty.
  Android Back and touch Back cancel without creating a playlist, and album actions reopen after returning.
- Eleven native action-handler tests cover explicit playback flags, held-item identity, normal taps, movement/scroll cancellation, Back, invalidated items, hiding, and Android touch cancellation.
  Existing playlist, long-press, and Files navigation tests also pass.
- Removed the three owned verification playlists and audio fixtures, restored Files list mode and the initially empty Collection, and verified the original eleven playlist entries remain.
  Source WAV hashes were unchanged before cleanup.

## 7. Rename a playlist by tapping its name in the playlist view

**Status:** Done (Android emulator verified; physical-device verification remains item 9)

**Goal:** Tapping the playlist name above the playlist view lets the user edit it in place; confirming saves the new name.

**Assessment:**
- Feasible, small to medium change.
  The name is a `QLabel` created in `MainWindow::SetupAndroidLayout()`, and renaming already exists: `PlaylistManager::Rename()` stores the name in the database and emits `PlaylistRenamed`, which already updates this label and the Playlists overview.
- Fits Android design: editing a title in place is a common pattern.
  Prefer inline editing over a dialog: replace the label with a `QLineEdit` (same font, text selected) while editing, which opens the on-screen keyboard and keeps the user in context.
  This also avoids depending on dialog taps, which still need verification (see item 2).
- Keep the title free of a rename icon; the title itself is the touch target, with an accessible name and tooltip for assistive technology.
- Editing behavior:
  - The keyboard's Done/Enter key saves; Back first hides the keyboard, as usual on Android, and leaving the field without confirming cancels the edit.
  - An empty or whitespace-only name is rejected and restores the previous name.
  - The previous name is shown again if the playlist changes while editing (for example when another playlist is opened).
- Renaming isn't part of the playlist's undo stack (the undo stack covers the songs in a playlist, not its name), so it can't be undone with the undo button of item 3; typing the old name back is the way to revert.

**Approach:** Replace the title label with a compact Android header containing a left-aligned, tappable playlist name and a Settings-style Back button on the right. Tapping the name switches to a `QLineEdit`; confirming calls `PlaylistManager::Rename()` and the title continues to update from `CurrentChanged` and `PlaylistRenamed`.

**Implemented and verified:**
- The playlist name is left-aligned, bold 18 pt, and opens an inline editor without a rename icon. Enter saves a trimmed non-empty name; an empty name or leaving without confirming cancels.
- A Settings-style Back button sits on the right with a 32 px arrow and 48 px minimum height. It returns to the Playlists overview and retains the selected playlist.
- Android hides the per-playlist tab strip; playlists are opened and switched from the Playlists overview. Desktop tabs are unchanged.
- Removed the Save playlist diskette from the Android overview toolbar. The export action remains available from the existing context menu; desktop controls are unchanged.
- The Android overview uses 14.4 pt playlist names without playlist icons, with compact rows and gray separators across the full list width; folder icons remain.
- The Android app target and APK build successfully. Emulator verification confirmed inline rename updates the title and overview, the tab strip and diskette button are absent, and Back returns to the selected playlist in the overview.

## 8. Fix playlist deletion and use bin icons for delete buttons

**Status:** Done (Android emulator verified; physical-device verification remains item 9)

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

**Implemented and verified:**
- The Android list shows all open playlists, but the old Delete action only unfavorited an open playlist.
  Its favorite-state callback added it back to the list, making the action appear to do nothing.
- With the user's approval, the Android bin now deletes the playlist itself, with confirmation, and removes its list entry through `PlaylistDeleted`.
  Desktop favorite-list behavior is unchanged.
- Added a compact asynchronous confirmation popup with a red cancel cross and green confirmation checkmark side by side, each with a 64 dp touch target.
  Its transparent modal host spans the usable window so Android delivers touch input correctly and blocks underlying controls and shortcuts.
  Nested event loops and compact top-level Android dialogs did not respond reliably to emulator taps and were not accepted as working solutions.
  The question names the selected playlist and explicitly states that music files will not be deleted.
- Deleting the last open playlist creates an empty replacement first.
  The manager now handles closed playlists and avoids emitting deletion twice for non-favorite playlists already removed by Close.
- Verified the bin icon, cancellation, confirmed deletion of an owned empty test playlist, selection moving to another playlist, and absence after application restart.
  Focused native playlist regressions pass.
- Rechecked the final compact popup: red-cross and Android Back cancellation preserve the temporary playlist; green-check confirmation removes it.
  Background navigation taps and the New playlist shortcut are blocked while confirmation is open.
  At 320 dp width the question wraps, both icons remain visible, and touch cancellation works.
  Removed both temporary empty playlists, confirmed their absence after restart, and restored the original emulator display size.
- Verified deleting the generated one-track Collection playlist left both source WAVs present with identical SHA-256 hashes before and after deletion.
**Completion (2026-11-05):**
- Moved last-playlist replacement into `PlaylistManager::Delete()`, so every caller keeps one open playlist without Android-specific setup.
- Added `DeleteHandlesOpenClosedAndLastPlaylists`, covering deletion of open and closed favorites, database removal, one deletion signal per playlist, and replacement of the final open playlist.
- Audited delete icons across the app. Actual delete actions use the bin icon; `list-remove` remains on close, unset, clear, and remove-from-list actions.
- The complete native playlist test binary passes. Physical-device verification remains tracked in item 9.

## 9. Verify Android changes on a physical device

**Status:** Planned

**Goal:** Confirm the emulator-verified changes on a physical Android phone before considering hardware validation complete.

**Next steps:**
- Verify mute, playlist long-press actions, cancellation, Undo/Redo, and toolbar usability.
- Verify folder/album saved-playlist creation with and without playback, both Files view modes, Back cancellation, and track persistence after restart.
- Verify equalizer scrolling, preset selection, saving, deletion, Custom persistence, and one-level Back navigation.
  Check slider endpoint drags with system gesture navigation enabled.
- Confirm the equalizer changes actual audio output, restores the baseline when reverted, and balances left/right channels correctly.
  Record the device, Android version, and audio output route used.
- Check narrow-screen and large-font usability, and persistence after an application restart.
- Record results and any device-specific issues here; emulator results alone do not close this item.

## 10. Make the Android equalizer clearer and more attractive

**Status:** Done (emulator verified)

**Goal:** Provide a compact equalizer screen with clear sections and editable dB values in a wrapping band grid.
Keep this visual update Android-only.

**Implemented:**
- Grouped sound presets, preamp, bass, midrange, treble, and stereo controls into rounded, theme-colored sections.
- Replaced the Android gain sliders with editable bold blue dB boxes between up/down arrow buttons, with black frequency labels underneath.
  Arrows use the app's navigation icons with auto-raised, icon-only buttons like the playback Love control.
  Larger blue numbers and smaller regular-weight black dB units are separate; compact controls allow up to four bands per row, wrapping for narrower widths.
  The focused number has a light-blue rounded box, and selected digits retain their blue text instead of changing to black.
  Positive gains show a plus sign in green, negative gains a minus sign in red, and zero remains blue; selection retains the gain color.
  Band controls wrap into additional rows when needed instead of requiring horizontal scrolling.
  Arrow buttons have 48-pixel touch targets and change gain by approximately 1 dB, rounded to the existing engine steps.
  The boxes display the actual quantized gain; preamp retains its linear multiplier and Muted endpoint.
  Enter commits an edited value without closing the equalizer.
- Reduced section spacing and box padding.
  Section headings and frequency labels remain black even when equalizer controls are disabled, as requested.
- Stereo balance retains its horizontal slider with a block-shaped handle and a distinct disabled appearance.
- Replaced the desktop-style scrollbars with slim, theme-highlight-colored indicators without arrow buttons on the main screen and preset choices, retaining swipe scrolling.
- Replaced the large preset area with a compact selector, disk Save icon, and bin Delete icon.
  The selector opens the existing full-page list with a New option.
  New copies the current curve and asks for a unique name; saving creates it.
  With the user's approval, edits now retain the selected named preset and show an asterisk until the disk button saves over it.
  Edits remain live and are retained as a draft across restarts, with a Custom backup; selecting another preset does not overwrite the previous saved curve.
  Custom remains protected from deletion.
- Pre-amp, Bass, Midrange, Treble, and Stereo balance are foldouts.
  The top Sections selector offers All expanded, All collapsed, and One section at a time.
  The first-time default is All collapsed; the mode and each section's open state are remembered.
- Moved Enable equalizer above the presets, with a larger bold label, a 28-pixel checkbox, and a 56-pixel minimum touch-row height.
- Disabled gain controls provide an "Enable equalizer first" tooltip.
  Android also shows an inline explanation while disabled because touch-only tooltip presentation is unreliable.
- Preset and Collapse behaviour of sections labels sit above their selector buttons; buttons show only the selected value, without trailing navigation markers.
- Matched the Settings header's bold 18-point title and Back button, including its 32-pixel arrow icon and 48-pixel minimum height.
- Preserved edge-Back-safe slider insets, wrapping labels, full-page preset navigation, and existing audio processing/live-save behavior.
  Desktop controls are unchanged.

**Verified:**
- Built and installed the updated Android APK and inspected the active and disabled controls on the emulator.
- At 320 dp width with system font scale configured to 2.0, reached the final 16 kHz and stereo controls, opened preset choices and the Save editor, and returned with the header Back button.
- Dragged stereo balance to both endpoints without closing the editor.
- Rechecked wrapping gain controls at 320 dp width and reached the final controls by dragging the slim scrollbar.
  Confirmed the preset screen uses the same arrow-free scrollbar.
- Verified arrow adjustment, typed negative dB entry, Enter keeping the editor open, and persistence of the edited Custom gain after restarting.
- Verified foldout toggles and exclusive-section mode, New preset creation from the current curve, disk saving to the selected preset, persistence after restarting, and bin deletion of the temporary test preset.
- All six native equalizer regression tests pass, including numeric gain rounding, preamp conversion, and the mute endpoint.
  Formatting/whitespace checks pass.
- Physical-device validation remains tracked in item 9.

## 11. Remove the Collection setting for context-menu file deletion

**Status:** Done

**Goal:** Remove "Enable delete files in the right click context menu" from Settings > General > Collection.

**Implemented and verified:**
- Removed the Collection checkbox and its settings load/save path and constants.
- Removed the CollectionView "Delete from disk" context-menu action and its execution path. A legacy `delete_files` value is no longer read, so it cannot enable the action.
- Kept the separate Playlist delete-files setting and action unchanged.
- Android app target builds successfully; the full native suite passes (54/54).

## 12. Import and export equalizer settings as JSON

**Status:** Planned

**Goal:** Provide a location in Tools > Equalizer to store and retrieve equalizer settings as JSON files, with Import and Export actions.

**Next steps:**
- Add a discoverable import/export area and let the user choose where to save or open JSON files using the Android system document picker.
- Define a versioned JSON format covering named presets, the selected preset and unsaved curve, preamp, band gains, and equalizer/stereo-balance settings.
- Decide whether importing merges with or replaces existing presets, with explicit handling of duplicate names and confirmation before overwriting.
- Validate the complete file before applying changes; report unsupported formats, invalid values, and file-access errors without losing existing settings.
- Verify export/import round trips, cancellation, persistence after restart, and compatibility with Android document-provider URLs.

## 13. Explore using Android's file picker for Files

**Status:** In progress

**Goal:** Investigate replacing the app's desktop-style Files browser with Android's native file picker on Android.

**Initial findings:**
- Android's existing Add file and Add folder actions already use Qt file dialogs that return SAF document URLs; these actions are separate from the Files tab.
- `AndroidStorage::LocalPathForUrl()` resolves external-storage documents, raw Downloads documents, and MediaStore items when they have a local path. It returns an empty path for other content providers.
- The Files tab is based on local filesystem models, and this flow does not persist SAF URI permissions. Replacing the browser is therefore not a drop-in picker change for every document provider.
- Emulator check: Add file opens Android DocumentsUI with Recent, Audio, Images, Videos, and Documents entry points. Cancel returns to the unchanged playlist; no file was selected.

**Next steps:**
- Compare native file and folder selection with the current Files tab, including multiple-file selection and whole-folder playback or saved-playlist creation.
- Test file and folder selection from shared storage, Downloads, and a document provider; check which URIs resolve to local paths and whether access survives an application restart.
- Define how the Files entry launches the picker and returns to the app, preserving cancellation and the distinction between temporary Now playing and saved playlists.
- Verify local and provider-backed storage on the emulator and a physical device; leave desktop browsing unchanged.

## 14. Keep the side menu visible when opening Settings

**Status:** Done (Android emulator verified; narrow-screen and physical-device testing remain)

**Goal:** Keep the Android app's side menu visible and usable when opening Settings.

**Verified:**
- Settings opens inside the existing sidebar tab, leaving the sidebar visible.
- Breadcrumbs and Back navigate one level at a time; root Back returns to the previous sidebar tab.
- Switching tabs keeps pending edits. Apply commits without leaving Settings, and Cancel discards edits and returns to the previous tab.
- The 115% Settings and Tools button text and existing larger icons fit on the tested emulator screen.

**Remaining validation:**
- Verify narrow-screen and physical-device behavior; leave desktop Settings unchanged.

## 15. Clarify the Collection empty-state action

**Status:** Planned

**Problem:** The Android Collection empty state says "Click here to add some music", but tapping it opens Settings rather than an add-music flow.

**Goal:** Make the Collection empty-state action and its text agree, so users know how to add music and the tap leads to the expected place.

**Next steps:**
- Verify the tap behavior from the Collection content area with an empty collection and confirm why it opens Settings.
- Decide whether the action should open the relevant Collection settings, start adding a music folder directly, or use different explanatory text.
- Verify Back returns to Collection and that the chosen flow can add a folder and populate the collection; preserve existing Collection behavior on desktop.

## 16. Verify Smart Playlists on Android

**Status:** Planned

**Goal:** Confirm that Smart Playlists are discoverable and their common workflows are usable on Android.

**Next steps:**
- Verify opening Smart Playlists from the sidebar and returning to the previous context.
- Verify creating, editing, saving, and activating a Smart Playlist, including touch targets and narrow-screen behavior.
- Confirm saved Smart Playlists and their rules persist after an application restart; leave desktop behavior unchanged.

## 17. Make Android checkboxes touch-friendly

**Status:** Planned

**Goal:** Review Android checkbox sizes and touch targets throughout the app, using the controls in Tools > Equalizer as the reference: a 28 x 28 px indicator in a 56 px row.

**Next steps:**
- Inventory Android checkbox controls in Settings, Tools, and other Android pages; identify indicators or rows that are difficult to tap.
- Match the Equalizer checkbox indicator and row sizing where practical, while preserving accessibility state and the existing desktop controls.
- Verify each affected checkbox with touch input on a narrow screen and at larger system font sizes.

## 20. Arrange sidebar tabs from Settings

**Status:** Planned

**Problem:** On Android, dragging the sidebar scrolls it, so tabs can no longer be reordered by dragging as on desktop.

**Goal:** Add a Settings page where users can change the order of the sidebar tabs and possibly hide tabs.

**Next steps:**
- Follow the Android hierarchical navigation rules: a compact button on the parent page opens a full-page editor with move up and move down actions for each tab.
- Store the order in the existing `tab_<name>` keys that `FancyTabWidget::SaveSettings()` and `LoadSettings()` already use, so the order stays compatible with desktop.
- Keep edits pending until Apply or OK, and apply the new order to the sidebar without restarting.

## 21. Remove the navigation/menu bar scroll arrows

**Status:** Planned

**Problem:** The Android sidebar still shows the tab bar's scroll arrow buttons when the tabs overflow, a leftover from before the sidebar could be scrolled by dragging.
The user also requested removing these arrows from the navigation/menu bar when testing on the FP5; this is the same planned change, not a separate duplicate item.

**Goal:** Remove the scroll arrows on Android, so dragging is the only way to scroll the sidebar.

**Next steps:**
- Disable the scroll buttons on Android (`FancyTabWidget` calls `setUsesScrollButtons(true)` for every platform); keep them on desktop.
- Verify in landscape, where the tabs overflow, that the arrows are gone, the full height is used for tabs, and every tab can still be reached by dragging and tapped.
- Check portrait and landscape on both the FP5 and emulator, preserving normal tab taps and avoiding accidental tab selection while scrolling.

## 22. Build and bundle a Spotify GStreamer plugin for Android

**Status:** Planned

**Problem:** Spotify playback requires a GStreamer plugin that is not included in the Android SDK, so Spotify is currently disabled on Android.

**Goal:** Investigate building and bundling an Android-compatible Spotify GStreamer plugin to restore Spotify playback.

**Next steps:**
- Identify the plugin and its dependencies, and verify Android support, licensing, and compatibility with Strawberry's Spotify playback and authentication.
- Cross-compile for the supported Android ABIs using the app's GStreamer toolchain, and package and register the plugin in the APK.
- Re-enable Spotify only once the required plugin is available; preserve existing desktop behavior.
- Verify login, playback, seeking, track transitions, and background playback on Android before marking this item done.

## 23. Investigate Discord Rich Presence support on Android

**Status:** Planned

**Problem:** Strawberry's Discord integration uses the desktop client's local IPC socket, which is not available on Android, so Rich Presence is currently disabled there.

**Goal:** Investigate whether a supported Android integration can publish the current playback activity to Discord.

**Next steps:**
- Check Discord's supported Android APIs, SDK availability, account requirements, and platform policies; do not assume the desktop IPC implementation can be ported.
- Assess authentication, privacy, background execution, and integration with Strawberry's playback lifecycle.
- If a supported approach is feasible, implement it separately from the desktop backend and verify activity updates, pause/stop behavior, and logout on Android.
- Keep the feature disabled until verified; document the limitation if no supported integration is available.

## 24. Restore and verify browser authentication on Android

**Status:** Planned

**Problem:** Browser login for Tidal, Qobuz, Last.fm, ListenBrainz and Genius is disabled on Android until its return flow and background lifecycle are verified.
This is the deferred follow-up to feature audit point 14, not a claim that provider logins have passed.

**Goal:** Restore supported browser-login flows without losing the callback while Android backgrounds or freezes an idle app.

**Next steps:**
- Restore Tidal's Android intent filter and Qt URL handler from commit `510bb5eb4`, checking callback routing, OAuth state validation, and return to the active settings page.
- Test loopback redirects for Qobuz, Last.fm, ListenBrainz and Genius with playback stopped, including a delayed browser return and process recreation; implement a supported lifecycle or redirect solution where needed.
- Verify real provider login and token exchange, denial/cancellation, retry, persistence after restart, refresh and logout using accounts entered directly by the tester.
- Remove the Android browser-login guards and restore the buttons only after the corresponding flows pass; preserve desktop behavior, existing sessions, ListenBrainz's manual user-token option and browserless client-credentials authentication.

## 25. Add NAS network locations directly to the Android collection

**Status:** In progress (local-network implementation built; real-NAS verification remains open)

**Problem:** Before this implementation, collection directories required local filesystem paths.
Users could not add an SMB NAS share directly with its server, share, and credentials; a network document provider in Android's file picker was not sufficient for the existing collection scanner.

**Goal:** Add a NAS music folder from within the app, scan it into the collection, and play its tracks without first copying them to local storage.
The first implementation uses manually entered SMB server/share/folder details and read-only access.
Automatic NAS discovery, built-in VPN connectivity, QuickConnect and other remote-access backends are out of scope.

**Verification still open:** The user will create a real NAS test share later.
Keep real-NAS/Synology verification open even after local protocol tests and Android builds pass.
VPN connectivity cannot currently be tested and is not required for this local-network implementation.
Show a short VPN tip behind an information button; do not build external-access setup into the app.

**Implemented:**
- Settings > General > Collection > Music folders > NAS locations provides full-page server/share/folder/account editors with an asynchronous connection check and explicit errors.
  The information button opens a short VPN tip, including the warning not to expose SMB directly to the Internet.
  Apply/OK saves pending locations; Cancel discards unapplied changes.
- A SHA-256-pinned, statically linked libsmb2 6.3 client provides read-only SMB2/SMB3 access with signing required.
  SMB1 and anonymous configuration are not offered.
  The license texts ship in the APK; build/dependency details are in [ANDROID_BUILD_GUIDE.md](ANDROID_BUILD_GUIDE.md#nas-client-dependency).
- Stable, credential-free virtual paths integrate with the existing collection scanner, QFile cover reads, a seekable TagLib stream, and the Android GStreamer QFile source.
  Desktop mounted-folder behavior and controls remain unchanged; the dependency defaults off on desktop.
- Location details and credentials are atomically encrypted using an Android Keystore AES-GCM key in the no-backup directory, not ordinary settings or playlist URLs.
- After counting files, NAS scans publish newly discovered tracks per completed folder, making progress visible in the collection without waiting for the entire NAS tree.
  Existing-track updates, deletions/unavailability, folder changes and expiry remain deferred until the full scan succeeds.
  Cancellation or a failed NAS operation keeps already published additions but discards those pending changes.
  SMB folders use startup/manual collection scans, not native filesystem notifications.
  After editing an existing server/share/folder, run Full collection scan.
- Android's app-owned title bar shows the app name on the left and centered scan details/progress without a Stop button.
  It replaces the native action bar; idle shows only the app name, and the footer retains non-scan background task reporting without duplicating scans.
  Scan details show the counting phase and found-entry count, followed by processed/total entries and an approximate remaining time after sufficient scanning samples.
  Counting itself has no reliable ETA because the total is not yet known, and NAS metadata requests can make it slow.
  Stop scan is available under Tools > Collection, alongside the scan-start actions, rather than in the title bar to reduce accidental cancellation.
  It works during counting and metadata scanning; cancellation waits for the current operation and leaves already published new tracks intact.
  The stopping state is explicit, and the next manual scan is still allowed.
- NAS directory iterators reuse the file type, size and timestamps already returned by SMB listings, avoiding fresh authentication/stat requests for each listed entry.
  The snapshot lasts only for the active iterator on its thread, is invalidated by NAS location changes, and is not retained between scans.
  Counting also reuses Qt's iterator-provided file information.

**Local validation:**
- Native protocol tests cover authenticated listing/filtering, a complete 2 MiB read, seeking, read-only enforcement, missing files, TagLib metadata, GStreamer WAV discovery, actual collection scanning, wrong passwords, and interrupted/offline connections.
  Folder-model tests cover labelled NAS removal and unchanged local-folder paths.
  These suites and the existing QFile source tests passed three consecutive runs.
- Per-folder publication regressions verify that a folder's new track is in the backend while the scan task is still active and other folders remain.
  Existing-track updates and missing-track removal are deferred until the final commit, without publishing additions twice.
  Disconnect and cancellation cases keep completed-folder additions and leave existing songs, folder state and expiry unchanged.
  All nine NAS tests pass three consecutive runs, and the complete collection-watcher suite still passes.
- The full existing collection-watcher suite passes, including the new offline-NAS regression that retains existing songs without deleting, marking unavailable, or advancing expiry.
- The Android application and APK build successfully with the Java credential vault and static client.
  Emulator checks confirm the VPN information page, hierarchical navigation, masked account entry, explicit connection errors, a successful signed loopback connection, pending/Cancel semantics, and encrypted Apply persistence.
  The loopback WAV appears in the Android collection database; after restarting the app, the saved location authenticates again without re-entering credentials.
  Removing the test folder removes only its collection entries and leaves an encrypted empty location list.
  Hardware Back from the information-only page exposed a Qt scroll-area focus crash; moving focus before detaching its content fixes that return path.
  Repeated hardware Back checks also cover the NAS editor and location list.
  The final signed, non-debuggable release APK is installed on the emulator without clearing existing app data.
  The subsequent per-folder publication update builds and packages successfully but has deliberately not been installed while the user's real-NAS scan is running.
- Native scan-controls tests cover unknown/estimated/finishing states, idempotent and task-scoped cancellation, a 240-pixel-wide layout and a minimum 48-pixel stop button.
  Loopback scan tests also cover cancellation before counting from another thread, cancellation during counting and after a published folder, preservation of existing songs and a successful subsequent scan.
  All three scan-controls tests and eleven NAS tests pass three consecutive runs; the complete collection-watcher suite passes.
  The updated Android APK builds, packages and passes signature verification, without installation.
  The new controls are Android-only; runtime checks on the user's device remain pending until the current scan can be left undisturbed.
- Metadata-request regression: inspecting 128 files without a listing snapshot makes 128 connection negotiations and 128 QUERY_INFO requests; inspecting them through a directory iterator makes two of each (98.4% fewer).
  Tests also verify that a file-size change is seen after the iterator ends, an offline NAS reports a failure rather than cached success, changed location settings bypass old snapshots, and another thread performs its own fresh query.
  All thirteen NAS tests pass three consecutive runs.
  The full collection-watcher suite passes, and the optimized Android release APK builds, packages and passes signature verification without installation.
  This is a local protocol measurement, not a measured total-scan speedup on the user's NAS.
- The signed, non-debuggable release APK containing per-folder publication, cancellation/ETA and metadata reuse is now installed on the emulator, preserving app data.
  Scan status has moved to the app-owned title bar, replacing the native action bar and removing duplicate footer controls.
  Four native title-bar tests pass three consecutive runs, covering counting, percentages/ETA, finishing/stopping, multiple scans, idle/non-scan states, actual 240-pixel width, landscape layout and a 48-pixel minimum Stop touch height.
  The explicit width assertion caught a progress-bar minimum-width constraint; allowing it to shrink fixes the narrow layout.
  Emulator checks confirm title-bar scan/counting feedback in portrait and landscape, a working right-side Stop tap, and return to the idle app-name-only title bar without clearing the saved NAS location.
  A subsequent restart exposed that the window could retain the desktop layout's initial width (575 logical pixels on a 411-pixel screen), placing Stop outside the visible screen; the earlier checks did not establish its visibility.
  Android now resizes to the available screen after the initial show/layout event, when the desktop minimum-size constraints have been removed.
  On-emulator geometry confirms a 411-pixel-wide window with Stop fully inside it, OCR confirms the visible Stop label, and tapping it cancels counting and hides the scan controls.
  A reported shutdown while repeatedly tapping Foo Fighters / One By One / All My Life remains unresolved: the captured crash stack ends in emulator EGL cleanup during process exit, without establishing the original reason for exiting.
  Subsequent emulator attempts play that same NAS track, including repeated taps with and without a collection scan, without reproducing the shutdown; Discord by After Forever also reaches Android's playing state.
  These checks do not confirm audible output, seeking or a complete track transition.
- Android Collection artist, album and song labels now use a minimum 16-point font, retaining larger existing fonts and leaving desktop fonts unchanged.
  The signed release is deployed; emulator OCR measures representative label glyphs at 33 pixels high instead of 24, with rows growing to fit in portrait and landscape.
- Scan cancellation is now offered only through Tools > Collection > Stop scan on Android; the title bar is status-only.
  The Tools action uses task-scoped cancellation, retaining the stopping message and idempotent requests.
  The updated header tests pass, and the signed emulator release verifies stopping/restarting via Tools and that tapping the former title-bar Stop location no longer cancels counting.
  This update has not been installed on the FP5, leaving its active scan undisturbed.

**Still open (not claimed as verified):**
- Real NAS/Synology testing: scanning a representative collection, cover loading, playback/seek/track transitions, responsiveness and connection counts for large collections, cancellation, permission changes, Wi-Fi loss/reconnect, and saved-location restoration on a physical phone.
  The loopback protocol fixture does not establish Synology or SMB3 interoperability.
- Use an external Android VPN client rather than building a VPN into Strawberry.
  Document that OpenVPN requires a reachable, configured VPN server on the NAS or router, a client profile, and routing/firewall access to the NAS.
  An alternative such as Tailscale requires a reachable NAS peer or subnet router.
  Verify VPN disconnect/reconnect behavior and explain that a VPN supplies connectivity, not the SMB collection support implemented by this item.
- Assess remote-access options separately from local SMB support.
  Synology QuickConnect is not a generic SMB/WebDAV tunnel: Synology lists third-party applications and direct SMB, NFS, FTP, and WebDAV file access as unsupported.
  Do not promise that entering a QuickConnect ID will work in Strawberry.
  Prefer a VPN for reaching the NAS's private address; do not recommend exposing SMB directly to the Internet.
  Any dedicated Synology API integration would require a separate supported-API and authentication feasibility assessment, not an assumed QuickConnect workaround.

**Reference:** [Synology: Which packages and services support QuickConnect?](https://kb.synology.com/en-global/DSM/tutorial/Which_services_support_QuickConnect)

## 26. Replace the Collection wrench menu with Android-friendly actions

**Status:** Planned

**Problem:** Collection still has a wrench button that opens a desktop-style menu.
This interaction needs an Android touch-friendly alternative, separate from the playlist wrench removal in item 3.

**Goal:** Keep the Collection actions discoverable and usable on Android without a desktop-style popup.

**Approach:**
- Inventory the existing wrench-menu actions and preserve their behavior and availability.
- Replace the Android popup with a clearly labelled, touch-sized entry opening a full-page action list within the existing navigation hierarchy.
- Keep the current location clear, make Back return one level, and restore the Collection selection and scroll position when returning.
- Keep desktop controls unchanged.
- Test each action, Back navigation, narrow screens and portrait/landscape layouts before marking this item done.

## 27. Make Android screens scroll naturally with a finger

**Status:** Planned

**Problem:** On the FP5, screens show scrollbars but cannot consistently be scrolled by dragging with a finger.
Requiring users to drag a narrow desktop-style scrollbar is confusing on a phone.

**Goal:** Make overflowing Android pages and lists scroll directly with touch, without requiring scrollbar manipulation.

**Approach:**
- Audit scrollable pages, settings, dialogs, album/song lists and nested scroll areas; identify surfaces missing touch scrolling and reuse existing touch-scrolling helpers where appropriate.
- Support natural finger dragging and kinetic scrolling; retain a scroll-position indicator where useful rather than treating scrollbar removal as the fix.
- Distinguish scrolling from taps, long presses, selection, reordering and slider gestures so a drag cannot accidentally activate an action.
- Preserve navigation context and scroll positions, and avoid competing gestures in nested scroll areas.
- Keep desktop mouse, wheel, keyboard and scrollbar behavior unchanged.
- Verify on the FP5 and emulator in portrait and landscape, including long lists, nested pages, gesture cancellation and controls embedded in scrollable content.

## 28. Design and add an alarm clock under Tools

**Status:** Planned (requirements interview and UI design first)

**Goal:** Let users create an alarm clock from Tools on Android.
The feature set and screen design are not yet agreed.

**Required first phase:**
- Start with a dedicated chat to ask which features the user wants; do not assume requirements or begin implementation first.
- Discuss options such as one or multiple alarms, one-off/repeating schedules, music sources, volume/fade-in, snooze, dismiss controls and fallback behavior when music is unavailable.
  These are interview topics, not committed features.
- Clarify expected behavior with the screen locked, the app in the background or closed, after a reboot, and without network access.
- Use the answers to work out the appearance and navigation: Tools entry, alarm overview, create/edit flow and ringing screen, following the shared Android hierarchical navigation rules.
- Present the proposed feature scope, screen layout and interactions in chat and obtain the user's approval before implementation.

**After design approval:**
- Assess Android alarm scheduling, permissions, background execution and audio-focus requirements against the agreed reliability expectations; explain platform limitations explicitly.
- Implement and test the approved scope separately, including physical-device checks on the FP5.

## 29. Investigate network playback to Marantz amplifiers and casting devices

**Status:** Planned (device identification and protocol feasibility first)

**Problem:** The app has no built-in network-output device picker or casting integration.
The user can connect Spotify to their Marantz amplifier and wants a comparable way to select an amplifier from Strawberry.

**Goal:** Explore playing Strawberry's local/NAS music through a network amplifier or casting receiver, with clear device selection and playback controls.
The requested long-term scope includes all discussed output options: Bluetooth, DLNA/UPnP, HEOS, Chromecast, AirPlay and investigation of Spotify Connect.
Do not silently reduce the roadmap to the user's Marantz or one protocol; assess each option separately and deliver feasible integrations in stages.
Requested coverage is not a claim that every protocol can legally or technically carry arbitrary local/NAS music.

**First steps:**
- Ask for the exact Marantz model and identify its supported protocols and how Spotify connects to it.
  Spotify may use Spotify Connect; that does not establish support for arbitrary local/NAS audio or access to the same integration from Strawberry.
- Assess supported options such as DLNA/UPnP renderers, HEOS, Chromecast or AirPlay against the actual device, available APIs, licensing and maintenance requirements.
  Record feasibility, limitations and validation requirements for every requested option, including Spotify Connect's service/content restrictions and Android's existing Bluetooth audio routing.
  Explicitly discuss any unsupported or restricted option with the user rather than promising universal compatibility.
- Agree the implementation order and design a shared device picker, active-output indication and return-to-phone behavior before implementation.
- Determine whether the receiver can fetch the media directly or needs a phone-hosted stream, accounting for NAS authentication, codecs, seeking, transcoding and network reachability.
  Never expose NAS credentials in receiver URLs or logs.
- Assess discovery permissions, background playback, audio focus, volume control, queue ownership and device/network disconnection handling.
- Implement only the agreed feasible scope, preserve existing phone/desktop output, and verify on the FP5 with the actual Marantz, including local/NAS tracks, pause/resume, seeking and track transitions.
