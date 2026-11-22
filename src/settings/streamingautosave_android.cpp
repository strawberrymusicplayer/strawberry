// SPDX-License-Identifier: GPL-3.0-or-later

#include "streamingautosave_android.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QWidget>

#include "settingspage.h"

void ConnectStreamingAutoSave(QObject *context, QWidget *page, const std::function<void()> &save) {

  for (QCheckBox *checkbox : page->findChildren<QCheckBox*>()) {
    QObject::connect(checkbox, &QCheckBox::clicked, context, save);
  }
  for (QRadioButton *radio : page->findChildren<QRadioButton*>()) {
    QObject::connect(radio, &QRadioButton::clicked, context, save);
  }
  for (QComboBox *combo : page->findChildren<QComboBox*>()) {
    QObject::connect(combo, &QComboBox::activated, context, save);
  }
  for (QSpinBox *spinbox : page->findChildren<QSpinBox*>()) {
    spinbox->setKeyboardTracking(false);
    QObject::connect(spinbox, &QSpinBox::valueChanged, context, save);
  }
  for (QDoubleSpinBox *spinbox : page->findChildren<QDoubleSpinBox*>()) {
    spinbox->setKeyboardTracking(false);
    QObject::connect(spinbox, &QDoubleSpinBox::valueChanged, context, save);
  }
  for (QSlider *slider : page->findChildren<QSlider*>()) {
    QObject::connect(slider, &QSlider::valueChanged, context, save);
  }
  for (QLineEdit *field : page->findChildren<QLineEdit*>()) {
    if (qobject_cast<QAbstractSpinBox*>(field->parentWidget())) continue;
    QObject::connect(field, &QLineEdit::editingFinished, context, save);
  }

}

void ConnectSettingsPageAutoSave(QObject *context, SettingsPage *page, const std::function<void()> &save) {

  // An editor reparents its controls, so the snapshot from SettingsPage::Init() can miss them; mark the page changed instead.
  ConnectStreamingAutoSave(context, page, [page, save]() {
    page->set_changed();
    save();
  });
  // Buttons that change settings mark their page as changed themselves; Apply() saves only real changes.
  for (QPushButton *button : page->findChildren<QPushButton*>()) {
    QObject::connect(button, &QPushButton::clicked, context, save);
  }
  for (QToolButton *button : page->findChildren<QToolButton*>()) {
    QObject::connect(button, &QToolButton::clicked, context, save);
  }
  for (QListWidget *list : page->findChildren<QListWidget*>()) {
    QObject::connect(list, &QListWidget::itemChanged, context, save);
  }
  for (QGroupBox *group : page->findChildren<QGroupBox*>()) {
    if (!group->isCheckable()) continue;
    QObject::connect(group, &QGroupBox::clicked, context, [page, save]() {
      page->set_changed();
      save();
    });
  }

}
