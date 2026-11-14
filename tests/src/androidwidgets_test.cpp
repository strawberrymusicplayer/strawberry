// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QApplication>
#include <QAccessible>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionGroupBox>
#include <QTest>
#include <QVBoxLayout>

#include "widgets/androidwidgets.h"

using namespace Qt::Literals::StringLiterals;

TEST(AndroidWidgetsTest, CheckBoxHasReferenceIndicatorAndFullRowTouchTarget) {

  QCheckBox check(u"Enable"_s);
  AndroidWidgets::AdaptCheckBox(&check);
  check.resize(240, AndroidWidgets::kCheckBoxMinimumHeight);
  check.show();
  QApplication::processEvents();
  QStyleOptionButton option;
  option.initFrom(&check);
  EXPECT_EQ(QSize(28, 28), check.style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, &check).size());
  EXPECT_GE(check.minimumHeight(), 56);
  QSignalSpy toggled(&check, &QCheckBox::toggled);
  QSignalSpy clicked(&check, &QCheckBox::clicked);
  QTest::mouseClick(&check, Qt::LeftButton, Qt::NoModifier, QPoint(check.width() - 3, check.height() - 3));
  EXPECT_TRUE(check.isChecked());
  EXPECT_EQ(1, toggled.count());
  EXPECT_EQ(1, clicked.count());
  EXPECT_EQ(u"Enable"_s, check.text());

}

TEST(AndroidWidgetsTest, CheckBoxWrapsAtNarrowWidthsAndLargerFonts) {

  QCheckBox check(u"Enable this setting and preserve its complete explanatory label"_s);
  AndroidWidgets::AdaptCheckBox(&check);
  QFont font = check.font();
  font.setPointSizeF(font.pointSizeF() * 1.5);
  check.setFont(font);
  check.resize(220, 56);
  check.show();
  QApplication::processEvents();
  auto *label = check.findChild<QLabel*>();
  ASSERT_NE(nullptr, label);
  EXPECT_TRUE(label->wordWrap());
  EXPECT_GT(check.minimumHeight(), 56);
  EXPECT_GE(check.height(), check.layout()->totalHeightForWidth(check.width()));
  EXPECT_EQ(check.text(), label->text());
  check.setLayoutDirection(Qt::RightToLeft);
  QApplication::processEvents();
  EXPECT_GE(check.layout()->contentsMargins().right(), 36);
  EXPECT_EQ(8, check.layout()->contentsMargins().left());

}

TEST(AndroidWidgetsTest, CheckBoxPreservesDisabledTristateAndKeyboardBehavior) {

  QCheckBox check(u"State"_s);
  check.setTristate(true);
  check.setCheckState(Qt::PartiallyChecked);
  AndroidWidgets::AdaptCheckBox(&check);
  check.resize(240, 56);
  check.show();
  QApplication::processEvents();
  QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(&check);
  ASSERT_NE(nullptr, accessible);
  EXPECT_EQ(QAccessible::CheckBox, accessible->role());
  EXPECT_EQ(u"State"_s, accessible->text(QAccessible::Name));
  EXPECT_TRUE(accessible->state().checkStateMixed);
  check.setEnabled(false);
  QTest::mouseClick(&check, Qt::LeftButton, Qt::NoModifier, QPoint(230, 28));
  EXPECT_EQ(Qt::PartiallyChecked, check.checkState());
  EXPECT_TRUE(accessible->state().disabled);
  check.setEnabled(true);
  QTest::mouseClick(&check, Qt::LeftButton, Qt::NoModifier, QPoint(230, 28));
  EXPECT_EQ(Qt::Checked, check.checkState());
  EXPECT_TRUE(accessible->state().checked);
  EXPECT_FALSE(accessible->state().checkStateMixed);
  QTest::keyClick(&check, Qt::Key_Space);
  EXPECT_EQ(Qt::Unchecked, check.checkState());

}

TEST(AndroidWidgetsTest, AdaptationIsIdempotent) {

  QCheckBox check(u"Setting"_s);
  AndroidWidgets::AdaptCheckBox(&check);
  AndroidWidgets::AdaptCheckBox(&check);
  EXPECT_EQ(1, check.findChildren<QLabel*>().count());
  check.setText(u"Updated setting"_s);
  check.show();
  QApplication::processEvents();
  EXPECT_EQ(u"Updated setting"_s, check.findChild<QLabel*>()->text());

}

TEST(AndroidWidgetsTest, CheckBoxPreservesMnemonicsAndExistingLargerTargets) {

  QCheckBox check(u"Rock && roll &mode"_s);
  check.setMinimumHeight(80);
  AndroidWidgets::AdaptCheckBox(&check);
  check.resize(240, 80);
  check.show();
  QApplication::processEvents();
  EXPECT_EQ(u"Rock & roll mode"_s, check.findChild<QLabel*>()->text());
  EXPECT_GE(check.minimumHeight(), 80);
  QTest::mousePress(&check, Qt::LeftButton, Qt::NoModifier, QPoint(230, 28));
  QTest::mouseRelease(&check, Qt::LeftButton, Qt::NoModifier, QPoint(250, 28));
  EXPECT_FALSE(check.isChecked());
  QTest::mousePress(&check, Qt::LeftButton, Qt::NoModifier, QPoint(-10, 28));
  QTest::mouseRelease(&check, Qt::LeftButton, Qt::NoModifier, QPoint(230, 28));
  EXPECT_FALSE(check.isChecked());

}

TEST(AndroidWidgetsTest, CheckableSectionHasLargeIndicatorAndFullHeaderTarget) {

  QGroupBox group(u"Use authentication"_s);
  group.setCheckable(true);
  auto *layout = new QVBoxLayout(&group);
  auto *child = new QCheckBox(u"Child"_s, &group);
  layout->addWidget(child);
  AndroidWidgets::AdaptCheckableGroupBox(&group);
  group.resize(240, 180);
  group.show();
  QApplication::processEvents();
  QStyleOptionGroupBox option;
  option.initFrom(&group);
  option.text = group.title();
  option.subControls = QStyle::SC_GroupBoxCheckBox | QStyle::SC_GroupBoxLabel;
  EXPECT_EQ(QSize(28, 28), group.style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxCheckBox, &group).size());
  EXPECT_GE(group.contentsRect().top(), 56);
  QFont font = group.font();
  font.setPointSizeF(font.pointSizeF() * 1.5);
  group.setFont(font);
  QApplication::processEvents();
  EXPECT_GE(group.contentsRect().top(), group.fontMetrics().height() + 28);
  QTest::mousePress(&group, Qt::LeftButton, Qt::NoModifier, QPoint(-10, 28));
  QTest::mouseRelease(&group, Qt::LeftButton, Qt::NoModifier, QPoint(235, 28));
  EXPECT_TRUE(group.isChecked());
  QSignalSpy toggled(&group, &QGroupBox::toggled);
  QSignalSpy clicked(&group, &QGroupBox::clicked);
  QTest::mouseClick(&group, Qt::LeftButton, Qt::NoModifier, QPoint(235, 28));
  EXPECT_FALSE(group.isChecked());
  EXPECT_FALSE(child->isEnabled());
  EXPECT_EQ(1, toggled.count());
  EXPECT_EQ(1, clicked.count());
  QTest::mouseClick(&group, Qt::LeftButton, Qt::NoModifier, QPoint(235, 28));
  EXPECT_TRUE(group.isChecked());
  EXPECT_TRUE(child->isEnabled());

}

TEST(AndroidWidgetsTest, ApplicationAdapterCoversDynamicallyCreatedControls) {

  AndroidWidgets::InitializeCheckBoxes(qApp);
  QCheckBox check(u"Dynamic"_s);
  check.ensurePolished();
  EXPECT_TRUE(check.property("androidTouchCheckBox").toBool());
  EXPECT_GE(check.minimumHeight(), 56);
  QGroupBox group(u"Section"_s);
  group.setCheckable(true);
  group.ensurePolished();
  EXPECT_TRUE(group.property("androidTouchGroupBox").toBool());

}
