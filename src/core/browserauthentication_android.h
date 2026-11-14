// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BROWSERAUTHENTICATION_ANDROID_H
#define BROWSERAUTHENTICATION_ANDROID_H

#include <jni.h>

#include <QObject>
#include <QPointer>
#include <QString>

class AndroidBrowserAuthentication : public QObject {
  Q_OBJECT

 public:
  explicit AndroidBrowserAuthentication(QObject *parent = nullptr);
  ~AndroidBrowserAuthentication() override;

  bool Start(const QString &service, QString &error);
  void Stop();
  bool active() const { return instance_ == this; }
  bool timed_out() const;
  static bool WasInterrupted(const QString &service);

 Q_SIGNALS:
  void Failed(const QString &error);

 private:
  static void NativeFinished(JNIEnv *, jclass, jlong request_id, jint reason);

  static QPointer<AndroidBrowserAuthentication> instance_;
  qint64 request_id_ = 0;
  qint64 started_at_ = 0;
  QString service_;
};

#endif  // BROWSERAUTHENTICATION_ANDROID_H
