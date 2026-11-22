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

// Schedules the alarms again after a reboot, an app update, a clock or time zone change, or when exact alarms are allowed again.
// Android clears scheduled alarms on reboot and on these changes alarm clock times would move, so they are recalculated from AlarmStore.
public final class AlarmBootReceiver extends BroadcastReceiver {

  @Override
  public void onReceive(final Context context, final Intent intent) {

    Log.i(AlarmStore.TAG, "Rescheduling alarms after " + intent.getAction());
    AlarmStore.reschedule(context);

  }

}
