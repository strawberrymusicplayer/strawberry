#include "gtest_include.h"

#include <QApplication>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QScroller>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTest>

#include "collection/collectionlettergroups_android.h"
#include "collection/collectionmodel.h"
#include "widgets/autoexpandingtreeview.h"
#include "widgets/listscrolling_android.h"

namespace {

QStandardItem *Item(const QString &text, const QString &key, const bool divider = false) {
  auto *item = new QStandardItem(text);
  item->setData(key, CollectionModel::Role_DividerKey);
  item->setData(divider, CollectionModel::Role_IsDivider);
  if (divider) item->setFlags(Qt::ItemIsEnabled | Qt::ItemNeverHasChildren);
  return item;
}

class CollectionLetterGroupsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    model_.appendRow(Item(QStringLiteral("A"), QStringLiteral("A"), true));
    auto *artist = Item(QStringLiteral("Artist"), QStringLiteral("A"));
    auto *album = new QStandardItem(QStringLiteral("Album"));
    album->appendRow(new QStandardItem(QStringLiteral("Song")));
    artist->appendRow(album);
    model_.appendRow(artist);
    model_.appendRow(Item(QStringLiteral("B"), QStringLiteral("B"), true));
    model_.appendRow(Item(QStringLiteral("Band"), QStringLiteral("B")));
    proxy_.setSourceModel(&model_);
    proxy_.sort(0);
    view_.setModel(&proxy_);
    view_.setAnimated(false);
    view_.resize(360, 500);
    view_.show();
    groups_.SetEnabled(true);
    QCoreApplication::processEvents();
    Tap(0);
    Tap(2);
  }

  void Tap(const int row) {
    QTest::mouseClick(view_.viewport(), Qt::LeftButton, Qt::NoModifier, view_.visualRect(proxy_.index(row, 0)).center());
    QCoreApplication::processEvents();
  }

  QStandardItemModel model_;
  QSortFilterProxyModel proxy_;
  AutoExpandingTreeView view_;
  CollectionLetterGroups groups_{&view_};
};

TEST_F(CollectionLetterGroupsTest, WholeDividerRowHidesOnlyItsGroupAndPreservesAlbumExpansion) {
  const QModelIndex artist = proxy_.index(1, 0);
  const QModelIndex album = proxy_.index(0, 0, artist);
  view_.expand(artist);
  view_.expand(album);
  Tap(0);
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_FALSE(view_.isRowHidden(0, QModelIndex()));
  EXPECT_FALSE(view_.isRowHidden(2, QModelIndex()));
  EXPECT_FALSE(view_.isRowHidden(3, QModelIndex()));
  EXPECT_TRUE(groups_.IsCollapsed(proxy_.index(0, 0)));
  Tap(0);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_TRUE(view_.isExpanded(artist));
  EXPECT_TRUE(view_.isExpanded(album));
}

TEST_F(CollectionLetterGroupsTest, DisablingImmediatelyRevealsGroupsAndLeavesOrdinaryArtistTapsAlone) {
  Tap(0);
  groups_.SetEnabled(false);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  Tap(0);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_FALSE(view_.isExpanded(proxy_.index(1, 0)));
  Tap(1);
  EXPECT_TRUE(view_.isExpanded(proxy_.index(1, 0)));
  groups_.SetEnabled(true);
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
}

TEST_F(CollectionLetterGroupsTest, GroupsStartCollapsedAndReloadingSettingsPreservesOpenedGroups) {
  groups_.SetEnabled(false);
  groups_.SetEnabled(true);
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_TRUE(view_.isRowHidden(3, QModelIndex()));
  Tap(0);
  groups_.SetEnabled(true);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_TRUE(view_.isRowHidden(3, QModelIndex()));
  model_.appendRow(Item(QStringLiteral("C"), QStringLiteral("C"), true));
  model_.appendRow(Item(QStringLiteral("Composer"), QStringLiteral("C")));
  QCoreApplication::processEvents();
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_TRUE(view_.isRowHidden(5, QModelIndex()));
}

TEST_F(CollectionLetterGroupsTest, NewArtistsAndSortingRemainInTheirCollapsedGroup) {
  Tap(0);
  model_.appendRow(Item(QStringLiteral("Another artist"), QStringLiteral("A")));
  QCoreApplication::processEvents();
  ASSERT_EQ(proxy_.rowCount(), 5);
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
  EXPECT_TRUE(view_.isRowHidden(2, QModelIndex()));
  EXPECT_FALSE(view_.isRowHidden(4, QModelIndex()));
  proxy_.sort(0, Qt::DescendingOrder);
  QCoreApplication::processEvents();
  EXPECT_FALSE(view_.isRowHidden(0, QModelIndex()));
  EXPECT_TRUE(view_.isRowHidden(2, QModelIndex()));
  EXPECT_TRUE(view_.isRowHidden(3, QModelIndex()));
  EXPECT_FALSE(view_.isRowHidden(4, QModelIndex()));
}

TEST_F(CollectionLetterGroupsTest, HiddenSelectionsAreClearedAndExternalSongFocusRevealsGroup) {
  const QModelIndex artist = proxy_.index(1, 0);
  const QModelIndex album = proxy_.index(0, 0, artist);
  const QModelIndex song = proxy_.index(0, 0, album);
  view_.expand(artist);
  view_.expand(album);
  view_.selectionModel()->select(song, QItemSelectionModel::Select);
  Tap(0);
  EXPECT_TRUE(view_.selectionModel()->selectedIndexes().isEmpty());
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
  view_.setCurrentIndex(song);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
}

TEST_F(CollectionLetterGroupsTest, FilteringRevealsMatchesAndRemovingDividerCannotHideAnArtist) {
  Tap(0);
  proxy_.setFilterFixedString(QStringLiteral("Artist"));
  QCoreApplication::processEvents();
  ASSERT_EQ(proxy_.rowCount(), 1);
  EXPECT_FALSE(view_.isRowHidden(0, QModelIndex()));
  proxy_.setFilterFixedString(QString());
  QCoreApplication::processEvents();
  ASSERT_EQ(proxy_.rowCount(), 4);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  Tap(0);
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
  model_.removeRow(0);
  QCoreApplication::processEvents();
  EXPECT_FALSE(view_.isRowHidden(0, QModelIndex()));
}

TEST_F(CollectionLetterGroupsTest, ResetAndModelReplacementDiscardOldHiddenRows) {
  Tap(0);
  model_.clear();
  model_.appendRow(Item(QStringLiteral("A"), QStringLiteral("A"), true));
  model_.appendRow(Item(QStringLiteral("Artist"), QStringLiteral("A")));
  QCoreApplication::processEvents();
  EXPECT_TRUE(view_.isRowHidden(1, QModelIndex()));
  Tap(0);
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  Tap(0);
  QStandardItemModel replacement;
  replacement.appendRow(Item(QStringLiteral("Artist"), QStringLiteral("A")));
  view_.hide();
  view_.setModel(&replacement);
  view_.show();
  QCoreApplication::processEvents();
  EXPECT_FALSE(view_.isRowHidden(0, QModelIndex()));
  view_.setModel(nullptr);
  groups_.SetEnabled(false);
}

TEST_F(CollectionLetterGroupsTest, FingerSwipeDoesNotToggleLetterGroup) {
  AndroidListScrolling scrolling(qApp);
  view_.hide();
  view_.show();
  for (int i = 0; i < 80; ++i) {
    model_.appendRow(Item(QStringLiteral("Artist %1").arg(i), QStringLiteral("A")));
  }
  QCoreApplication::processEvents();
  const QPoint start = view_.visualRect(proxy_.index(0, 0)).center();
  QTest::mousePress(view_.viewport(), Qt::LeftButton, Qt::NoModifier, start);
  for (int i = 1; i <= 8; ++i) {
    QTest::qWait(20);
    const QPoint position = start - QPoint(0, i * 18);
    QMouseEvent move(QEvent::MouseMove, position, view_.viewport()->mapToGlobal(position), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view_.viewport(), &move);
  }
  QTest::mouseRelease(view_.viewport(), Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 144));
  QTest::qWait(100);
  EXPECT_FALSE(groups_.IsCollapsed(proxy_.index(0, 0)));
  EXPECT_FALSE(view_.isRowHidden(1, QModelIndex()));
  QScroller::scroller(view_.viewport())->stop();
}

}
