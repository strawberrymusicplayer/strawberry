/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "gtest_include.h"

#include "test_utils.h"
#include "core/mergedproxymodel.h"

#include <QStandardItemModel>
#include <QSignalSpy>

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression,function-args-by-value

class MergedProxyModelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    merged_.setSourceModel(&source_);
  }

  QStandardItemModel source_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  MergedProxyModel merged_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

};

TEST_F(MergedProxyModelTest, Flat) {

  source_.appendRow(new QStandardItem(u"one"_s));
  source_.appendRow(new QStandardItem(u"two"_s));

  ASSERT_EQ(2, merged_.rowCount(QModelIndex()));
  QModelIndex one_i = merged_.index(0, 0, QModelIndex());
  QModelIndex two_i = merged_.index(1, 0, QModelIndex());

  EXPECT_EQ(u"one"_s, one_i.data().toString());
  EXPECT_EQ(u"two"_s, two_i.data().toString());
  EXPECT_FALSE(merged_.parent(one_i).isValid());
  EXPECT_FALSE(merged_.hasChildren(one_i));

}

TEST_F(MergedProxyModelTest, Tree) {

  QStandardItem* one = new QStandardItem(u"one"_s);
  QStandardItem* two = new QStandardItem(u"two"_s);
  source_.appendRow(one);
  one->appendRow(two);

  ASSERT_EQ(1, merged_.rowCount(QModelIndex()));
  QModelIndex one_i = merged_.index(0, 0, QModelIndex());

  ASSERT_EQ(1, merged_.rowCount(one_i));
  QModelIndex two_i = merged_.index(0, 0, one_i);

  EXPECT_EQ(u"one"_s, one_i.data().toString());
  EXPECT_EQ(u"two"_s, two_i.data().toString());
  EXPECT_EQ(u"one"_s, two_i.parent().data().toString());

}

TEST_F(MergedProxyModelTest, Merged) {

  source_.appendRow(new QStandardItem(u"one"_s));

  QStandardItemModel submodel;
  submodel.appendRow(new QStandardItem(u"two"_s));

  merged_.AddSubModel(source_.index(0, 0, QModelIndex()), &submodel);

  ASSERT_EQ(1, merged_.rowCount(QModelIndex()));
  QModelIndex one_i = merged_.index(0, 0, QModelIndex());

  EXPECT_EQ(u"one"_s, merged_.data(one_i).toString());
  EXPECT_TRUE(merged_.hasChildren(one_i));

  ASSERT_EQ(1, merged_.rowCount(one_i));
  QModelIndex two_i = merged_.index(0, 0, one_i);

  EXPECT_EQ(u"two"_s, merged_.data(two_i).toString());
  EXPECT_EQ(0, merged_.rowCount(two_i));
  EXPECT_FALSE(merged_.hasChildren(two_i));

}

TEST_F(MergedProxyModelTest, SourceInsert) {

  QSignalSpy before_spy(&merged_, &MergedProxyModel::rowsAboutToBeInserted);
  QSignalSpy after_spy(&merged_, &MergedProxyModel::rowsInserted);

  source_.appendRow(new QStandardItem(u"one"_s));

  ASSERT_EQ(1, before_spy.count());
  ASSERT_EQ(1, after_spy.count());
  EXPECT_FALSE(before_spy[0][0].toModelIndex().isValid());
  EXPECT_EQ(0, before_spy[0][1].toInt());
  EXPECT_EQ(0, before_spy[0][2].toInt());
  EXPECT_FALSE(after_spy[0][0].toModelIndex().isValid());
  EXPECT_EQ(0, after_spy[0][1].toInt());
  EXPECT_EQ(0, after_spy[0][2].toInt());

}

TEST_F(MergedProxyModelTest, SourceRemove) {

  source_.appendRow(new QStandardItem(u"one"_s));

  QSignalSpy before_spy(&merged_, &MergedProxyModel::rowsAboutToBeRemoved);
  QSignalSpy after_spy(&merged_, &MergedProxyModel::rowsRemoved);

  source_.removeRow(0, QModelIndex());

  ASSERT_EQ(1, before_spy.count());
  ASSERT_EQ(1, after_spy.count());
  EXPECT_FALSE(before_spy[0][0].toModelIndex().isValid());
  EXPECT_EQ(0, before_spy[0][1].toInt());
  EXPECT_EQ(0, before_spy[0][2].toInt());
  EXPECT_FALSE(after_spy[0][0].toModelIndex().isValid());
  EXPECT_EQ(0, after_spy[0][1].toInt());
  EXPECT_EQ(0, after_spy[0][2].toInt());

}

TEST_F(MergedProxyModelTest, SubInsert) {

  source_.appendRow(new QStandardItem(u"one"_s));
  QStandardItemModel submodel;
  merged_.AddSubModel(source_.index(0, 0, QModelIndex()), &submodel);

  QSignalSpy before_spy(&merged_, &MergedProxyModel::rowsAboutToBeInserted);
  QSignalSpy after_spy(&merged_, &MergedProxyModel::rowsInserted);

  submodel.appendRow(new QStandardItem(u"two"_s));

  ASSERT_EQ(1, before_spy.count());
  ASSERT_EQ(1, after_spy.count());
  EXPECT_EQ(u"one"_s, before_spy[0][0].toModelIndex().data());
  EXPECT_EQ(0, before_spy[0][1].toInt());
  EXPECT_EQ(0, before_spy[0][2].toInt());
  EXPECT_EQ(u"one"_s, after_spy[0][0].toModelIndex().data());
  EXPECT_EQ(0, after_spy[0][1].toInt());
  EXPECT_EQ(0, after_spy[0][2].toInt());

}

TEST_F(MergedProxyModelTest, SubRemove) {

  source_.appendRow(new QStandardItem(u"one"_s));
  QStandardItemModel submodel;
  merged_.AddSubModel(source_.index(0, 0, QModelIndex()), &submodel);

  submodel.appendRow(new QStandardItem(u"two"_s));

  QSignalSpy before_spy(&merged_, &MergedProxyModel::rowsAboutToBeRemoved);
  QSignalSpy after_spy(&merged_, &MergedProxyModel::rowsRemoved);

  submodel.removeRow(0, QModelIndex());

  ASSERT_EQ(1, before_spy.count());
  ASSERT_EQ(1, after_spy.count());
  EXPECT_EQ(u"one"_s, before_spy[0][0].toModelIndex().data());
  EXPECT_EQ(0, before_spy[0][1].toInt());
  EXPECT_EQ(0, before_spy[0][2].toInt());
  EXPECT_EQ(u"one"_s, after_spy[0][0].toModelIndex().data());
  EXPECT_EQ(0, after_spy[0][1].toInt());
  EXPECT_EQ(0, after_spy[0][2].toInt());

}
