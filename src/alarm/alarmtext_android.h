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
  // The music the alarm rings with, for example "Random song" or "Playlist: Morning".
  static QString Kind(const Alarm &alarm);
};

#endif  // ALARMTEXT_ANDROID_H
