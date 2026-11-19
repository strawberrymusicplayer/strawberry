// SPDX-License-Identifier: GPL-3.0-or-later

#include "qobuzsettingspage.h"

#include "streamingautosave_android.h"
#include "settingsdialog.h"
#include "ui_qobuzsettingspage.h"

void QobuzSettingsPage::SetupAndroidAutoSave() {

  ConnectStreamingAutoSave(this, this, [this]() {
    if (android_settings_loaded_ && !dialog()->is_loading_settings()) SaveAndroidSettings();
  });

}

bool QobuzSettingsPage::SaveAndroidSettings() {

  Save();
  if (!android_save_succeeded_) return false;
  Q_EMIT dialog()->ReloadSettings();
  return true;

}
