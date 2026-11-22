// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMBRIDGE_ANDROID_H
#define ALARMBRIDGE_ANDROID_H

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

#include "alarmschedule_android.h"

// Calls AlarmBridge.java, which keeps the alarms in the app's SharedPreferences and schedules them with Android's AlarmManager.
// The alarms are written one at a time, so changes made by the Java side, such as switching off a one-off alarm when it rings, are not overwritten.
namespace AlarmBridge {

// Must match the PERMISSION_ constants in AlarmBridge.java.
constexpr int kPermissionNotifications = 1;
constexpr int kPermissionExactAlarms = 2;
constexpr int kPermissionFullScreen = 4;
constexpr int kPermissionChannel = 8;
constexpr int kPermissionOverlay = 16;

// Stores the translated texts that the Java alarm screens show and schedules the stored alarms.
void Init(const QJsonObject &labels);
AlarmList Alarms();
// Returns the stored alarm with the ID, or an alarm with an empty ID.
Alarm FindAlarm(const QString &id);
bool SaveAlarm(const Alarm &alarm);
void DeleteAlarm(const QString &id);
int AutoStopMinutes();
void SetAutoStopMinutes(const int minutes);
// The clock type for Tools > Clock and the alarm screen: 0 for digital, 1 for analog.
int ClockType();
void SetClockType(const int type);
// The screen brightness in percent while Tools > Clock is shown, or 0 for the system brightness.
int ClockBrightness();
void SetClockBrightness(const int percent);
// Returns the next time an alarm rings in milliseconds since the epoch, including snoozed alarms, or -1.
qint64 NextAlarmMsec();
// Returns when the snoozed alarm rings again in milliseconds since the epoch, or 0.
qint64 SnoozeUntilMsec(const QString &id);
// Returns the kPermission bits of the permissions and notification settings that ringing alarms need but that are missing.
int MissingPermissions();
// Shows Android's notification permission dialog if it may still be shown; the other permissions need the system settings.
void RequestRuntimePermissions();
// Asks for the permission, or opens the system settings page where it can be granted.
void RequestPermission(const int permission);
// Opens the ringing screen if an alarm is ringing.
void ShowRingingScreen();
void OpenClock();

}  // namespace AlarmBridge

#endif  // ALARMBRIDGE_ANDROID_H
