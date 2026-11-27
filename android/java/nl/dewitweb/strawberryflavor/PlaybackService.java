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

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;

// Foreground service that keeps the process running while a song is playing or paused, so Android does not freeze it in the background.
// MediaSessionBridge starts and stops it and owns its notification.
public class PlaybackService extends Service {

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {

    final Notification notification = MediaSessionBridge.buildNotification(this);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      startForeground(MediaSessionBridge.NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
    }
    else {
      startForeground(MediaSessionBridge.NOTIFICATION_ID, notification);
    }
    return START_NOT_STICKY;

  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }

}
