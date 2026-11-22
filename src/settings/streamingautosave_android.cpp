// SPDX-License-Identifier: GPL-3.0-or-later

#include "streamingautosave_android.h"

#include <algorithm>
#include <utility>

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
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

namespace {

class HideFilter : public QObject {
 public:
  HideFilter(QObject *parent, std::function<void()> on_hide) : QObject(parent), on_hide_(std::move(on_hide)) {}

 protected:
  bool eventFilter(QObject*, QEvent *event) override {
    if (event->type() == QEvent::Hide) on_hide_();
    return false;
  }

 private:
  std::function<void()> on_hide_;
};

}  // namespace

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

  // Apply() compares the controls with the page's snapshot, so events that change nothing, such as Return without edits, do not save.
  ConnectStreamingAutoSave(context, page, save);
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
  // The snapshot does not include checkable group boxes, and a click always toggles them.
  for (QGroupBox *group : page->findChildren<QGroupBox*>()) {
    if (!group->isCheckable()) continue;
    QObject::connect(group, &QGroupBox::clicked, context, [page, save]() {
      page->set_changed();
      save();
    });
  }

}

QList<QWidget*> SubpageWidgetsOf(const QWidget *ui_widget, const QList<QPair<QWidget*, QWidget*>> &subpages) {

  QList<const QWidget*> roots{ui_widget};
  QList<QWidget*> widgets;
  for (const QPair<QWidget*, QWidget*> &subpage : subpages) {
    QWidget *content = subpage.first;
    const QWidget *content_parent = subpage.second;
    if (!content || !content_parent) continue;
    if (std::none_of(roots.cbegin(), roots.cend(), [content_parent](const QWidget *root) { return root == content_parent || root->isAncestorOf(content_parent); })) continue;
    roots << content;
    widgets << content << content->findChildren<QWidget*>();
  }
  return widgets;

}

QObject *InstallFlushOnHide(QWidget *widget, const std::function<void()> &flush) {

  auto *filter = new HideFilter(widget, flush);
  widget->installEventFilter(filter);
  return filter;

}
