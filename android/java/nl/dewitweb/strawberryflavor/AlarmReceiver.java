/*
 * Strawberry Music Player
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package nl.dewitweb.strawberryflavor;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

// Receives the scheduled alarms from AlarmManager and starts ringing; not exported, only AlarmStore's pending intents reach it.
public final class AlarmReceiver extends BroadcastReceiver {

  @Override
  public void onReceive(final Context context, final Intent intent) {

    if (!AlarmStore.ACTION_FIRE.equals(intent.getAction())) return;
    final String id = intent.getStringExtra(AlarmStore.EXTRA_ID);
    if (id == null) return;
    Log.i(AlarmStore.TAG, "Alarm fired" + (intent.getBooleanExtra(AlarmStore.EXTRA_SNOOZE, false) ? " after snoozing" : ""));
    // An alarm clock alarm allows starting a foreground service from the background.
    final Intent ring = new Intent(context, AlarmService.class)
      .setAction(AlarmService.ACTION_RING)
      .putExtra(AlarmStore.EXTRA_ID, id)
      .putExtra(AlarmStore.EXTRA_SNOOZE, intent.getBooleanExtra(AlarmStore.EXTRA_SNOOZE, false));
    try {
      context.startForegroundService(ring);
    }
    catch (RuntimeException e) {
      Log.e(AlarmStore.TAG, "Could not start ringing", e);
    }

  }

}
