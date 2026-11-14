// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QApplication>
#include <QLineEdit>
#include <QMetaObject>
#include <QWidget>

#include "core/settings.h"
#include "settings/qobuzautosave.h"
#include "ui_qobuzsettingspage.h"

using namespace Qt::Literals::StringLiterals;

TEST(QobuzAutoSaveTest, SavesEachControlAndDefersTextUntilEditingFinishes) {

  QWidget page;
  Ui::QobuzSettingsPage ui;
  ui.setupUi(&page);
  ui.format->addItems({u"First"_s, u"Second"_s});
  Settings settings;
  settings.remove(u"QobuzAutoSaveTest"_s);
  bool loading = false;
  int saves = 0;
  ConnectQobuzAutoSave(&page, &ui, [&]() {
    if (loading) return;
    ++saves;
    settings.setValue(u"QobuzAutoSaveTest/app_id"_s, ui.app_id->text());
    settings.setValue(u"QobuzAutoSaveTest/enabled"_s, ui.enable->isChecked());
    settings.setValue(u"QobuzAutoSaveTest/format"_s, ui.format->currentIndex());
    settings.sync();
  });

  loading = true;
  ui.enable->setChecked(true);
  EXPECT_EQ(0, saves);
  loading = false;
  ui.enable->setChecked(false);
  EXPECT_EQ(1, saves);
  EXPECT_FALSE(settings.value(u"QobuzAutoSaveTest/enabled"_s).toBool());
  ui.format->setCurrentIndex(1);
  EXPECT_EQ(2, saves);
  EXPECT_EQ(1, settings.value(u"QobuzAutoSaveTest/format"_s).toInt());
  ui.searchdelay->setValue(2000);
  EXPECT_EQ(3, saves);
  EXPECT_FALSE(ui.searchdelay->keyboardTracking());
  ui.checkbox_remove_remastered->setChecked(!ui.checkbox_remove_remastered->isChecked());
  EXPECT_EQ(4, saves);
  ui.app_id->setText(u"test-app"_s);
  EXPECT_EQ(4, saves);
  ASSERT_TRUE(QMetaObject::invokeMethod(ui.app_id, "editingFinished"));
  EXPECT_EQ(5, saves);
  EXPECT_EQ(u"test-app"_s, settings.value(u"QobuzAutoSaveTest/app_id"_s).toString());
  settings.remove(u"QobuzAutoSaveTest"_s);

}
