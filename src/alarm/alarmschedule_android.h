// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMSCHEDULE_ANDROID_H
#define ALARMSCHEDULE_ANDROID_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// An alarm of the Android alarm clock.
// AlarmStore.java keeps the alarms as JSON in the app's SharedPreferences, so it can ring and reschedule them while the Qt app is not running.
// The JSON keys must match AlarmStore.java; AlarmSchedule.java calculates when alarms ring.
struct Alarm {
  // Saved as the strings of SourceName(), so the order may change.
  enum class Source { Sound, Playlist, LastPlayed, Random, Song, Radio };

  // Days of the week, Monday is bit 0; no days means the alarm rings once.
  static constexpr int kMonday = 0x01;
  static constexpr int kWeekdays = 0x1f;
  static constexpr int kWeekend = 0x60;
  static constexpr int kAllDays = 0x7f;

  QString id;
  bool enabled = true;
  int hour = 7;
  int minute = 0;
  QString name;
  int days = 0;
  Source source = Source::Random;
  // The playlist ID, or empty.
  QString source_id;
  // The name of the playlist, song or radio station shown in the overview.
  QString source_name;
  // The song or radio stream URL.
  QString source_url;
  // Alarm volume in percent of the device's maximum alarm volume.
  int volume = 80;
  int fade_in_seconds = 30;
  int snooze_minutes = 10;
  // Local copies of the songs to play, so the alarm rings without the app, network or NAS.
  QStringList tracks;
  // Seconds since the epoch when the tracks were copied, 0 if never.
  qint64 tracks_updated = 0;

  bool repeats() const { return (days & kAllDays) != 0; }
  // Whether the alarm rings on the day of the week, 1 for Monday to 7 for Sunday as QDate::dayOfWeek().
  bool RingsOnDay(int day_of_week) const;

  QJsonObject ToJson() const;
  static Alarm FromJson(const QJsonObject &object);
  static QString SourceName(Source source);
  static Source SourceFromName(const QString &name);
};

using AlarmList = QList<Alarm>;

namespace AlarmSchedule {

AlarmList ListFromJson(const QByteArray &json);
QByteArray ListToJson(const AlarmList &alarms);

}  // namespace AlarmSchedule

#endif  // ALARMSCHEDULE_ANDROID_H
