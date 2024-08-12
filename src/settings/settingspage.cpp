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

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QSlider>
#include <QLineEdit>
#include <QSettings>
#include <QShowEvent>

#include "core/logging.h"
#include "core/settings.h"

#include "settingsdialog.h"
#include "settingspage.h"

SettingsPage::SettingsPage(SettingsDialog *dialog, QWidget *parent)
    : QWidget(parent),
      dialog_(dialog),
      ui_widget_(nullptr),
      changed_(false) {}

void SettingsPage::Init(QWidget *ui_widget) {

  if (!ui_widget) return;

  ui_widget_ = ui_widget;
  changed_ = false;

  checkboxes_.clear();
  radiobuttons_.clear();
  comboboxes_.clear();
  spinboxes_.clear();
  sliders_.clear();
  lineedits_.clear();

  const QList<QWidget*> list = ui_widget_->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
  for (QWidget *w : list) {
    if (QCheckBox *checkbox = qobject_cast<QCheckBox*>(w)) {
      checkboxes_ << qMakePair(checkbox, checkbox->checkState());
    }
    else if (QRadioButton *radiobutton = qobject_cast<QRadioButton*>(w)) {
      radiobuttons_ << qMakePair(radiobutton, radiobutton->isChecked());
    }
    else if (QComboBox *combobox = qobject_cast<QComboBox*>(w)) {
      combobox->setFocusPolicy(Qt::StrongFocus);
      combobox->installEventFilter(this);
      comboboxes_ << qMakePair(combobox, combobox->currentText());
    }
    else if (QSpinBox *spinbox = qobject_cast<QSpinBox*>(w)) {
      spinbox->setFocusPolicy(Qt::StrongFocus);
      spinbox->installEventFilter(this);
      spinboxes_ << qMakePair(spinbox, spinbox->value());
    }
    else if (QDoubleSpinBox *double_spinbox = qobject_cast<QDoubleSpinBox*>(w)) {
      double_spinbox->setFocusPolicy(Qt::StrongFocus);
      double_spinbox->installEventFilter(this);
      double_spinboxes_ << qMakePair(double_spinbox, double_spinbox->value());
    }
    else if (QLineEdit *lineedit = qobject_cast<QLineEdit*>(w)) {
      lineedits_ << qMakePair(lineedit, lineedit->text());
    }
    else if (QSlider *slider = qobject_cast<QSlider*>(w)) {
      slider->setFocusPolicy(Qt::StrongFocus);
      slider->installEventFilter(this);
      sliders_ << qMakePair(slider, slider->value());
    }
  }

}

void SettingsPage::Accept() {
  Apply();
}

void SettingsPage::Reject() {
  Cancel();
  changed_ = false;
}

void SettingsPage::Apply() {

  if (!ui_widget_) {
    qLog(Error) << windowTitle() << "is not initialized!";
    changed_ = true;
  }

  for (QPair<QCheckBox*, Qt::CheckState> &checkbox : checkboxes_) {
    if (checkbox.first->checkState() == checkbox.second) continue;
    changed_ = true;
    qLog(Info) << checkbox.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }
  for (QPair<QRadioButton*, bool> &radiobutton : radiobuttons_) {
    if (radiobutton.first->isChecked() == radiobutton.second) continue;
    changed_ = true;
    qLog(Info) << radiobutton.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }
  for (QPair<QComboBox*, QString> &combobox : comboboxes_) {
    if (combobox.first->currentText() == combobox.second) continue;
    changed_ = true;
    qLog(Info) << combobox.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }
  for (QPair<QSpinBox*, int> &spinbox : spinboxes_) {
    if (spinbox.first->value() == spinbox.second) continue;
    changed_ = true;
    qLog(Info) << spinbox.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }
  for (QPair<QDoubleSpinBox*, double> &double_spinbox : double_spinboxes_) {
    if (double_spinbox.first->value() == double_spinbox.second) continue;
    changed_ = true;
    qLog(Info) << double_spinbox.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }
  for (QPair<QLineEdit*, QString> &lineedit : lineedits_) {
    if (lineedit.first->text() == lineedit.second) continue;
    changed_ = true;
    if (lineedit.first->objectName().isEmpty()) continue;
    qLog(Info) << lineedit.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }
  for (QPair<QSlider*, int> &slider : sliders_) {
    if (slider.first->value() == slider.second) continue;
    changed_ = true;
    qLog(Info) << slider.first->objectName() << "is changed for" << windowTitle() << "settings.";
  }

  if (changed_) {
    qLog(Info) << "Saving settings for" << windowTitle();
    Save();
    Init(ui_widget_);
  }

}

void SettingsPage::ComboBoxLoadFromSettings(const Settings &s, QComboBox *combobox, const QString &setting, const QString &default_value) {

  QString value = s.value(setting, default_value).toString();
  int i = combobox->findData(value);
  if (i == -1) i = combobox->findData(default_value);
  combobox->setCurrentIndex(i);

}

void SettingsPage::ComboBoxLoadFromSettings(const Settings &s, QComboBox *combobox, const QString &setting, const int default_value) {

  int value = s.value(setting, default_value).toInt();
  int i = combobox->findData(value);
  if (i == -1) i = combobox->findData(default_value);
  combobox->setCurrentIndex(i);

}

void SettingsPage::ComboBoxLoadFromSettingsByIndex(const Settings &s, QComboBox *combobox, const QString &setting, const int default_value) {

  if (combobox->count() == 0) return;
  int i = s.value(setting, default_value).toInt();
  if (i <= 0 || i >= combobox->count()) i = 0;
  combobox->setCurrentIndex(i);

}

bool SettingsPage::eventFilter(QObject *obj, QEvent *e) {

  if (e->type() == QEvent::Wheel) {
    if (QComboBox *combobox = qobject_cast<QComboBox*>(obj)) {
      if (!combobox->hasFocus()) {
        return event(e);
      }
    }
    else if (QSpinBox *spinbox = qobject_cast<QSpinBox*>(obj)) {
      if (!spinbox->hasFocus()) {
        return event(e);
      }
    }
    else if (QDoubleSpinBox *double_spinbox = qobject_cast<QDoubleSpinBox*>(obj)) {
      if (!double_spinbox->hasFocus()) {
        return event(e);
      }
    }
    else if (QSlider *slider = qobject_cast<QSlider*>(obj)) {
      if (!slider->hasFocus()) {
        return event(e);
      }
    }
  }

  return false;

}

