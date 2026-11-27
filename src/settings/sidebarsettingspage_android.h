// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SIDEBARSETTINGSPAGE_ANDROID_H
#define SIDEBARSETTINGSPAGE_ANDROID_H

#include "settingspage.h"
#include "widgets/fancytabwidget.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;

class AndroidSidebarSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit AndroidSidebarSettingsPage(SettingsDialog *dialog, FancyTabWidget *sidebar, QWidget *parent = nullptr);
  void Load() override;

 private:
  void Save() override;
  void Cancel() override;
  void MoveSelectedTab(int direction);
  void UpdateSelection();
  void UpdateSummary();

  FancyTabWidget *sidebar_;
  QList<FancyTabWidget::SidebarTab> pending_;
  QWidget *editor_;
  QListWidget *list_;
  QPushButton *move_up_;
  QPushButton *move_down_;
  QCheckBox *visible_;
  QLabel *hint_;
  QLabel *summary_;
};

#endif  // SIDEBARSETTINGSPAGE_ANDROID_H
