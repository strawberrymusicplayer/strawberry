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

**Approach:** In `MuteChanged()` (Android only), set `SP_MediaVolume` or `SP_MediaVolumeMuted` and the matching accessible name; set the initial icon where the mute button is created in `MainWindow`.

## 2. Undo and redo buttons replace the wrench menu

**Status:** Planned

**Problem:** The wrench ("More") button in the playlist toolbar opens a menu with Clear playlist, Undo and Redo.
Tapping an item in this menu closes it without running the action, so these actions are unreachable by touch.

**Goal:** Remove the wrench button and show Undo and Redo as icon buttons in the playlist toolbar.

**Assessment:**
- Feasible.
  `PlaylistContainer` already creates the `undo` and `redo` tool buttons and their actions from the playlist's `QUndoStack`; Android currently hides them and adds the actions to the menu instead.
  The undo/redo actions are recreated per playlist, which the existing buttons already handle through `setDefaultAction()`.
- Disabled buttons give immediate feedback when there is nothing to undo or redo, which the menu didn't.
- Fit with Android design: acceptable, with one point of attention.
  The toolbar then has six 48 dp buttons (New, Load, Save, Add, Undo, Redo); Material guidance recommends a few actions in a bar with the rest in an overflow menu.
  The playlist pane is about 336 dp wide on a typical phone, so six buttons fit, but leave little room on narrow screens.
  Verify on a small screen; if they don't fit, wrap the row rather than reintroducing a menu.
- Clear playlist moves from this menu to the long-press dialog of item 3, so it stays reachable.
  Item 2 should therefore ship together with or after item 3.
- The underlying bug (touch taps on `QMenu` items don't trigger actions) remains and can affect other menus, such as the playlist context menu.
  Investigate it separately; item 3 must not rely on a `QMenu`.

**Approach:** On Android, stop creating `toolbar_menu_` and the menu button, move `ui_->undo` and `ui_->redo` into the Android button row with a 48 dp minimum size, and remove the actions that were added to the menu in `SetActions()` and `SetPlaylist()`.

## 3. Long press on a song to remove it or clear the playlist

**Status:** Planned

**Problem:** On Android, there is no touch way to remove a single song from a playlist; desktop uses the Delete key or the context menu.

**Goal:** Long-pressing a song opens a small dialog with "Remove song", "Clear playlist" and "Cancel".
Both actions can be undone and redone.

**Assessment:**
- Feasible.
  Long press isn't delivered as a context-menu event to widgets on Android, so `PlaylistView` needs its own detection: start a timer (about 500 ms, the Android default) on press, cancel it when the finger moves beyond the touch slop or is released, and open the dialog when it fires.
  The press that triggered the dialog must not also start a drag, select-and-play or a scroll.
- Undo and redo are already supported: removing songs (`Playlist::removeRows()`) and clearing (`Playlist::Clear()`) push `PlaylistUndoCommandRemoveItems` onto the playlist's undo stack, so the toolbar buttons of item 2 can undo them.
  Exception: clearing a playlist with more than 500 songs (`kUndoItemLimit`) can't be undone and clears the undo history; the dialog should ask for confirmation in that case.
- Fit with Android design: good.
  Long press for item actions is a standard Android gesture, and a simple dialog with a few actions matches Material dialogs.
  Show the song title in the dialog and highlight the pressed row, so it is clear which song the action applies to.
  Use a dialog with buttons, not a `QMenu` (see item 2).
- Long press is a hidden gesture; Material advises not making it the only way to reach an action.
  The Clear playlist action stays reachable without it only if the toolbar or another screen offers it; consider a later swipe-to-remove or a selection mode as a discoverable alternative.

**Approach:** Add Android-only long-press detection to `PlaylistView` (mouse/touch press, move and release handling with a single-shot timer), and a small modal `QDialog` (or `QMessageBox` with custom buttons) that removes the pressed row through the playlist filter model or calls `Playlist::Clear()`.
Add a test for the long-press timing and cancellation where it can run without a device.
