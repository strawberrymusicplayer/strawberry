// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>

#include "constants/mainwindowsettings.h"
#include "core/settings.h"
#include "settings/sidebarsettingspage_android.h"
#include "widgets/fancytabwidget.h"

using namespace Qt::Literals::StringLiterals;

namespace {

const QString kGroup = QLatin1String(MainWindowSettings::kSettingsGroup);

class SidebarSettingsPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings settings;
    settings.remove(kGroup);
    context_ = new QWidget;
    collection_ = new QWidget;
    service_ = new QWidget;
    settings_ = new QWidget;
    tabs_.AddTab(context_, u"context"_s, QIcon(), u"Context"_s);
    tabs_.AddTab(collection_, u"collection"_s, QIcon(), u"Collection"_s);
    tabs_.AddTab(service_, u"service"_s, QIcon(), u"Service"_s);
    tabs_.AddTab(settings_, u"settings"_s, QIcon(), u"Settings"_s);
    tabs_.LoadSettings(kGroup);
    page_ = std::make_unique<AndroidSidebarSettingsPage>(nullptr, &tabs_);
    page_->Load();
    list_ = page_->findChild<QListWidget*>(u"sidebar_tabs"_s);
    up_ = page_->findChild<QPushButton*>(u"sidebar_move_up"_s);
    down_ = page_->findChild<QPushButton*>(u"sidebar_move_down"_s);
    visible_ = page_->findChild<QCheckBox*>(u"sidebar_visible"_s);
    ASSERT_NE(nullptr, list_);
    ASSERT_NE(nullptr, up_);
    ASSERT_NE(nullptr, down_);
    ASSERT_NE(nullptr, visible_);
  }

  void TearDown() override {
    Settings settings;
    settings.remove(kGroup);
  }

  FancyTabWidget tabs_;
  QWidget *context_;
  QWidget *collection_;
  QWidget *service_;
  QWidget *settings_;
  std::unique_ptr<AndroidSidebarSettingsPage> page_;
  QListWidget *list_;
  QPushButton *up_;
  QPushButton *down_;
  QCheckBox *visible_;
};

TEST_F(SidebarSettingsPageTest, EditsStayPendingAndCancelRestoresThem) {

  down_->click();
  visible_->click();
  EXPECT_EQ(1, list_->currentRow());
  EXPECT_EQ(u"Context (hidden)"_s, list_->currentItem()->text());
  EXPECT_EQ(0, tabs_.IndexOfTab(context_));
  EXPECT_TRUE(tabs_.isTabVisible(tabs_.IndexOfTab(context_)));
  Settings settings;
  EXPECT_FALSE(settings.contains(kGroup + u"/tab_context"_s));
  EXPECT_FALSE(settings.contains(kGroup + u"/tab_hidden_context"_s));
  page_->Reject();
  EXPECT_EQ(u"Context"_s, list_->item(0)->text());
  EXPECT_TRUE(visible_->isChecked());
  EXPECT_TRUE(tabs_.isTabVisible(tabs_.IndexOfTab(context_)));

}

TEST_F(SidebarSettingsPageTest, ApplyPersistsOrderAndVisibilityWithoutChangingActivePage) {

  tabs_.setCurrentIndex(tabs_.IndexOfTab(settings_));
  QWidget *active = tabs_.currentWidget();
  QSignalSpy current(&tabs_, &QTabWidget::currentChanged);
  QSignalSpy navigation(&tabs_, &FancyTabWidget::CurrentTabChanged);
  down_->click();
  visible_->click();
  page_->Apply();
  EXPECT_EQ(1, tabs_.IndexOfTab(context_));
  EXPECT_EQ(0, tabs_.IndexOfTab(collection_));
  EXPECT_FALSE(tabs_.isTabVisible(tabs_.IndexOfTab(context_)));
  EXPECT_EQ(active, tabs_.currentWidget());
  EXPECT_EQ(0, current.count());
  EXPECT_EQ(0, navigation.count());
  Settings settings;
  settings.beginGroup(kGroup);
  EXPECT_EQ(1, settings.value(u"tab_context"_s).toInt());
  EXPECT_EQ(0, settings.value(u"tab_collection"_s).toInt());
  EXPECT_TRUE(settings.value(u"tab_hidden_context"_s).toBool());
  EXPECT_EQ(1, list_->currentRow());
  visible_->click();
  page_->Reject();
  EXPECT_EQ(u"Context (hidden)"_s, list_->item(1)->text());
  EXPECT_FALSE(tabs_.isTabVisible(tabs_.IndexOfTab(context_)));

}

TEST_F(SidebarSettingsPageTest, SettingsCanMoveButCannotBeHidden) {

  tabs_.setCurrentIndex(tabs_.IndexOfTab(settings_));
  QWidget *active = tabs_.currentWidget();
  QSignalSpy navigation(&tabs_, &FancyTabWidget::CurrentTabChanged);
  list_->setCurrentRow(3);
  EXPECT_FALSE(down_->isEnabled());
  EXPECT_FALSE(visible_->isEnabled());
  EXPECT_TRUE(visible_->isChecked());
  for (int move = 0; move < 3; ++move) up_->click();
  EXPECT_EQ(0, list_->currentRow());
  EXPECT_FALSE(up_->isEnabled());
  page_->Accept();
  EXPECT_EQ(0, tabs_.IndexOfTab(settings_));
  EXPECT_EQ(active, tabs_.currentWidget());
  EXPECT_EQ(0, navigation.count());
  EXPECT_TRUE(tabs_.isTabVisible(0));
  auto arrangement = tabs_.AndroidSidebarTabs(kGroup);
  arrangement[0].visible = false;
  tabs_.SaveAndroidSidebarSettings(kGroup, arrangement);
  EXPECT_TRUE(tabs_.isTabVisible(tabs_.IndexOfTab(settings_)));

}

TEST_F(SidebarSettingsPageTest, DisabledServiceKeepsItsPositionAndHiddenPreference) {

  tabs_.DisableTab(service_);
  page_->Load();
  list_->setCurrentRow(2);
  EXPECT_EQ(u"Service (disabled)"_s, list_->currentItem()->text());
  up_->click();
  up_->click();
  visible_->click();
  page_->Apply();
  EXPECT_EQ(-1, tabs_.IndexOfTab(service_));
  EXPECT_EQ(u"service"_s, tabs_.AndroidSidebarTabs(kGroup).first().name);
  tabs_.EnableTab(service_);
  tabs_.LoadAndroidSidebarSettings(kGroup);
  EXPECT_EQ(0, tabs_.IndexOfTab(service_));
  EXPECT_FALSE(tabs_.isTabVisible(tabs_.IndexOfTab(service_)));
  tabs_.DisableTab(service_);
  tabs_.LoadAndroidSidebarSettings(kGroup);
  EXPECT_FALSE(tabs_.AndroidSidebarTabs(kGroup).first().available);
  tabs_.EnableTab(service_);
  tabs_.LoadAndroidSidebarSettings(kGroup);
  EXPECT_EQ(0, tabs_.IndexOfTab(service_));
  EXPECT_FALSE(tabs_.isTabVisible(0));

}

TEST_F(SidebarSettingsPageTest, ArrangementSurvivesFreshWidgetAndDoesNotMutateDesktopDefaults) {

  down_->click();
  visible_->click();
  page_->Apply();
  FancyTabWidget reopened;
  QWidget *context = new QWidget;
  reopened.AddTab(context, u"context"_s, QIcon(), u"Context"_s);
  reopened.AddTab(new QWidget, u"collection"_s, QIcon(), u"Collection"_s);
  reopened.AddTab(new QWidget, u"service"_s, QIcon(), u"Service"_s);
  reopened.AddTab(new QWidget, u"settings"_s, QIcon(), u"Settings"_s);
  reopened.LoadSettings(kGroup);
  EXPECT_TRUE(reopened.isTabVisible(reopened.IndexOfTab(context)));
  reopened.LoadAndroidSidebarSettings(kGroup);
  EXPECT_EQ(1, reopened.IndexOfTab(context));
  EXPECT_FALSE(reopened.isTabVisible(reopened.IndexOfTab(context)));

}

TEST_F(SidebarSettingsPageTest, HidingEveryOptionalTabKeepsSettingsReachable) {

  QSignalSpy navigation(&tabs_, &FancyTabWidget::CurrentTabChanged);
  for (int row = 0; row < 3; ++row) {
    list_->setCurrentRow(row);
    visible_->click();
  }
  page_->Apply();
  EXPECT_EQ(tabs_.IndexOfTab(settings_), tabs_.currentIndex());
  EXPECT_TRUE(tabs_.isTabVisible(tabs_.currentIndex()));
  EXPECT_EQ(1, navigation.count());
  for (QWidget *view : {context_, collection_, service_}) EXPECT_FALSE(tabs_.isTabVisible(tabs_.IndexOfTab(view)));

}

TEST_F(SidebarSettingsPageTest, NarrowEditorWrapsAtLargerFontAndKeepsTouchTargets) {

  auto *editor = page_->findChild<QWidget*>(u"sidebar_editor"_s);
  ASSERT_NE(nullptr, editor);
  QFont font = page_->font();
  font.setPointSizeF(font.pointSizeF() * 1.5);
  page_->setFont(font);
  editor->setWindowFlag(Qt::Window);
  editor->resize(220, 700);
  editor->show();
  QApplication::processEvents();
  EXPECT_LE(editor->minimumSizeHint().width(), 220);
  EXPECT_GE(up_->height(), 48);
  EXPECT_GE(down_->height(), 48);
  EXPECT_GE(visible_->height(), 56);
  EXPECT_TRUE(list_->wordWrap());
  EXPECT_EQ(Qt::ScrollBarAlwaysOff, list_->horizontalScrollBarPolicy());
  EXPECT_GE(list_->height(), 224);

}

}  // namespace
