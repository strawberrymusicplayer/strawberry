// SPDX-License-Identifier: GPL-3.0-or-later

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include "alarmschedule_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr int kMaximumVolume = 100;
constexpr int kMaximumFadeInSeconds = 600;
constexpr int kMaximumSnoozeMinutes = 60;

}  // namespace

bool Alarm::RingsOnDay(const int day_of_week) const {

  return day_of_week >= 1 && day_of_week <= 7 && (days & (kMonday << (day_of_week - 1))) != 0;

}

QString Alarm::SourceName(const Source source) {

  switch (source) {
    case Source::Sound:
      return u"sound"_s;
    case Source::Playlist:
      return u"playlist"_s;
    case Source::LastPlayed:
      return u"last_played"_s;
    case Source::Random:
      return u"random"_s;
    case Source::Song:
      return u"song"_s;
    case Source::Radio:
      return u"radio"_s;
  }
  return u"random"_s;

}

Alarm::Source Alarm::SourceFromName(const QString &name) {

  for (const Source source : {Source::Sound, Source::Playlist, Source::LastPlayed, Source::Random, Source::Song, Source::Radio}) {
    if (SourceName(source) == name) return source;
  }
  return Source::Random;

}

QJsonObject Alarm::ToJson() const {

  QJsonObject object;
  object["id"_L1] = id;
  object["enabled"_L1] = enabled;
  object["hour"_L1] = hour;
  object["minute"_L1] = minute;
  object["name"_L1] = name;
  object["days"_L1] = days & kAllDays;
  object["source"_L1] = SourceName(source);
  object["source_id"_L1] = source_id;
  object["source_name"_L1] = source_name;
  object["source_url"_L1] = source_url;
  object["volume"_L1] = volume;
  object["fade_in"_L1] = fade_in_seconds;
  object["snooze"_L1] = snooze_minutes;
  object["tracks"_L1] = QJsonArray::fromStringList(tracks);
  object["tracks_updated"_L1] = tracks_updated;
  return object;

}

Alarm Alarm::FromJson(const QJsonObject &object) {

  Alarm alarm;
  alarm.id = object["id"_L1].toString();
  alarm.enabled = object["enabled"_L1].toBool(true);
  alarm.hour = qBound(0, object["hour"_L1].toInt(alarm.hour), 23);
  alarm.minute = qBound(0, object["minute"_L1].toInt(alarm.minute), 59);
  alarm.name = object["name"_L1].toString();
  alarm.days = object["days"_L1].toInt() & kAllDays;
  alarm.source = SourceFromName(object["source"_L1].toString());
  alarm.source_id = object["source_id"_L1].toString();
  alarm.source_name = object["source_name"_L1].toString();
  alarm.source_url = object["source_url"_L1].toString();
  alarm.volume = qBound(0, object["volume"_L1].toInt(alarm.volume), kMaximumVolume);
  alarm.fade_in_seconds = qBound(0, object["fade_in"_L1].toInt(alarm.fade_in_seconds), kMaximumFadeInSeconds);
  alarm.snooze_minutes = qBound(1, object["snooze"_L1].toInt(alarm.snooze_minutes), kMaximumSnoozeMinutes);
  const QJsonArray tracks = object["tracks"_L1].toArray();
  for (const QJsonValue &track : tracks) {
    if (track.isString()) alarm.tracks << track.toString();
  }
  alarm.tracks_updated = object["tracks_updated"_L1].toInteger();
  return alarm;

}

namespace AlarmSchedule {

AlarmList ListFromJson(const QByteArray &json) {

  AlarmList alarms;
  const QJsonArray array = QJsonDocument::fromJson(json).array();
  for (const QJsonValue &value : array) {
    if (!value.isObject()) continue;
    const Alarm alarm = Alarm::FromJson(value.toObject());
    if (!alarm.id.isEmpty()) alarms << alarm;
  }
  return alarms;

}

QByteArray ListToJson(const AlarmList &alarms) {

  QJsonArray array;
  for (const Alarm &alarm : alarms) array << alarm.ToJson();
  return QJsonDocument(array).toJson(QJsonDocument::Compact);

}

}  // namespace AlarmSchedule
