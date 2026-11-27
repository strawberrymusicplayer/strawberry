// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMTEXT_ANDROID_H
#define ALARMTEXT_ANDROID_H

#include <QCoreApplication>
#include <QLocale>
#include <QString>

#include "alarm/alarmschedule_android.h"

// Short texts that describe an alarm in the alarm overview and editor.
class AlarmText {
  Q_DECLARE_TR_FUNCTIONS(AlarmText)

 public:
  // "Once", "Daily", "Weekdays", "Weekends", "Weekly (Mon)" or the short day names, starting on the locale's first day of the week.
  static QString Frequency(const int days, const QLocale &locale = QLocale());
  // The music the alarm plays, for example "Random song" or "Playlist: Morning".
  static QString Kind(const Alarm &alarm);

  // Whole minutes until the time, rounded up, so the last minute shows 1 rather than 0; as AlarmCountdown.java.
  static qint64 MinutesLeft(const qint64 now_msec, const qint64 until_msec);
  // The delay until the next wall-clock minute or, if sooner, until the countdown to the time changes.
  static qint64 RefreshDelayMsec(const qint64 now_msec, const qint64 until_msec);
  // "Snoozed until %1, goes off in %2 min"; also passed to the clock screen.
  static QString SnoozeTemplate();
  static QString Snooze(const QString &time, const qint64 minutes);
};

#endif  // ALARMTEXT_ANDROID_H
