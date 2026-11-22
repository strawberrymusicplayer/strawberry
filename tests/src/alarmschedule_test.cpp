// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>

#include "alarm/alarmschedule_android.h"

namespace {

class AlarmScheduleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(zone_.isValid());
  }

  QDateTime Local(const int year, const int month, const int day, const int hour, const int minute) const {
    return QDateTime(QDate(year, month, day), QTime(hour, minute), zone_);
  }

  static QDateTime Utc(const int year, const int month, const int day, const int hour, const int minute) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone::utc());
  }

  static Alarm MakeAlarm(const int hour, const int minute, const int days) {
    Alarm alarm;
    alarm.id = QStringLiteral("test");
    alarm.hour = hour;
    alarm.minute = minute;
    alarm.days = days;
    return alarm;
  }

  const QTimeZone zone_ = QTimeZone("Europe/Amsterdam");
};

TEST_F(AlarmScheduleTest, RepeatingAlarmSkipsToTheNextSelectedDay) {

  // 2026-11-20 is a Friday; weekday alarms ring next on Monday.
  const Alarm alarm = MakeAlarm(7, 0, Alarm::kWeekdays);
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 20, 8, 0), zone_), Local(2026, 11, 23, 7, 0));
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 20, 6, 59), zone_), Local(2026, 11, 20, 7, 0));

}

TEST_F(AlarmScheduleTest, RepeatingAlarmOnOneDayWaitsAWeek) {

  // 2026-11-22 is a Sunday.
  const Alarm alarm = MakeAlarm(9, 30, Alarm::kMonday << 6);
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 22, 9, 30), zone_), Local(2026, 11, 29, 9, 30));

}

TEST_F(AlarmScheduleTest, OneOffAlarmInThePastRingsTomorrow) {

  const Alarm alarm = MakeAlarm(6, 45, 0);
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 22, 15, 40), zone_), Local(2026, 11, 23, 6, 45));
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 22, 6, 44), zone_), Local(2026, 11, 22, 6, 45));
  // At the alarm's time it has rung, so the next time is tomorrow.
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 22, 6, 45), zone_), Local(2026, 11, 23, 6, 45));
  // Across the end of a month and a year.
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Local(2026, 12, 31, 23, 0), zone_), Local(2027, 1, 1, 6, 45));

}

TEST_F(AlarmScheduleTest, DisabledAlarmDoesNotRing) {

  Alarm alarm = MakeAlarm(7, 0, Alarm::kAllDays);
  alarm.enabled = false;
  EXPECT_FALSE(AlarmSchedule::NextTrigger(alarm, Local(2026, 11, 22, 6, 0), zone_).isValid());

}

TEST_F(AlarmScheduleTest, TimeSkippedBySpringForwardRingsAfterTheGap) {

  // On 2026-03-29 clocks go from 02:00 CET to 03:00 CEST, so 02:30 does not exist and rings at 03:30 CEST (01:30 UTC).
  const Alarm alarm = MakeAlarm(2, 30, 0);
  const QDateTime trigger = AlarmSchedule::NextTrigger(alarm, Local(2026, 3, 28, 23, 0), zone_);
  EXPECT_EQ(trigger.toUTC(), Utc(2026, 3, 29, 1, 30));
  EXPECT_EQ(trigger.toTimeZone(zone_).time(), QTime(3, 30));

  // Times outside the gap keep their wall-clock time on both sides of the change.
  EXPECT_EQ(AlarmSchedule::NextTrigger(MakeAlarm(1, 30, 0), Local(2026, 3, 28, 23, 0), zone_).toUTC(), Utc(2026, 3, 29, 0, 30));
  EXPECT_EQ(AlarmSchedule::NextTrigger(MakeAlarm(7, 0, 0), Local(2026, 3, 28, 23, 0), zone_).toUTC(), Utc(2026, 3, 29, 5, 0));

}

TEST_F(AlarmScheduleTest, TimeRepeatedByFallBackRingsOnce) {

  // On 2026-10-25 clocks go from 03:00 CEST back to 02:00 CET, so 02:30 occurs at 00:30 and 01:30 UTC.
  const Alarm alarm = MakeAlarm(2, 30, Alarm::kAllDays);
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Utc(2026, 10, 24, 22, 0), zone_).toUTC(), Utc(2026, 10, 25, 0, 30));
  // After ringing the first time it rings again the next day, not at the repeated 02:30.
  EXPECT_EQ(AlarmSchedule::NextTrigger(alarm, Utc(2026, 10, 25, 0, 31), zone_).toUTC(), Utc(2026, 10, 26, 1, 30));

}

TEST_F(AlarmScheduleTest, NextAlarmIsTheEarliestEnabledAlarm) {

  Alarm late = MakeAlarm(8, 0, 0);
  Alarm early = MakeAlarm(6, 0, 0);
  Alarm disabled = MakeAlarm(5, 0, 0);
  disabled.enabled = false;
  const AlarmList alarms = {late, disabled, early};
  EXPECT_EQ(AlarmSchedule::NextAlarm(alarms, Local(2026, 11, 22, 7, 0), zone_), 0);
  EXPECT_EQ(AlarmSchedule::NextAlarm(alarms, Local(2026, 11, 22, 9, 0), zone_), 2);
  EXPECT_EQ(AlarmSchedule::NextAlarm(AlarmList({disabled}), Local(2026, 11, 22, 9, 0), zone_), -1);

}

TEST_F(AlarmScheduleTest, JsonKeepsAllFieldsAndBoundsValues) {

  Alarm alarm = MakeAlarm(21, 15, Alarm::kWeekend);
  alarm.name = QStringLiteral("Weekend");
  alarm.enabled = false;
  alarm.source = Alarm::Source::Radio;
  alarm.source_name = QStringLiteral("Station");
  alarm.source_url = QStringLiteral("https://example.com/stream.mp3");
  alarm.volume = 55;
  alarm.fade_in_seconds = 60;
  alarm.snooze_minutes = 5;
  alarm.tracks = {QStringLiteral("/data/alarm/1.mp3")};
  alarm.tracks_updated = 1795000000;

  const AlarmList alarms = AlarmSchedule::ListFromJson(AlarmSchedule::ListToJson({alarm}));
  ASSERT_EQ(alarms.count(), 1);
  const Alarm &loaded = alarms.first();
  EXPECT_EQ(loaded.id, alarm.id);
  EXPECT_EQ(loaded.hour, 21);
  EXPECT_EQ(loaded.minute, 15);
  EXPECT_EQ(loaded.days, Alarm::kWeekend);
  EXPECT_EQ(loaded.name, alarm.name);
  EXPECT_FALSE(loaded.enabled);
  EXPECT_EQ(loaded.source, Alarm::Source::Radio);
  EXPECT_EQ(loaded.source_name, alarm.source_name);
  EXPECT_EQ(loaded.source_url, alarm.source_url);
  EXPECT_EQ(loaded.volume, 55);
  EXPECT_EQ(loaded.fade_in_seconds, 60);
  EXPECT_EQ(loaded.snooze_minutes, 5);
  EXPECT_EQ(loaded.tracks, alarm.tracks);
  EXPECT_EQ(loaded.tracks_updated, alarm.tracks_updated);

  const AlarmList invalid = AlarmSchedule::ListFromJson(R"([{"id":"a","hour":30,"minute":-5,"days":255,"volume":150,"snooze":0,"source":"unknown"},{"hour":7}])");
  ASSERT_EQ(invalid.count(), 1);
  EXPECT_EQ(invalid.first().hour, 23);
  EXPECT_EQ(invalid.first().minute, 0);
  EXPECT_EQ(invalid.first().days, Alarm::kAllDays);
  EXPECT_EQ(invalid.first().volume, 100);
  EXPECT_EQ(invalid.first().snooze_minutes, 1);
  EXPECT_EQ(invalid.first().source, Alarm::Source::Random);

}

}  // namespace
