// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ALARMCLOCKPAGE_ANDROID_H
#define ALARMCLOCKPAGE_ANDROID_H

#include <functional>

#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "includes/shared_ptr.h"
#include "alarmschedule_android.h"

class QCheckBox;
class QHideEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QShowEvent;
class QSlider;
class QStackedWidget;
class QTimer;
class QVBoxLayout;
class AlarmClock;
class RadioServices;

namespace AndroidWidgets {
class ValueButton;
}

// The Tools > Alarm clock page on Android: the list of alarms and their editor.
// Every change is saved immediately to AlarmStore.java, which rings the alarms.
class AlarmClockPage : public QWidget {
  Q_OBJECT

 public:
  explicit AlarmClockPage(AlarmClock *alarm_clock, SharedPtr<RadioServices> radio_services, QWidget *parent = nullptr);

  // Returns to the list of alarms, for opening the page from Tools.
  void ShowOverview();

 protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private:
  using Choice = QPair<int, QString>;

  void SetPage(QWidget *page, const QString &title);
  // Returns one level up, or closes the page from the overview.
  void Back();
  void BackTo(QWidget *page);
  void RefreshOverview();
  // The settings for all alarms, on their own page below the overview.
  void ShowSettingsPage();
  void RefreshSettings();
  void ShowEditor(const QString &id);
  void RefreshEditor();
  void ShowTimePage();
  void ShowDaysPage();
  void ShowSourcePage();
  void ShowPlaylistPage();
  void ShowSongPage();
  void ShowRadioPage();
  void ShowChoicePage(const QString &title, const QList<Choice> &choices, const int current, const std::function<void(int)> &chosen);
  void ShowDeletePage();
  void AddPermissionWarnings(QVBoxLayout *layout);
  // Adds the settings for the copies of the alarms' songs below the other settings for all alarms.
  void AddTrackSettings(QVBoxLayout *layout);
  void ShowDeleteCopiesPage();
  void SetSource(const Alarm::Source source, const QString &source_id = QString(), const QString &source_name = QString(), const QString &source_url = QString());
  // Saves the edited alarm after a short delay, so repeated taps on the time buttons are saved once.
  void ScheduleSave();
  void Save();

  static QString TimeText(const int hour, const int minute);
  static QString SourceText(const Alarm &alarm);
  static QString MinutesText(const int minutes);
  static QString SizeText(const int megabytes);
  static QString FadeInText(const int seconds);

 private:
  AlarmClock *alarm_clock_;
  SharedPtr<RadioServices> radio_services_;
  QLabel *title_;
  QStackedWidget *pages_;
  QWidget *overview_;
  QWidget *overview_contents_;
  QPointer<QWidget> settings_;
  QPointer<QWidget> editor_;
  Alarm alarm_;
  // Whether the on/off switch in the editor changed since the last save.
  bool enabled_changed_ = false;
  QTimer *save_timer_;
  QPointer<AndroidWidgets::ValueButton> time_button_;
  QPointer<AndroidWidgets::ValueButton> days_button_;
  QPointer<AndroidWidgets::ValueButton> source_button_;
  QPointer<AndroidWidgets::ValueButton> fade_in_button_;
  QPointer<AndroidWidgets::ValueButton> snooze_button_;
  QPointer<QCheckBox> enabled_;
  QPointer<QLineEdit> name_;
  QPointer<QSlider> volume_;
  QPointer<QLabel> volume_label_;
};

#endif  // ALARMCLOCKPAGE_ANDROID_H
