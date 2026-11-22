// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QLocale>
#include <QString>

#include "alarm/alarmschedule_android.h"
#include "alarm/alarmtext_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// British English starts the week on Monday, American English on Sunday.
const QLocale kBritish(QLocale::English, QLocale::UnitedKingdom);
const QLocale kAmerican(QLocale::English, QLocale::UnitedStates);

Alarm MakeAlarm(const Alarm::Source source, const QString &source_name = QString()) {
  Alarm alarm;
  alarm.source = source;
  alarm.source_name = source_name;
  return alarm;
}

TEST(AlarmTextTest, FrequencyNamesCommonRepeats) {

  EXPECT_EQ(AlarmText::Frequency(0, kBritish), u"Once"_s);
  EXPECT_EQ(AlarmText::Frequency(Alarm::kAllDays, kBritish), u"Daily"_s);
  EXPECT_EQ(AlarmText::Frequency(Alarm::kWeekdays, kBritish), u"Weekdays"_s);
  EXPECT_EQ(AlarmText::Frequency(Alarm::kWeekend, kBritish), u"Weekends"_s);
  // Wednesday only.
  EXPECT_EQ(AlarmText::Frequency(Alarm::kMonday << 2, kBritish), u"Weekly (Wed)"_s);

}

TEST(AlarmTextTest, FrequencyListsOtherDaysFromTheFirstDayOfTheWeek) {

  // Monday, Wednesday, Friday.
  EXPECT_EQ(AlarmText::Frequency(0x15, kBritish), u"Mon, Wed, Fri"_s);
  // Monday and Sunday.
  EXPECT_EQ(AlarmText::Frequency(0x41, kBritish), u"Mon, Sun"_s);
  EXPECT_EQ(AlarmText::Frequency(0x41, kAmerican), u"Sun, Mon"_s);
  // Bits outside the week are ignored.
  EXPECT_EQ(AlarmText::Frequency(0x80 | Alarm::kAllDays, kBritish), u"Daily"_s);

}

TEST(AlarmTextTest, KindNamesTheMusic) {

  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::Random)), u"Random song"_s);
  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::LastPlayed)), u"Last played"_s);
  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::Sound)), u"Default sound"_s);
  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::Playlist, u"Morning"_s)), u"Playlist: Morning"_s);
  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::Playlist)), u"Playlist"_s);
  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::Song, u"Here Comes the Sun"_s)), u"Song: Here Comes the Sun"_s);
  EXPECT_EQ(AlarmText::Kind(MakeAlarm(Alarm::Source::Radio, u"NPO Radio 1"_s)), u"Radio: NPO Radio 1"_s);

}

}  // namespace
