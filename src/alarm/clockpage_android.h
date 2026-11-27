// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLOCKPAGE_ANDROID_H
#define CLOCKPAGE_ANDROID_H

#include <functional>

#include <jni.h>

#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QWidget>

class QKeyEvent;
class QLabel;
class QShowEvent;
class QStackedWidget;

// The Tools > Clock page on Android: Show clock opens the fullscreen clock, and Clock settings chooses its type and brightness.
// The settings are saved immediately to AlarmStore.java, which the clock and the alarm screen read.
class ClockPage : public QWidget {
  Q_OBJECT

 public:
  explicit ClockPage(QWidget *parent = nullptr);

  // Returns to the menu, for opening the page from Tools.
  void ShowMenu();

 Q_SIGNALS:
  void OpenAlarmList();

 protected:
  void showEvent(QShowEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private:
  using Choice = QPair<int, QString>;

  void SetPage(QWidget *page, const QString &title);
  // Returns one level up, or closes the page from the menu.
  void Back();
  void ShowSettingsPage();
  void RefreshSettings();
  void ShowChoicePage(const QString &title, const QList<Choice> &choices, const int current, const std::function<void(int)> &chosen);

  static QString ClockTypeText(const int type);
  static QString BrightnessText(const int percent);
  static jboolean NativeOpenAlarmList(JNIEnv *env, jclass type);

 private:
  static QPointer<ClockPage> instance_;
  QLabel *title_;
  QStackedWidget *pages_;
  QWidget *menu_;
  QPointer<QWidget> settings_;
};

#endif  // CLOCKPAGE_ANDROID_H
