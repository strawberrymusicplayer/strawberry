/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QLabel>
#include <QKeySequence>
#include <QDialogButtonBox>
#include <QEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QKeyEvent>

#include "globalshortcutgrabber.h"
#include "ui_globalshortcutgrabber.h"

using namespace Qt::Literals::StringLiterals;

GlobalShortcutGrabber::GlobalShortcutGrabber(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui::GlobalShortcutGrabber),
      wrapper_(nullptr) {

  ui_->setupUi(this);

  modifier_keys_ << Qt::Key_Shift << Qt::Key_Control << Qt::Key_Meta << Qt::Key_Alt << Qt::Key_AltGr;

  QObject::connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &GlobalShortcutGrabber::Accepted);
  QObject::connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &GlobalShortcutGrabber::Rejected);

}

GlobalShortcutGrabber::~GlobalShortcutGrabber() {
  delete ui_;
}

QKeySequence GlobalShortcutGrabber::GetKey(const QString &name) {

  ui_->label_shortcut->setText(tr("Press a key combination to use for %1...").arg(name));
  ui_->label_key->clear();

  ret_ = QKeySequence();

  if (exec() == QDialog::Rejected) return QKeySequence();
  return ret_;

}

void GlobalShortcutGrabber::showEvent(QShowEvent *e) {
  grabKeyboard();
  QDialog::showEvent(e);
}

void GlobalShortcutGrabber::hideEvent(QHideEvent *e) {
  releaseKeyboard();
  QDialog::hideEvent(e);
}

void GlobalShortcutGrabber::grabKeyboard() {
#ifdef Q_OS_MACOS
  SetupMacEventHandler();
#endif
  QDialog::grabKeyboard();
}

void GlobalShortcutGrabber::releaseKeyboard() {
#ifdef Q_OS_MACOS
  TeardownMacEventHandler();
#endif
  QDialog::releaseKeyboard();
}

bool GlobalShortcutGrabber::event(QEvent *e) {

  if (e->type() == QEvent::ShortcutOverride) {
    QKeyEvent *ke = static_cast<QKeyEvent*>(e);

    if (modifier_keys_.contains(ke->key())) {
      ret_ = QKeySequence(static_cast<int>(ke->modifiers()));
    }
    else {
      ret_ = QKeySequence(static_cast<int>(ke->modifiers() | ke->key()));
    }

    UpdateText();

    if (!modifier_keys_.contains(ke->key())) accept();
    return true;
  }
  return QDialog::event(e);

}

void GlobalShortcutGrabber::UpdateText() {
  ui_->label_key->setText("<b>"_L1 + ret_.toString(QKeySequence::NativeText) + "</b>"_L1);
}

void GlobalShortcutGrabber::Accepted() {
  accept();
}

void GlobalShortcutGrabber::Rejected() {
  if (ui_->label_key->text().isEmpty()) reject();
}
