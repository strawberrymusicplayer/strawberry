/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QPair>
#include <QList>
#include <QVariant>
#include <QString>
#include <QSettings>

#include "core/logging.h"
#include "core/settings.h"
#include "constants/notificationssettings.h"

class QCheckBox;
class QComboBox;
class QRadioButton;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QLineEdit;
class QShowEvent;

class SettingsDialog;

class SettingsPage : public QWidget {
  Q_OBJECT

 public:
  explicit SettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);

  void Init(QWidget *ui_widget);

  // Return false to grey out the page's item in the list.
  virtual bool IsEnabled() const { return true; }

  virtual void Load() = 0;

  void Accept();
  void Reject();
  void Apply();

  // The dialog that this page belongs to.
  SettingsDialog *dialog() const { return dialog_; }

  void set_changed() { changed_ = true; }

  static void ComboBoxLoadFromSettings(const Settings &s, QComboBox *combobox, const QString &setting, const QString &default_value);
  static void ComboBoxLoadFromSettings(const Settings &s, QComboBox *combobox, const QString &setting, const int default_value);
  static void ComboBoxLoadFromSettingsByIndex(const Settings &s, QComboBox *combobox, const QString &setting, const int default_value);

 private:
  virtual void Save() = 0;
  virtual void Cancel() {}

 protected:
  bool eventFilter(QObject *obj, QEvent *e) override;

 Q_SIGNALS:
  void NotificationPreview(const OSDSettings::Type, const QString&, const QString&);

 private:
  SettingsDialog *dialog_;
  QWidget *ui_widget_;
  bool changed_;
  QList<QPair<QCheckBox*, Qt::CheckState>> checkboxes_;
  QList<QPair<QRadioButton*, bool>> radiobuttons_;
  QList<QPair<QComboBox*, QString>> comboboxes_;
  QList<QPair<QSpinBox*, int>> spinboxes_;
  QList<QPair<QDoubleSpinBox*, double>> double_spinboxes_;
  QList<QPair<QSlider*, int>> sliders_;
  QList<QPair<QLineEdit*, QString>> lineedits_;
};

#endif  // SETTINGSPAGE_H
