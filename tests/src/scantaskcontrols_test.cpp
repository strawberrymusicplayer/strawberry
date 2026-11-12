// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include "core/taskmanager.h"
#include "widgets/scantaskcontrols_android.h"

TEST(ScanTaskControlsTest, OnlyOffersCancellationForScanTasks) {
  const auto manager = std::make_shared<TaskManager>();
  ScanTaskControls controls(manager);
  const int other = manager->StartTask(QStringLiteral("Other work"));
  EXPECT_TRUE(controls.isHidden());
  QSignalSpy cancelled(&*manager, &TaskManager::TaskCancellationRequested);
  manager->RequestTaskCancellation(other);
  EXPECT_EQ(cancelled.count(), 0);
  const int scan = manager->StartTask(QStringLiteral("Updating collection"));
  manager->SetTaskDetail(scan, QStringLiteral("Counting files: 123 entries found"));
  manager->SetTaskCancellable(scan);
  EXPECT_FALSE(controls.isHidden());
  auto *stop = controls.findChild<QPushButton*>(QStringLiteral("stop_scan"));
  auto *status = controls.findChild<QLabel*>(QStringLiteral("scan_status"));
  ASSERT_NE(stop, nullptr);
  ASSERT_NE(status, nullptr);
  EXPECT_GE(stop->minimumHeight(), 48);
  EXPECT_TRUE(status->text().contains(QStringLiteral("123")));
  stop->click();
  ASSERT_EQ(cancelled.count(), 1);
  EXPECT_EQ(cancelled.at(0).at(0).toInt(), scan);
  EXPECT_FALSE(stop->isEnabled());
  EXPECT_TRUE(status->text().contains(QStringLiteral("Stopping")));
  manager->RequestTaskCancellation(scan);
  EXPECT_EQ(cancelled.count(), 1);
  manager->SetTaskFinished(scan);
  EXPECT_TRUE(controls.isHidden());
  EXPECT_EQ(manager->GetTasks().size(), 1);
  manager->RequestTaskCancellation(scan);
  EXPECT_EQ(cancelled.count(), 1);
}

TEST(ScanTaskControlsTest, EstimatesOnlyAfterCountingAndCollectingScanSamples) {
  const auto manager = std::make_shared<TaskManager>();
  ScanTaskControls controls(manager);
  const int scan = manager->StartTask(QStringLiteral("Updating collection"));
  manager->SetTaskCancellable(scan);
  manager->SetTaskDetail(scan, QStringLiteral("Counting files..."));
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
  manager->SetTaskDetail(scan, QStringLiteral("Reading music information"));
  manager->SetTaskProgress(scan, 2, 100);
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
  manager->SetTaskProgress(scan, 25, 100);
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
  QTest::qWait(5100);
  const int remaining = manager->GetTasks().first().EstimatedSecondsRemaining();
  EXPECT_GE(remaining, 15);
  EXPECT_LE(remaining, 30);
  auto *status = controls.findChild<QLabel*>(QStringLiteral("scan_status"));
  ASSERT_NE(status, nullptr);
  manager->SetTaskProgress(scan, 25, 100);
  EXPECT_TRUE(status->text().contains(QStringLiteral("25 / 100")));
  EXPECT_TRUE(status->text().contains(QStringLiteral("about")));
  manager->SetTaskProgress(scan, 100, 100);
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
  EXPECT_TRUE(status->text().contains(QStringLiteral("finishing")));
  manager->RequestTaskCancellation(scan);
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
}

TEST(ScanTaskControlsTest, KeepsStatusAndStopButtonUsableAtPhoneWidth) {
  const auto manager = std::make_shared<TaskManager>();
  ScanTaskControls controls(manager);
  const int scan = manager->StartTask(QStringLiteral("Updating collection"));
  manager->SetTaskDetail(scan, QStringLiteral("Reading music information"));
  manager->SetTaskCancellable(scan);
  manager->SetTaskProgress(scan, 123456, 654321);
  controls.resize(240, controls.layout()->totalHeightForWidth(240));
  controls.show();
  QCoreApplication::processEvents();
  auto *stop = controls.findChild<QPushButton*>(QStringLiteral("stop_scan"));
  auto *status = controls.findChild<QLabel*>(QStringLiteral("scan_status"));
  ASSERT_NE(stop, nullptr);
  ASSERT_NE(status, nullptr);
  EXPECT_TRUE(status->wordWrap());
  EXPECT_TRUE(controls.rect().contains(stop->geometry()));
  EXPECT_TRUE(controls.rect().contains(status->geometry()));
  EXPECT_GT(stop->geometry().top(), status->geometry().bottom());
  EXPECT_GE(stop->height(), 48);
  manager->RequestTaskCancellation(scan);
  EXPECT_TRUE(status->text().contains(QStringLiteral("current operation")));
}
