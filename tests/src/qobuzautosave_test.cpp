// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include "core/settings.h"
#include "settings/settingspage.h"
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

namespace {

class AutoSavePage : public SettingsPage {
 public:
  AutoSavePage() : SettingsPage(nullptr) {
    auto *layout = new QVBoxLayout(this);
    checkbox = new QCheckBox(this);
    field = new QLineEdit(this);
    navigation = new QPushButton(this);
    remove = new QPushButton(this);
    group = new QGroupBox(this);
    group->setCheckable(true);
    for (QWidget *widget : {static_cast<QWidget*>(checkbox), static_cast<QWidget*>(field), static_cast<QWidget*>(navigation), static_cast<QWidget*>(remove), static_cast<QWidget*>(group)}) {
      layout->addWidget(widget);
    }
    QObject::connect(remove, &QPushButton::clicked, this, [this]() { set_changed(); });
  }
  void Load() override { Init(this); }
  int saves = 0;
  QCheckBox *checkbox;
  QLineEdit *field;
  QPushButton *navigation;
  QPushButton *remove;
  QGroupBox *group;

 private:
  void Save() override { ++saves; }
};

}  // namespace

TEST(SettingsPageAutoSaveTest, ChangesSaveThroughApplyWithoutDialogButtons) {

  AutoSavePage page;
  page.Load();
  ConnectSettingsPageAutoSave(&page, &page, [&page]() { page.Apply(); });

  page.checkbox->click();
  EXPECT_EQ(1, page.saves);
  page.field->setText(u"pattern"_s);
  EXPECT_EQ(1, page.saves);
  ASSERT_TRUE(QMetaObject::invokeMethod(page.field, "editingFinished"));
  EXPECT_EQ(2, page.saves);
  // A navigation button that changes nothing must not save.
  page.navigation->click();
  EXPECT_EQ(2, page.saves);
  EXPECT_FALSE(page.Apply());
  // Page actions such as Remove folder only mark the page as changed; they are saved right away.
  page.remove->click();
  EXPECT_EQ(3, page.saves);
  page.group->setChecked(false);
  Q_EMIT page.group->clicked(false);
  EXPECT_EQ(4, page.saves);
  // Events that change nothing, such as Return without edits, do not save.
  ASSERT_TRUE(QMetaObject::invokeMethod(page.field, "editingFinished"));
  EXPECT_EQ(4, page.saves);
  // Toggling back is a change compared with the last save.
  page.checkbox->click();
  EXPECT_EQ(5, page.saves);

}
