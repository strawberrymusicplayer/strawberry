// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTabBar>
#include <QTest>
#include <QVBoxLayout>

#include "widgets/androidwidgets.h"
#include "widgets/fancytabwidget.h"
#include "widgets/menupage_android.h"

namespace {

QString ButtonText(const QAbstractButton *button) {

  QStringList texts;
  for (const QLabel *label : button->findChildren<QLabel*>()) {
    if (!label->isHidden()) texts << label->text();
  }
  return texts.join(QLatin1Char('|'));

}

class MenuPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    window_.resize(360, 640);
    auto *layout = new QVBoxLayout(&window_);
    tabs_ = new FancyTabWidget(&window_);
    layout->addWidget(tabs_);

    content_ = new QWidget;
    auto *content_layout = new QVBoxLayout(content_);
    owner_ = new QPushButton(QStringLiteral("Options"), content_);
    content_layout->addWidget(owner_);
    tabs_->AddTab(content_, QStringLiteral("Collection"), QIcon(), QStringLiteral("Collection"));
    ASSERT_TRUE(tabs_->EnableTab(content_));
    other_ = new QWidget;
    tabs_->AddTab(other_, QStringLiteral("Files"), QIcon(), QStringLiteral("Files"));
    ASSERT_TRUE(tabs_->EnableTab(other_));
    tabs_->SetMode(FancyTabWidget::Mode::LargeSidebar);
    tabs_->setCurrentIndex(0);

    menu_ = new QMenu(QStringLiteral("Display options"), &window_);
    group_by_menu_ = menu_->addMenu(QStringLiteral("Group by"));
    auto *grouping = new QActionGroup(group_by_menu_);
    for (const QString &text : {QStringLiteral("Album artist/Album"), QStringLiteral("Artist/Album"), QStringLiteral("Genre/Album")}) {
      QAction *action = grouping->addAction(text);
      action->setCheckable(true);
      group_by_menu_->addAction(action);
    }
    grouping->actions().at(0)->setChecked(true);
    menu_->addSeparator();

    auto *show_group = new QActionGroup(&window_);
    show_all_ = show_group->addAction(QStringLiteral("Show all songs"));
    show_duplicates_ = show_group->addAction(QStringLiteral("Show only duplicates"));
    for (QAction *action : show_group->actions()) {
      action->setCheckable(true);
      menu_->addAction(action);
    }
    show_all_->setChecked(true);
    menu_->addSeparator();
    configure_ = menu_->addAction(QStringLiteral("Configure collection..."));

    window_.show();
    QCoreApplication::processEvents();
  }

  QList<QAbstractButton*> VisibleButtons(MenuPage *page) {
    QList<QAbstractButton*> buttons;
    for (QAbstractButton *button : page->findChildren<QAbstractButton*>()) {
      if (button->isVisible() && button->parentWidget() != page) buttons << button;
    }
    return buttons;
  }

  QAbstractButton *FindButton(MenuPage *page, const QString &text) {
    for (QAbstractButton *button : VisibleButtons(page)) {
      if (ButtonText(button).contains(text) || button->accessibleName() == text) return button;
    }
    return nullptr;
  }

  QWidget window_;
  FancyTabWidget *tabs_ = nullptr;
  QWidget *content_ = nullptr;
  QWidget *other_ = nullptr;
  QPushButton *owner_ = nullptr;
  QMenu *menu_ = nullptr;
  QMenu *group_by_menu_ = nullptr;
  QAction *show_all_ = nullptr;
  QAction *show_duplicates_ = nullptr;
  QAction *configure_ = nullptr;
};

TEST_F(MenuPageTest, OpensInTabWithValuesAndRestoresContent) {
  MenuPage page(menu_, owner_);
  page.Open();
  QCoreApplication::processEvents();

  EXPECT_EQ(page.depth(), 1);
  EXPECT_EQ(page.title(), QStringLiteral("Collection > Display options"));
  EXPECT_TRUE(page.isVisible());
  EXPECT_FALSE(owner_->isVisible());
  EXPECT_EQ(page.geometry(), page.parentWidget()->rect());

  QAbstractButton *group_by = FindButton(&page, QStringLiteral("Group by"));
  ASSERT_NE(group_by, nullptr);
  EXPECT_EQ(ButtonText(group_by), QStringLiteral("Album artist/Album"));
  QAbstractButton *show = FindButton(&page, QStringLiteral("Show"));
  ASSERT_NE(show, nullptr);
  EXPECT_EQ(ButtonText(show), QStringLiteral("Show all songs"));
  EXPECT_NE(FindButton(&page, QStringLiteral("Configure collection >")), nullptr);
  for (QAbstractButton *button : VisibleButtons(&page)) EXPECT_GE(button->height(), AndroidWidgets::kMinimumTouchSize);

  page.Back();
  QCoreApplication::processEvents();
  EXPECT_EQ(page.depth(), 0);
  EXPECT_FALSE(page.isVisible());
  EXPECT_TRUE(owner_->isVisible());
}

TEST_F(MenuPageTest, ChoosingReturnsToParentWithNewValue) {
  MenuPage page(menu_, owner_);
  page.Open();
  QCoreApplication::processEvents();
  QSignalSpy triggered(group_by_menu_, &QMenu::triggered);

  QTest::mouseClick(FindButton(&page, QStringLiteral("Group by")), Qt::LeftButton);
  QCoreApplication::processEvents();
  EXPECT_EQ(page.depth(), 2);
  EXPECT_EQ(page.title(), QStringLiteral("Collection > Display options >\nGroup by"));
  auto *checked = qobject_cast<QRadioButton*>(FindButton(&page, QStringLiteral("Album artist/Album")));
  ASSERT_NE(checked, nullptr);
  EXPECT_TRUE(checked->isChecked());

  QTest::mouseClick(FindButton(&page, QStringLiteral("Genre/Album")), Qt::LeftButton);
  QCoreApplication::processEvents();
  EXPECT_EQ(triggered.count(), 1);
  EXPECT_TRUE(group_by_menu_->actions().at(2)->isChecked());
  EXPECT_EQ(page.depth(), 1);
  EXPECT_EQ(ButtonText(FindButton(&page, QStringLiteral("Group by"))), QStringLiteral("Genre/Album"));

  QTest::mouseClick(FindButton(&page, QStringLiteral("Show")), Qt::LeftButton);
  QCoreApplication::processEvents();
  EXPECT_EQ(page.title(), QStringLiteral("Collection > Display options >\nShow"));
  QTest::mouseClick(FindButton(&page, QStringLiteral("Show only duplicates")), Qt::LeftButton);
  QCoreApplication::processEvents();
  EXPECT_TRUE(show_duplicates_->isChecked());
  EXPECT_EQ(ButtonText(FindButton(&page, QStringLiteral("Show"))), QStringLiteral("Show only duplicates"));
}

TEST_F(MenuPageTest, BackKeyReturnsOneLevelWithoutChoosing) {
  MenuPage page(menu_, owner_);
  page.Open();
  QCoreApplication::processEvents();
  QTest::mouseClick(FindButton(&page, QStringLiteral("Group by")), Qt::LeftButton);
  QCoreApplication::processEvents();
  ASSERT_EQ(page.depth(), 2);

  QKeyEvent back(QEvent::KeyPress, Qt::Key_Back, Qt::NoModifier);
  QCoreApplication::sendEvent(QApplication::focusWidget() ? QApplication::focusWidget() : &page, &back);
  QCoreApplication::processEvents();
  EXPECT_EQ(page.depth(), 1);
  EXPECT_TRUE(group_by_menu_->actions().at(0)->isChecked());

  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QCoreApplication::sendEvent(&page, &escape);
  QCoreApplication::processEvents();
  EXPECT_EQ(page.depth(), 0);
  EXPECT_TRUE(owner_->isVisible());
}

TEST_F(MenuPageTest, ActionClosesPageBeforeTriggering) {
  MenuPage page(menu_, owner_);
  bool open_when_triggered = true;
  QObject::connect(configure_, &QAction::triggered, &page, [&]() { open_when_triggered = page.depth() > 0; });
  page.Open();
  QCoreApplication::processEvents();

  QTest::mouseClick(FindButton(&page, QStringLiteral("Configure collection >")), Qt::LeftButton);
  QCoreApplication::processEvents();
  EXPECT_FALSE(open_when_triggered);
  EXPECT_EQ(page.depth(), 0);
  EXPECT_TRUE(owner_->isVisible());
}

TEST_F(MenuPageTest, DisabledSubmenuIsNotSelectable) {
  group_by_menu_->setEnabled(false);
  MenuPage page(menu_, owner_);
  page.Open();
  QCoreApplication::processEvents();
  QAbstractButton *group_by = FindButton(&page, QStringLiteral("Group by"));
  ASSERT_NE(group_by, nullptr);
  EXPECT_FALSE(group_by->isEnabled());
}

TEST_F(MenuPageTest, LeavingOrTappingTabClosesPage) {
  MenuPage page(menu_, owner_);
  page.Open();
  QCoreApplication::processEvents();
  tabs_->setCurrentIndex(1);
  QCoreApplication::processEvents();
  EXPECT_EQ(page.depth(), 0);
  tabs_->setCurrentIndex(0);
  QCoreApplication::processEvents();
  EXPECT_TRUE(owner_->isVisible());

  page.Open();
  QCoreApplication::processEvents();
  Q_EMIT tabs_->tabBar()->tabBarClicked(0);
  QCoreApplication::processEvents();
  EXPECT_EQ(page.depth(), 0);
  EXPECT_TRUE(owner_->isVisible());
}

TEST_F(MenuPageTest, ReturningRestoresParentScrollPosition) {
  window_.resize(360, 300);
  for (int i = 0; i < 20; ++i) menu_->addAction(QStringLiteral("Action %1").arg(i));
  QMenu *last = menu_->addMenu(QStringLiteral("Last choices"));
  last->addAction(QStringLiteral("Choice"))->setCheckable(true);
  MenuPage page(menu_, owner_);
  page.Open();
  QCoreApplication::processEvents();

  QScrollArea *area = nullptr;
  for (QScrollArea *candidate : page.findChildren<QScrollArea*>()) {
    if (candidate->isVisible()) area = candidate;
  }
  ASSERT_NE(area, nullptr);
  ASSERT_GT(area->verticalScrollBar()->maximum(), 0);
  area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
  const int scroll = area->verticalScrollBar()->value();

  QAbstractButton *button = FindButton(&page, QStringLiteral("Last choices"));
  ASSERT_NE(button, nullptr);
  QTest::mouseClick(button, Qt::LeftButton);
  QCoreApplication::processEvents();
  ASSERT_EQ(page.depth(), 2);
  page.Back();
  QCoreApplication::processEvents();

  area = nullptr;
  for (QScrollArea *candidate : page.findChildren<QScrollArea*>()) {
    if (candidate->isVisible()) area = candidate;
  }
  ASSERT_NE(area, nullptr);
  EXPECT_EQ(area->verticalScrollBar()->value(), scroll);
}

}  // namespace
