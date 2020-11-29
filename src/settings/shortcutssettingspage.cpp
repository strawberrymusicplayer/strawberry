/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QList>
#include <QVariant>
#include <QStringList>
#include <QAction>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QTreeWidget>
#include <QKeySequence>
#include <QShortcut>
#include <QSettings>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>

#include "core/iconloader.h"
#include "core/logging.h"
#include "core/utilities.h"
#ifdef Q_OS_MACOS
#  include "core/mac_utilities.h"
#endif
#include "globalshortcuts/globalshortcutgrabber.h"
#include "globalshortcuts/globalshortcuts.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "shortcutssettingspage.h"
#include "ui_shortcutssettingspage.h"

const char *GlobalShortcutsSettingsPage::kSettingsGroup = "GlobalShortcuts";

GlobalShortcutsSettingsPage::GlobalShortcutsSettingsPage(SettingsDialog *dialog)
    : SettingsPage(dialog),
      ui_(new Ui_GlobalShortcutsSettingsPage),
      initialized_(false),
      grabber_(new GlobalShortcutGrabber) {

  ui_->setupUi(this);
  ui_->shortcut_options->setEnabled(false);
  ui_->list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  setWindowIcon(IconLoader::Load("keyboard"));

  connect(ui_->list, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), SLOT(ItemClicked(QTreeWidgetItem*)));
  connect(ui_->radio_none, SIGNAL(clicked()), SLOT(NoneClicked()));
  connect(ui_->radio_default, SIGNAL(clicked()), SLOT(DefaultClicked()));
  connect(ui_->radio_custom, SIGNAL(clicked()), SLOT(ChangeClicked()));
  connect(ui_->button_change, SIGNAL(clicked()), SLOT(ChangeClicked()));

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#  ifdef HAVE_DBUS
  connect(ui_->checkbox_gsd, SIGNAL(clicked(bool)), SLOT(ShortcutOptionsChanged()));
  connect(ui_->checkbox_kde, SIGNAL(clicked(bool)), SLOT(ShortcutOptionsChanged()));
  connect(ui_->button_gsd_open, SIGNAL(clicked()), SLOT(OpenGnomeKeybindingProperties()));
#  endif
#  ifdef HAVE_X11EXTRAS
  connect(ui_->checkbox_x11, SIGNAL(clicked(bool)), SLOT(ShortcutOptionsChanged()));
#  endif
#else
  ui_->widget_gsd->hide();
  ui_->widget_kde->hide();
  ui_->widget_x11->hide();
#endif  // defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

}

GlobalShortcutsSettingsPage::~GlobalShortcutsSettingsPage() { delete ui_; }

bool GlobalShortcutsSettingsPage::IsEnabled() const {

#ifdef Q_OS_MACOS
  qLog(Debug) << Utilities::GetMacOsVersion();
  if (Utilities::GetMacOsVersion() < 6) {  // Leopard and earlier.
    return false;
  }
#endif

  return true;

}

void GlobalShortcutsSettingsPage::Load() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  GlobalShortcuts *manager = dialog()->global_shortcuts_manager();

  if (!initialized_) {
    initialized_ = true;

    de_ = Utilities::DesktopEnvironment();
    ui_->widget_warning->hide();

    connect(ui_->button_macos_open, SIGNAL(clicked()), manager, SLOT(ShowMacAccessibilityDialog()));

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (manager->IsGsdAvailable()) {
      qLog(Debug) << "Gnome (GSD) backend is available.";
      ui_->widget_gsd->show();
    }
    else {
      qLog(Debug) << "Gnome (GSD) backend is unavailable.";
      ui_->widget_gsd->hide();
    }

    if (manager->IsKdeAvailable()) {
      qLog(Debug) << "KDE (KGlobalAccel) backend is available.";
      ui_->widget_kde->show();
    }
    else {
      qLog(Debug) << "KDE (KGlobalAccel) backend is unavailable.";
      ui_->widget_kde->hide();
    }

    if (manager->IsX11Available()) {
      qLog(Debug) << "X11 backend is available.";
      ui_->widget_x11->show();
    }
    else {
      qLog(Debug) << "X11 backend is unavailable.";
      ui_->widget_x11->hide();
    }
#endif

    for (const GlobalShortcuts::Shortcut &i : manager->shortcuts().values()) {
      Shortcut shortcut;
      shortcut.s = i;
      shortcut.key = i.action->shortcut();
      shortcut.item = new QTreeWidgetItem(ui_->list, QStringList() << i.action->text() << i.action->shortcut().toString(QKeySequence::NativeText));
      shortcut.item->setData(0, Qt::UserRole, i.id);
      shortcuts_[i.id] = shortcut;
    }

    ui_->list->sortItems(0, Qt::AscendingOrder);
    ItemClicked(ui_->list->topLevelItem(0));
  }

  for (const Shortcut &shortcut : shortcuts_.values()) {
    SetShortcut(shortcut.s.id, shortcut.s.action->shortcut());
  }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  if (ui_->widget_gsd->isVisibleTo(this)) {
    ui_->checkbox_gsd->setChecked(s.value("use_gsd", true).toBool());
  }

  if (ui_->widget_kde->isVisibleTo(this)) {
    ui_->checkbox_kde->setChecked(s.value("use_kde", true).toBool());
  }

  if (ui_->widget_x11->isVisibleTo(this)) {
    ui_->checkbox_x11->setChecked(s.value("use_x11", false).toBool());
  }
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  ShortcutOptionsChanged();
#endif

  ui_->widget_macos->setVisible(!manager->IsMacAccessibilityEnabled());
#ifdef Q_OS_MACOS
  qint32 macos_version = Utilities::GetMacOsVersion();
  ui_->label_macos->setVisible(macos_version < 9);
  ui_->label_macos_mavericks->setVisible(macos_version >= 9);
#endif  // Q_OS_MACOS

  s.endGroup();

  Init(ui_->layout_globalshortcutssettingspage->parentWidget());

  if (!QSettings().childGroups().contains(kSettingsGroup)) set_changed();

}

void GlobalShortcutsSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  for (const Shortcut &shortcut : shortcuts_.values()) {
    shortcut.s.action->setShortcut(shortcut.key);
    shortcut.s.shortcut->setKey(shortcut.key);
    s.setValue(shortcut.s.id, shortcut.key.toString());
  }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  s.setValue("use_gsd", ui_->checkbox_gsd->isChecked());
  s.setValue("use_kde", ui_->checkbox_kde->isChecked());
  s.setValue("use_x11", ui_->checkbox_x11->isChecked());
#endif

  s.endGroup();

  dialog()->global_shortcuts_manager()->ReloadSettings();

}

void GlobalShortcutsSettingsPage::ShortcutOptionsChanged() {

  bool configure_shortcuts = (ui_->widget_kde->isVisibleTo(this) && ui_->checkbox_kde->isChecked()) ||
                             (ui_->widget_x11->isVisibleTo(this) && ui_->checkbox_x11->isChecked());

  ui_->list->setEnabled(configure_shortcuts);
  ui_->shortcut_options->setEnabled(configure_shortcuts);

  if (ui_->widget_x11->isVisibleTo(this) && ui_->checkbox_x11->isChecked()) {
    ui_->widget_warning->show();
    X11Warning();
  }
  else {
    ui_->widget_warning->hide();
  }

}

void GlobalShortcutsSettingsPage::OpenGnomeKeybindingProperties() {

  if (!QProcess::startDetached("gnome-keybinding-properties", QStringList())) {
    if (!QProcess::startDetached("gnome-control-center", QStringList() << "keyboard")) {
      QMessageBox::warning(this, "Error", tr("The \"%1\" command could not be started.").arg("gnome-keybinding-properties"));
    }
  }

}

void GlobalShortcutsSettingsPage::SetShortcut(const QString &id, const QKeySequence &key) {

  Shortcut &shortcut = shortcuts_[id];

  shortcut.key = key;
  shortcut.item->setText(1, key.toString(QKeySequence::NativeText));

}

void GlobalShortcutsSettingsPage::ItemClicked(QTreeWidgetItem *item) {

  current_id_ = item->data(0, Qt::UserRole).toString();
  Shortcut &shortcut = shortcuts_[current_id_];

  // Enable options
  ui_->shortcut_options->setEnabled(true);
  ui_->shortcut_options->setTitle(tr("Shortcut for %1").arg(shortcut.s.action->text()));

  if (shortcut.key == shortcut.s.default_key)
    ui_->radio_default->setChecked(true);
  else if (shortcut.key.isEmpty())
    ui_->radio_none->setChecked(true);
  else
    ui_->radio_custom->setChecked(true);

}

void GlobalShortcutsSettingsPage::NoneClicked() {

  SetShortcut(current_id_, QKeySequence());
  set_changed();

}

void GlobalShortcutsSettingsPage::DefaultClicked() {

  SetShortcut(current_id_, shortcuts_[current_id_].s.default_key);
  set_changed();

}

void GlobalShortcutsSettingsPage::ChangeClicked() {

  GlobalShortcuts *manager = dialog()->global_shortcuts_manager();
  manager->Unregister();
  QKeySequence key = grabber_->GetKey(shortcuts_[current_id_].s.action->text());
  manager->Register();

  if (key.isEmpty()) return;

  // Check if this key sequence is used by any other actions
  for (const QString &id : shortcuts_.keys()) {
    if (shortcuts_[id].key == key) SetShortcut(id, QKeySequence());
  }

  ui_->radio_custom->setChecked(true);
  SetShortcut(current_id_, key);

  set_changed();

}

void GlobalShortcutsSettingsPage::X11Warning() {

  if (de_.toLower() == "kde" || de_.toLower() == "gnome" || de_.toLower() == "x-cinnamon") {
    QString text(tr("Using X11 shortcuts on %1 is not recommended and can cause keyboard to become unresponsive!").arg(de_));
    if (de_.toLower() == "kde")
      text += tr(" Shortcuts on %1 are usually used through MPRIS and KGlobalAccel.").arg(de_);
    else if (de_.toLower() == "gnome")
      text += tr(" Shortcuts on %1 are usually used through GSD and should be configured in gnome-settings-daemon instead.").arg(de_);
    else if (de_.toLower() == "x-cinnamon")
      text += tr(" Shortcuts on %1 are usually used through GSD and should be configured in cinnamon-settings-daemon instead.").arg(de_);
    ui_->label_warn_text->setText(text);
    ui_->widget_warning->show();
  }
  else {
    ui_->widget_warning->hide();
  }

}
