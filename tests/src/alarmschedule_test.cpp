// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QByteArray>

#include "alarm/alarmschedule_android.h"

// When alarms ring is calculated by AlarmSchedule.java and tested by tests/java/AlarmScheduleTest.java.

namespace {

class AlarmScheduleTest : public ::testing::Test {
 protected:
  static Alarm MakeAlarm(const int hour, const int minute, const int days) {
    Alarm alarm;
    alarm.id = QStringLiteral("test");
    alarm.hour = hour;
    alarm.minute = minute;
    alarm.days = days;
    return alarm;
  }
};

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
