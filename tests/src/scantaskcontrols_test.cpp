// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QProgressBar>
#include "core/taskmanager.h"
#include "widgets/scantaskcontrols_android.h"

TEST(ScanTaskControlsTest, ShowsScanStatusWithoutOfferingHeaderCancellation) {
  const auto manager = std::make_shared<TaskManager>();
  ScanTaskControls controls(manager);
  const int other = manager->StartTask(QStringLiteral("Other work"));
  EXPECT_TRUE(controls.findChildren<QPushButton*>().isEmpty());
  auto *progress = controls.findChild<QProgressBar*>(QStringLiteral("scan_progress"));
  ASSERT_NE(progress, nullptr);
  EXPECT_FALSE(progress->isVisibleTo(&controls));
  QSignalSpy cancelled(&*manager, &TaskManager::TaskCancellationRequested);
  manager->RequestTaskCancellation(other);
  EXPECT_EQ(cancelled.count(), 0);
  const int scan = manager->StartTask(QStringLiteral("Updating collection"));
  manager->SetTaskDetail(scan, QStringLiteral("Counting files: 123 entries found"));
  manager->SetTaskCancellable(scan);
  EXPECT_TRUE(progress->isVisibleTo(&controls));
  auto *status = controls.findChild<QLabel*>(QStringLiteral("scan_status"));
  ASSERT_NE(status, nullptr);
  EXPECT_TRUE(controls.findChildren<QPushButton*>().isEmpty());
  EXPECT_TRUE(status->text().contains(QStringLiteral("123")));
  manager->RequestTaskCancellation(scan);
  ASSERT_EQ(cancelled.count(), 1);
  EXPECT_EQ(cancelled.at(0).at(0).toInt(), scan);
  EXPECT_TRUE(status->text().contains(QStringLiteral("Stopping")));
  manager->RequestTaskCancellation(scan);
  EXPECT_EQ(cancelled.count(), 1);
  manager->SetTaskFinished(scan);
  EXPECT_FALSE(progress->isVisibleTo(&controls));
  EXPECT_FALSE(controls.findChild<QLabel*>(QStringLiteral("app_title"))->isHidden());
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
  manager->SetTaskDetail(scan, QStringLiteral("Processing"));
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
  EXPECT_TRUE(status->text().contains(QStringLiteral("Processing - 25/100 - ")));
  EXPECT_FALSE(status->text().contains(QStringLiteral("entries")));
  EXPECT_TRUE(status->text().contains(QStringLiteral("Updating collection 25%")));
  EXPECT_EQ(controls.findChild<QProgressBar*>(QStringLiteral("scan_progress"))->value(), 25);
  EXPECT_TRUE(status->text().contains(QStringLiteral("about")));
  manager->SetTaskProgress(scan, 100, 100);
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
  EXPECT_TRUE(status->text().contains(QStringLiteral("finishing")));
  manager->RequestTaskCancellation(scan);
  EXPECT_EQ(manager->GetTasks().first().EstimatedSecondsRemaining(), -1);
}

TEST(ScanTaskControlsTest, KeepsStatusReadableAtPhoneWidth) {
  const auto manager = std::make_shared<TaskManager>();
  ScanTaskControls controls(manager);
  const int scan = manager->StartTask(QStringLiteral("Updating collection"));
  manager->SetTaskDetail(scan, QStringLiteral("Processing"));
  manager->SetTaskCancellable(scan);
  controls.resize(240, controls.layout()->totalHeightForWidth(240));
  controls.show();
  QCoreApplication::processEvents();
  EXPECT_EQ(controls.width(), 240);
  auto *status = controls.findChild<QLabel*>(QStringLiteral("scan_status"));
  ASSERT_NE(status, nullptr);
  EXPECT_TRUE(controls.findChildren<QPushButton*>().isEmpty());
  manager->SetTaskProgress(scan, 123456, 654321);
  QCoreApplication::processEvents();
  controls.resize(240, controls.layout()->totalHeightForWidth(240));
  QCoreApplication::processEvents();
  EXPECT_TRUE(status->wordWrap());
  const QRect status_rect(status->mapTo(&controls, QPoint()), status->size());
  EXPECT_TRUE(controls.rect().contains(status_rect));
  EXPECT_EQ(status->alignment(), Qt::AlignCenter);
  EXPECT_GE(status->height(), status->heightForWidth(status->width()));
  controls.resize(640, controls.layout()->totalHeightForWidth(640));
  QCoreApplication::processEvents();
  EXPECT_TRUE(controls.rect().contains(QRect(status->mapTo(&controls, QPoint()), status->size())));
  EXPECT_GE(status->height(), status->heightForWidth(status->width()));
  manager->RequestTaskCancellation(scan);
  EXPECT_TRUE(status->text().contains(QStringLiteral("current operation")));
}

TEST(ScanTaskControlsTest, ShowsUnknownProgressUntilAllScansHaveTotals) {
  const auto manager = std::make_shared<TaskManager>();
  ScanTaskControls controls(manager);
  const int first = manager->StartTask(QStringLiteral("First scan"));
  const int second = manager->StartTask(QStringLiteral("Second scan"));
  manager->SetTaskCancellable(first);
  manager->SetTaskCancellable(second);
  manager->SetTaskProgress(first, 50, 100);
  auto *progress = controls.findChild<QProgressBar*>(QStringLiteral("scan_progress"));
  ASSERT_NE(progress, nullptr);
  EXPECT_EQ(progress->maximum(), 0);
  manager->SetTaskProgress(second, 100, 200);
  EXPECT_EQ(progress->maximum(), 100);
  EXPECT_EQ(progress->value(), 50);
  QSignalSpy cancelled(&*manager, &TaskManager::TaskCancellationRequested);
  manager->RequestTaskCancellation(first);
  manager->RequestTaskCancellation(second);
  EXPECT_EQ(cancelled.count(), 2);
  manager->SetTaskFinished(first);
  EXPECT_TRUE(progress->isVisibleTo(&controls));
  manager->SetTaskFinished(second);
  EXPECT_FALSE(progress->isVisibleTo(&controls));
}
