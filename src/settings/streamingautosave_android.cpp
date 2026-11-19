// SPDX-License-Identifier: GPL-3.0-or-later

#include "streamingautosave_android.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QObject>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

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
