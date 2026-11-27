// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStringList>

#include "alarm/alarmtext_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr qint64 kMinuteMsec = 60000;
}  // namespace

QString AlarmText::Frequency(const int days, const QLocale &locale) {

  switch (days & Alarm::kAllDays) {
    case 0:
      return tr("Once");
    case Alarm::kAllDays:
      return tr("Daily");
    case Alarm::kWeekdays:
      return tr("Weekdays");
    case Alarm::kWeekend:
      return tr("Weekends");
    default:
      break;
  }

  const int first = static_cast<int>(locale.firstDayOfWeek());
  QStringList names;
  for (int i = 0; i < 7; ++i) {
    const int day = (first - 1 + i) % 7 + 1;
    if (days & (Alarm::kMonday << (day - 1))) names << locale.standaloneDayName(day, QLocale::ShortFormat);
  }
  if (names.count() == 1) return tr("Weekly (%1)").arg(names.first());
  return names.join(", "_L1);

}

QString AlarmText::Kind(const Alarm &alarm) {

  switch (alarm.source) {
    case Alarm::Source::Sound:
      return tr("Default sound");
    case Alarm::Source::Random:
      return tr("Random song");
    case Alarm::Source::LastPlayed:
      return tr("Last played");
    case Alarm::Source::Playlist:
      return alarm.source_name.isEmpty() ? tr("Playlist") : tr("Playlist: %1").arg(alarm.source_name);
    case Alarm::Source::Song:
      return alarm.source_name.isEmpty() ? tr("Song") : tr("Song: %1").arg(alarm.source_name);
    case Alarm::Source::Radio:
      return alarm.source_name.isEmpty() ? tr("Radio") : tr("Radio: %1").arg(alarm.source_name);
  }
  return QString();

}

qint64 AlarmText::MinutesLeft(const qint64 now_msec, const qint64 until_msec) {
  return until_msec <= now_msec ? 0 : (until_msec - now_msec + kMinuteMsec - 1) / kMinuteMsec;
}

qint64 AlarmText::RefreshDelayMsec(const qint64 now_msec, const qint64 until_msec) {

  qint64 delay = kMinuteMsec - ((now_msec % kMinuteMsec) + kMinuteMsec) % kMinuteMsec;
  if (until_msec > now_msec) {
    const qint64 change = (until_msec - now_msec) % kMinuteMsec;
    delay = qMin(delay, change == 0 ? kMinuteMsec : change);
  }
  return delay;

}

QString AlarmText::SnoozeTemplate() {
  return tr("Snoozed until %1, goes off in %2 min");
}

QString AlarmText::Snooze(const QString &time, const qint64 minutes) {
  return SnoozeTemplate().arg(time, QString::number(minutes));
}
