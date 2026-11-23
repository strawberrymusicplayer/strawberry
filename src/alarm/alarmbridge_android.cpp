// SPDX-License-Identifier: GPL-3.0-or-later

#include <jni.h>

#include <QByteArray>
#include <QJsonDocument>
#include <QJniEnvironment>
#include <QJniObject>
#include <QString>
#include <QtCore/qcoreapplication_platform.h>

#include "core/logging.h"
#include "alarmbridge_android.h"

namespace {

constexpr char kBridgeClass[] = "nl/dewitweb/strawberryflavor/AlarmBridge";

QJniObject Context() {
  return QNativeInterface::QAndroidApplication::context();
}

// Logs and clears a Java exception, which would otherwise abort the next JNI call.
void CheckException(const char *method) {

  QJniEnvironment env;
  if (env.checkAndClearExceptions()) {
    qLog(Error) << "Could not call AlarmBridge." << method;
  }

}

QJniObject String(const QString &text) {
  return QJniObject::fromString(text);
}

}  // namespace

namespace AlarmBridge {

void Init(const QJsonObject &labels) {

  QJniObject::callStaticMethod<void>(kBridgeClass, "init", "(Landroid/content/Context;Ljava/lang/String;)V", Context().object(), String(QString::fromUtf8(QJsonDocument(labels).toJson(QJsonDocument::Compact))).object<jstring>());
  CheckException("init");

}

AlarmList Alarms() {

  const QString json = QJniObject::callStaticObjectMethod(kBridgeClass, "getAlarms", "(Landroid/content/Context;)Ljava/lang/String;", Context().object()).toString();
  CheckException("getAlarms");
  return AlarmSchedule::ListFromJson(json.toUtf8());

}

Alarm FindAlarm(const QString &id) {

  const AlarmList alarms = Alarms();
  for (const Alarm &alarm : alarms) {
    if (alarm.id == id) return alarm;
  }
  return Alarm();

}

bool SaveAlarm(const Alarm &alarm) {

  const QString json = QString::fromUtf8(QJsonDocument(alarm.ToJson()).toJson(QJsonDocument::Compact));
  const bool saved = QJniObject::callStaticMethod<jboolean>(kBridgeClass, "saveAlarm", "(Landroid/content/Context;Ljava/lang/String;)Z", Context().object(), String(json).object<jstring>());
  CheckException("saveAlarm");
  return saved;

}

void DeleteAlarm(const QString &id) {

  QJniObject::callStaticMethod<void>(kBridgeClass, "deleteAlarm", "(Landroid/content/Context;Ljava/lang/String;)V", Context().object(), String(id).object<jstring>());
  CheckException("deleteAlarm");

}

int AutoStopMinutes() {

  const int minutes = QJniObject::callStaticMethod<jint>(kBridgeClass, "getAutoStopMinutes", "(Landroid/content/Context;)I", Context().object());
  CheckException("getAutoStopMinutes");
  return minutes;

}

void SetAutoStopMinutes(const int minutes) {

  QJniObject::callStaticMethod<void>(kBridgeClass, "setAutoStopMinutes", "(Landroid/content/Context;I)V", Context().object(), static_cast<jint>(minutes));
  CheckException("setAutoStopMinutes");

}

int ClockType() {

  const int value = QJniObject::callStaticMethod<jint>(kBridgeClass, "getClockType", "(Landroid/content/Context;)I", Context().object());
  CheckException("getClockType");
  return value;

}

void SetClockType(const int type) {

  QJniObject::callStaticMethod<void>(kBridgeClass, "setClockType", "(Landroid/content/Context;I)V", Context().object(), static_cast<jint>(type));
  CheckException("setClockType");

}

int ClockBrightness() {

  const int value = QJniObject::callStaticMethod<jint>(kBridgeClass, "getClockBrightness", "(Landroid/content/Context;)I", Context().object());
  CheckException("getClockBrightness");
  return value;

}

void SetClockBrightness(const int percent) {

  QJniObject::callStaticMethod<void>(kBridgeClass, "setClockBrightness", "(Landroid/content/Context;I)V", Context().object(), static_cast<jint>(percent));
  CheckException("setClockBrightness");

}

qint64 NextAlarmMsec() {

  const qint64 next = QJniObject::callStaticMethod<jlong>(kBridgeClass, "nextAlarmMillis", "(Landroid/content/Context;)J", Context().object());
  CheckException("nextAlarmMillis");
  return next;

}

qint64 SnoozeUntilMsec(const QString &id) {

  const qint64 until = QJniObject::callStaticMethod<jlong>(kBridgeClass, "snoozeUntil", "(Landroid/content/Context;Ljava/lang/String;)J", Context().object(), String(id).object<jstring>());
  CheckException("snoozeUntil");
  return until;

}

int MissingPermissions() {

  const int missing = QJniObject::callStaticMethod<jint>(kBridgeClass, "missingPermissions", "(Landroid/content/Context;)I", Context().object());
  CheckException("missingPermissions");
  return missing;

}

void RequestRuntimePermissions() {

  QJniObject::callStaticMethod<void>(kBridgeClass, "requestRuntimePermissions", "(Landroid/content/Context;)V", Context().object());
  CheckException("requestRuntimePermissions");

}

bool IsAlarmSounding() {

  const bool sounding = QJniObject::callStaticMethod<jboolean>(kBridgeClass, "isAlarmSounding", "()Z");
  CheckException("isAlarmSounding");
  return sounding;

}

void ShowRingingScreen() {

  QJniObject::callStaticMethod<void>(kBridgeClass, "showRingingScreen", "(Landroid/content/Context;)V", Context().object());
  CheckException("showRingingScreen");

}

void RequestPermission(const int permission) {

  QJniObject::callStaticMethod<void>(kBridgeClass, "requestPermission", "(Landroid/content/Context;I)V", Context().object(), static_cast<jint>(permission));
  CheckException("requestPermission");

}

void OpenClock() {

  QJniObject::callStaticMethod<void>(kBridgeClass, "openClock", "(Landroid/content/Context;)V", Context().object());
  CheckException("openClock");

}

}  // namespace AlarmBridge
