// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <memory>

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QScroller>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QTest>
#include <QVBoxLayout>

#include "collection/collectionlettergroups_android.h"
#include "collection/collectionmodel.h"
#include "collection/collectionselection_android.h"
#include "core/mimedata.h"
#include "widgets/autoexpandingtreeview.h"
#include "widgets/listscrolling_android.h"

namespace {

class SelectionModel : public QStandardItemModel {
 public:
  QMimeData *mimeData(const QModelIndexList &indexes) const override {
    requested = indexes;
    return valid ? new MimeData : new QMimeData;
  }

  mutable QModelIndexList requested;
  bool valid = true;
};

class CollectionSelectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto *layout = new QVBoxLayout(&window_);
    layout->addWidget(new QLineEdit(&window_));
    view_ = new AutoExpandingTreeView(&window_);
    view_->setAnimated(false);
    view_->setHeaderHidden(true);
    view_->setStyleSheet(QStringLiteral("QTreeView::item { min-height: 48px; }"));
    layout->addWidget(view_);
    for (int i = 0; i < 80; ++i) {
      auto *artist = new QStandardItem(QStringLiteral("Artist %1").arg(i));
      auto *album = new QStandardItem(QStringLiteral("Album"));
      album->appendRow(new QStandardItem(QStringLiteral("Song")));
      artist->appendRow(album);
      model_.appendRow(artist);
    }
    view_->setModel(&model_);
    selection_ = new CollectionSelection(view_, &window_);
    layout->insertWidget(0, selection_->toolbar());
    QObject::connect(selection_, &CollectionSelection::AddToPlaylist, &window_, [this](QMimeData *data) { data_.reset(qobject_cast<MimeData*>(data)); });
    window_.resize(360, 640);
    window_.show();
    QCoreApplication::processEvents();
  }

  void TearDown() override {
    QScroller::scroller(view_->viewport())->stop();
    // Destroy the view while the model and application adapter still exist.
    delete view_;
    view_ = nullptr;
  }

  QPoint Point(const QModelIndex &index, const bool checkbox = false) {
    const QRect rect = view_->visualRect(index);
    return QPoint(checkbox ? rect.left() + 16 : rect.right() - 30, rect.center().y());
  }

  void Tap(const QModelIndex &index, const bool checkbox = false) {
    QTest::mouseClick(view_->viewport(), Qt::LeftButton, Qt::NoModifier, Point(index, checkbox));
    QCoreApplication::processEvents();
  }

  void Hold(const QModelIndex &index) {
    const QPoint point = Point(index);
    QTest::mousePress(view_->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QTest::qWait(LongPressInterval());
    QTest::mouseRelease(view_->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QCoreApplication::processEvents();
  }

  int LongPressInterval() const { return QApplication::styleHints()->mousePressAndHoldInterval() + 100; }

  void Drag(const QPoint &start, const QPoint &delta, const int steps = 8) {
    QTest::mousePress(view_->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    for (int i = 1; i <= steps; ++i) {
      QTest::qWait(30);
      const QPoint point = start + delta * i / steps;
      QMouseEvent move(QEvent::MouseMove, point, view_->viewport()->mapToGlobal(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(view_->viewport(), &move);
    }
    QTest::mouseRelease(view_->viewport(), Qt::LeftButton, Qt::NoModifier, start + delta);
    QTest::qWait(80);
  }

  QPushButton *Button(QWidget *parent, const QString &text) {
    for (QPushButton *button : parent->findChildren<QPushButton*>()) if (button->text() == text) return button;
    return nullptr;
  }

  QDialog *OpenActions() {
    QPushButton *actions = Button(selection_->toolbar(), QStringLiteral("Actions"));
    if (!actions) return nullptr;
    QTest::mouseClick(actions, Qt::LeftButton);
    QCoreApplication::processEvents();
    for (QDialog *dialog : view_->findChildren<QDialog*>()) if (dialog->isVisible()) return dialog;
    return nullptr;
  }

  AndroidListScrolling scrolling_{qApp};
  SelectionModel model_;
  QWidget window_;
  AutoExpandingTreeView *view_ = nullptr;
  CollectionSelection *selection_ = nullptr;
  std::unique_ptr<MimeData> data_;
};

TEST_F(CollectionSelectionTest, PressAndNormalTapNeverSelectButTapExpands) {
  const QModelIndex artist = model_.index(0, 0);
  QTest::mousePress(view_->viewport(), Qt::LeftButton, Qt::NoModifier, Point(artist));
  EXPECT_FALSE(view_->selectionModel()->hasSelection());
  QTest::mouseRelease(view_->viewport(), Qt::LeftButton, Qt::NoModifier, Point(artist));
  EXPECT_TRUE(view_->isExpanded(artist));
  EXPECT_EQ(view_->currentIndex(), artist);
  EXPECT_FALSE(view_->selectionModel()->hasSelection());
  EXPECT_FALSE(selection_->active());
}

TEST_F(CollectionSelectionTest, LongPressEntersOnceAndCheckboxesSelectWithoutExpanding) {
  const QModelIndex artist = model_.index(0, 0);
  Hold(artist);
  ASSERT_TRUE(selection_->active());
  EXPECT_TRUE(view_->hasFocus());
  EXPECT_TRUE(selection_->toolbar()->isVisible());
  EXPECT_TRUE(view_->selectionModel()->isSelected(artist));
  EXPECT_FALSE(view_->isExpanded(artist));
  const QModelIndex second = model_.index(1, 0);
  Tap(second, true);
  EXPECT_TRUE(view_->selectionModel()->isSelected(second));
  EXPECT_FALSE(view_->isExpanded(second));
  EXPECT_EQ(selection_->toolbar()->findChild<QLabel*>()->text(), QStringLiteral("2 item(s) selected"));
  Tap(artist, true);
  EXPECT_FALSE(view_->selectionModel()->isSelected(artist));
  EXPECT_TRUE(view_->selectionModel()->isSelected(second));
  Tap(second);
  EXPECT_TRUE(view_->isExpanded(second));
  EXPECT_TRUE(view_->selectionModel()->isSelected(second));
  EXPECT_TRUE(selection_->Exit());
  EXPECT_FALSE(selection_->Exit());
  EXPECT_FALSE(view_->selectionModel()->hasSelection());
  EXPECT_FALSE(selection_->toolbar()->isVisible());
}

TEST_F(CollectionSelectionTest, SlowDiagonalAndShortDragsNeverSelectOrExpand) {
  QSignalSpy expanded(view_, &QTreeView::expanded);
  Drag(QPoint(180, 300), QPoint(-30, -180));
  EXPECT_GT(view_->verticalScrollBar()->value(), 0);
  EXPECT_FALSE(view_->selectionModel()->hasSelection());
  EXPECT_FALSE(selection_->active());
  EXPECT_EQ(expanded.count(), 0);
  QScroller::scroller(view_->viewport())->stop();
  Drag(QPoint(180, 200), QPoint(5, -20), 4);
  QTest::qWait(LongPressInterval());
  EXPECT_FALSE(selection_->active());
  EXPECT_FALSE(view_->selectionModel()->hasSelection());
  EXPECT_EQ(expanded.count(), 0);
}

TEST_F(CollectionSelectionTest, ScrollingAndMomentumStopPreserveSelection) {
  Hold(model_.index(0, 0));
  ASSERT_TRUE(selection_->active());
  Drag(QPoint(180, 300), QPoint(0, -180));
  EXPECT_GT(view_->verticalScrollBar()->value(), 0);
  EXPECT_EQ(view_->selectionModel()->selectedRows(), QModelIndexList({model_.index(0, 0)}));
  EXPECT_EQ(QScroller::scroller(view_->viewport())->state(), QScroller::Scrolling);
  QTest::mouseClick(view_->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 150));
  EXPECT_EQ(view_->selectionModel()->selectedRows(), QModelIndexList({model_.index(0, 0)}));
}

TEST_F(CollectionSelectionTest, CancellationAndHidingCannotEnterSelectionLater) {
  QTest::mousePress(view_->viewport(), Qt::LeftButton, Qt::NoModifier, Point(model_.index(0, 0)));
  QEvent cancel(QEvent::TouchCancel);
  QApplication::sendEvent(view_->viewport(), &cancel);
  QTest::qWait(LongPressInterval());
  QTest::mouseRelease(view_->viewport(), Qt::LeftButton);
  EXPECT_FALSE(selection_->active());
  QTest::mousePress(view_->viewport(), Qt::LeftButton, Qt::NoModifier, Point(model_.index(0, 0)));
  view_->hide();
  QTest::qWait(LongPressInterval());
  EXPECT_FALSE(selection_->active());
}

TEST_F(CollectionSelectionTest, BackDoneAndModelResetClearMode) {
  Hold(model_.index(0, 0));
  QTest::keyClick(view_, Qt::Key_Back);
  EXPECT_FALSE(selection_->active());
  Hold(model_.index(0, 0));
  auto *done = Button(selection_->toolbar(), QStringLiteral("Done"));
  ASSERT_NE(done, nullptr);
  QTest::mouseClick(done, Qt::LeftButton);
  EXPECT_FALSE(selection_->active());
  EXPECT_TRUE(view_->hasFocus());
  Hold(model_.index(0, 0));
  ASSERT_TRUE(selection_->active());
  model_.clear();
  EXPECT_FALSE(selection_->active());
  EXPECT_FALSE(selection_->toolbar()->isVisible());
}

TEST_F(CollectionSelectionTest, AllPlaylistActionsUseNormalizedSelectionAndExplicitFlags) {
  const QModelIndex artist = model_.index(0, 0);
  const QModelIndex album = model_.index(0, 0, artist);
  Hold(artist);
  view_->selectionModel()->select(album, QItemSelectionModel::Select | QItemSelectionModel::Rows);
  EXPECT_EQ(selection_->SelectedIndexes(), QModelIndexList({artist}));
  struct Action {
    const char *label;
    bool create;
    bool play;
    bool enqueue;
    bool next;
  };
  for (const Action &action : {Action{"Add to current playlist", false, false, false, false},
                              Action{"Queue", false, false, true, false},
                              Action{"Queue to play next", false, false, false, true},
                              Action{"Create saved playlist", true, false, false, false},
                              Action{"Create and play", true, true, false, false}}) {
    QDialog *dialog = OpenActions();
    ASSERT_NE(dialog, nullptr);
    auto *button = Button(dialog, QString::fromLatin1(action.label));
    ASSERT_NE(button, nullptr);
    QTest::mouseClick(button, Qt::LeftButton);
    ASSERT_NE(data_, nullptr);
    EXPECT_EQ(model_.requested, QModelIndexList({artist}));
    EXPECT_TRUE(data_->override_user_settings_);
    EXPECT_EQ(data_->open_in_new_playlist_, action.create);
    EXPECT_EQ(data_->play_now_, action.play);
    EXPECT_EQ(data_->enqueue_now_, action.enqueue);
    EXPECT_EQ(data_->enqueue_next_now_, action.next);
    EXPECT_TRUE(selection_->active());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
}

TEST_F(CollectionSelectionTest, InvalidMimeDataReportsFailureAndRemovingRowsClosesActions) {
  Hold(model_.index(0, 0));
  QSignalSpy errors(selection_, &CollectionSelection::Error);
  model_.valid = false;
  QDialog *dialog = OpenActions();
  ASSERT_NE(dialog, nullptr);
  auto *button = Button(dialog, QStringLiteral("Add to current playlist"));
  ASSERT_NE(button, nullptr);
  QTest::mouseClick(button, Qt::LeftButton);
  EXPECT_EQ(errors.count(), 1);
  EXPECT_EQ(data_, nullptr);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  dialog = OpenActions();
  ASSERT_NE(dialog, nullptr);
  model_.removeRow(0);
  EXPECT_FALSE(dialog->isVisible());
  EXPECT_TRUE(selection_->SelectedIndexes().isEmpty());
}

TEST_F(CollectionSelectionTest, EmptySelectionDisablesActionsAndBackFromActionsKeepsSelection) {
  const QModelIndex artist = model_.index(0, 0);
  Hold(artist);
  QDialog *dialog = OpenActions();
  ASSERT_NE(dialog, nullptr);
  EXPECT_TRUE(selection_->Back());
  EXPECT_FALSE(dialog->isVisible());
  EXPECT_TRUE(selection_->active());
  EXPECT_TRUE(view_->selectionModel()->isSelected(artist));
  EXPECT_EQ(window_.focusWidget(), view_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  dialog = OpenActions();
  ASSERT_NE(dialog, nullptr);
  QTest::keyClick(dialog, Qt::Key_Back);
  EXPECT_FALSE(dialog->isVisible());
  EXPECT_TRUE(selection_->active());
  EXPECT_EQ(window_.focusWidget(), view_);
  Tap(artist, true);
  EXPECT_TRUE(selection_->SelectedIndexes().isEmpty());
  auto *actions = Button(selection_->toolbar(), QStringLiteral("Actions"));
  ASSERT_NE(actions, nullptr);
  EXPECT_FALSE(actions->isEnabled());
}

TEST_F(CollectionSelectionTest, RescanIsOnlyAvailableForEditableSelection) {
  const QModelIndex artist = model_.index(0, 0);
  Hold(artist);
  QDialog *dialog = OpenActions();
  ASSERT_NE(dialog, nullptr);
  EXPECT_EQ(Button(dialog, QStringLiteral("Rescan selected music")), nullptr);
  dialog->reject();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  model_.setData(artist, true, CollectionModel::Role_Editable);
  QSignalSpy rescan(selection_, &CollectionSelection::RescanSongs);
  dialog = OpenActions();
  ASSERT_NE(dialog, nullptr);
  auto *button = Button(dialog, QStringLiteral("Rescan selected music"));
  ASSERT_NE(button, nullptr);
  QTest::mouseClick(button, Qt::LeftButton);
  ASSERT_EQ(rescan.count(), 1);
  EXPECT_EQ(rescan.first().first().value<QModelIndexList>(), QModelIndexList({artist}));
}

TEST_F(CollectionSelectionTest, LetterGroupTapsStillWorkAndDividersCannotBeSelected) {
  selection_->Exit();
  // Install groups before selection, as in CollectionView.
  delete selection_;
  CollectionLetterGroups groups(view_);
  auto *divider = new QStandardItem(QStringLiteral("A"));
  divider->setData(true, CollectionModel::Role_IsDivider);
  divider->setData(QStringLiteral("A"), CollectionModel::Role_DividerKey);
  divider->setFlags(Qt::ItemIsEnabled | Qt::ItemNeverHasChildren);
  model_.insertRow(0, divider);
  model_.item(1)->setData(QStringLiteral("A"), CollectionModel::Role_DividerKey);
  groups.SetEnabled(true);
  selection_ = new CollectionSelection(view_, &window_);
  window_.layout()->addWidget(selection_->toolbar());
  QCoreApplication::processEvents();
  EXPECT_TRUE(view_->isRowHidden(1, QModelIndex()));
  Tap(model_.index(0, 0));
  EXPECT_FALSE(view_->isRowHidden(1, QModelIndex()));
  Hold(model_.index(0, 0));
  EXPECT_FALSE(selection_->active());
  Hold(model_.index(1, 0));
  EXPECT_TRUE(selection_->active());
  Tap(model_.index(0, 0));
  EXPECT_TRUE(view_->isRowHidden(1, QModelIndex()));
  EXPECT_TRUE(selection_->SelectedIndexes().isEmpty());
}

}  // namespace
