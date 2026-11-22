// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QMetaObject>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include "core/settings.h"
#include "settings/streamingautosave_android.h"
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
  ConnectStreamingAutoSave(&page, &page, [&]() {
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
  ui.enable->click();
  EXPECT_EQ(1, saves);
  EXPECT_FALSE(settings.value(u"QobuzAutoSaveTest/enabled"_s).toBool());
  ui.format->setCurrentIndex(1);
  Q_EMIT ui.format->activated(1);
  EXPECT_EQ(2, saves);
  EXPECT_EQ(1, settings.value(u"QobuzAutoSaveTest/format"_s).toInt());
  ui.searchdelay->setValue(2000);
  EXPECT_EQ(3, saves);
  EXPECT_FALSE(ui.searchdelay->keyboardTracking());
  ui.checkbox_remove_remastered->click();
  EXPECT_EQ(4, saves);
  ui.app_id->setText(u"test-app"_s);
  EXPECT_EQ(4, saves);
  ASSERT_TRUE(QMetaObject::invokeMethod(ui.app_id, "editingFinished"));
  EXPECT_EQ(5, saves);
  EXPECT_EQ(u"test-app"_s, settings.value(u"QobuzAutoSaveTest/app_id"_s).toString());
  settings.remove(u"QobuzAutoSaveTest"_s);

}

TEST(QobuzAutoSaveTest, IgnoresProgrammaticSelectionsAndSurvivesEditorReparenting) {

  QWidget page;
  auto *editor = new QWidget(&page);
  auto *layout = new QVBoxLayout(editor);
  auto *checkbox = new QCheckBox(editor);
  auto *combo = new QComboBox(editor);
  auto *radio = new QRadioButton(editor);
  auto *slider = new QSlider(editor);
  auto *field = new QLineEdit(editor);
  auto *spinbox = new QSpinBox(editor);
  layout->addWidget(checkbox);
  layout->addWidget(combo);
  layout->addWidget(radio);
  layout->addWidget(slider);
  layout->addWidget(field);
  layout->addWidget(spinbox);
  combo->addItems({u"First"_s, u"Second"_s});
  int saves = 0;
  ConnectStreamingAutoSave(&page, &page, [&saves]() { ++saves; });
  checkbox->setChecked(true);
  radio->setChecked(true);
  combo->setCurrentIndex(1);
  field->setText(u"test"_s);
  EXPECT_EQ(0, saves);

  QWidget subscreen;
  auto *subscreen_layout = new QVBoxLayout(&subscreen);
  subscreen_layout->addWidget(editor);
  subscreen.show();
  QApplication::setActiveWindow(&subscreen);
  field->setFocus();
  QTest::keyClicks(field, u"client-id"_s);
  EXPECT_EQ(0, saves);
  checkbox->setFocus();
  EXPECT_EQ(1, saves);
  checkbox->click();
  Q_EMIT combo->activated(1);
  radio->click();
  slider->setValue(10);
  EXPECT_EQ(5, saves);
  ASSERT_TRUE(QMetaObject::invokeMethod(spinbox->findChild<QLineEdit*>(), "editingFinished"));
  EXPECT_EQ(5, saves);
  spinbox->setValue(3);
  EXPECT_EQ(6, saves);

}
