// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStringList>

#include "alarm/alarmtext_android.h"

using namespace Qt::Literals::StringLiterals;

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
