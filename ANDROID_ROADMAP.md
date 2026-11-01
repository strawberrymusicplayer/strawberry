# Strawberry Android Roadmap

Planned Android UI changes, in priority order.
Each item lists the assessment of feasibility and fit with Android design, and the planned approach.

Status values: Planned, In progress, Done.

## 1. Mute button shows the current state

**Status:** Planned

**Problem:** The Android mute button in the player controls always shows the muted icon (`QStyle::SP_MediaVolumeMuted`, a speaker with a red cross), even when playback isn't muted.
This suggests the player is muted when it isn't.

**Goal:** Show a normal speaker icon when not muted and the muted icon when muted.

**Assessment:**
- Feasible, small change.
  `MainWindow::MuteChanged()` already receives every mute state change; on Android it only needs to swap the icon of `action_mute`.
- `QStyle::SP_MediaVolume` is the matching unmuted icon from the same style, so both states look consistent.
- Fits Android design: Material media controls show the current state (`volume_up` / `volume_off`).
- Also update the accessible name ("Mute" / "Unmute") so screen readers announce the action the button performs.
- Initialization order matters: the constructor calls `MuteChanged()` with the initial state before `SetupAndroidLayout()` runs, and `SetupAndroidLayout()` then sets the muted icon unconditionally.
  Without changing that, the app would always start with the muted icon.

**Approach:** In `MuteChanged()` (Android only), set `SP_MediaVolume` or `SP_MediaVolumeMuted` and the matching accessible name.
In `SetupAndroidLayout()`, replace the hardcoded muted icon with the icon for the player's current mute state.

## 2. Long press on a song to remove it or clear the playlist

**Status:** Planned

**Problem:** On Android, there is no touch way to remove songs from a playlist; desktop uses the Delete key or the context menu.

**Goal:** Long-pressing a song opens a small dialog with "Remove song", "Clear playlist" and "Cancel".
Both actions can be undone and redone.

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
  - `PlaylistView::RemoveSelected()` calls `removeRows()` once per contiguous range, so removing a selection with gaps creates several undo steps.
    Wrap the removal in `QUndoStack::beginMacro()` / `endMacro()` so it is one step.
  - Removing or clearing more than 500 songs (`kUndoItemLimit`) can't be undone and clears the undo history.
    The dialog should ask for confirmation in that case.
- Fit with Android design: good.
  Long press for item actions is a standard Android gesture, and a simple dialog with a few actions matches Material dialogs.
  Show the song title (or the number of selected songs) in the dialog and keep the pressed row highlighted, so it is clear what the action applies to.
- Taps on `QMenu` items don't trigger actions on Android (see item 3), so the dialog must use buttons, not a menu.
  Verify that taps on `QDialog` / `QMessageBox` buttons work before building on them.
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
- Show a small modal `QDialog` with buttons that removes the selection (one undo macro) or calls `Playlist::Clear()`.

## 3. Remove the wrench button; undo and redo buttons take its place

**Status:** Planned (after item 2)

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

**Approach:** On Android, stop creating `toolbar_menu_` and the menu button, move `ui_->undo` and `ui_->redo` into the Android button row with a 48 dp minimum size, and remove the actions that were added to the menu in `SetActions()` and `SetPlaylist()`.

## Future items

These items are not yet planned in detail; the assessment is a first impression that needs further investigation.

## 4. Fix the equalizer view and make the equalizer work

**Status:** To investigate

**Problem:** The equalizer view (Tools > Equalizer) doesn't display properly on Android, and it is unknown whether the equalizer has any effect on playback.

**Goal:** A usable equalizer page on a phone screen, and an equalizer that audibly changes playback.

**First findings:**
- The equalizer likely doesn't work on Android.
  `GstEnginePipeline` creates the `equalizer-nbands` element from the GStreamer `equalizer` plugin when the equalizer is enabled, but that plugin isn't among the plugins linked into the Android build (`build/gstreamer_android.c`).
- Worse, the pipeline returns an error when the element can't be created, so enabling the equalizer probably stops all playback, not just the equalizer.
  The stereo balancer (`audiopanorama`, from the `audiofx` plugin) and ReplayGain (`rgvolume`, from the `replaygain` plugin) are missing in the same way.
- Fixing this means adding the `equalizer`, `audiofx` and `replaygain` plugins to the Android GStreamer build, or at least hiding or disabling these options when the elements aren't available, so they can't break playback.
- The view itself still needs to be examined on the emulator; the equalizer's vertical band sliders are likely too wide for a phone in portrait orientation.
  It should follow the Android design principles in `CLAUDE.md`, for example a full-page editor instead of a floating dialog.

**Next steps:** Reproduce on the emulator (open the view, enable the equalizer, start playback and check logcat for pipeline errors), then decide between adding the plugins and guarding the options, and redesign the view.

## 5. Explore playing a song outside a playlist

**Status:** To explore

**Question:** Can a song be played directly, for example from the Collection or Files tab, without it being added to a playlist?

**First findings:**
- Strawberry's `Player` always plays from the current playlist: the current track, Next, Previous, repeat and shuffle all come from the playlist.
  Playing a song without any playlist would need a change to that core design and is probably not worth it.
- Existing behavior settings (Settings > Behavior) already cover similar needs: double-clicking a song in the collection can append it, add it to the queue, replace the current playlist (`AddBehaviour::Load`) or open it in a new playlist, and optionally start playing it (`PlayBehaviour`).
- A feasible approach that fits Android is a temporary "Now playing" playlist that songs are played from when tapped outside a playlist, kept separate from the user's own playlists, similar to how Android music apps play a tapped song in the context of its album or list.

**Next steps:** Decide what the user experience should be (what happens with Next and Previous, and whether the user's playlists stay untouched), check how the current settings behave on Android, and then decide between configuring existing behavior and a temporary playlist.
