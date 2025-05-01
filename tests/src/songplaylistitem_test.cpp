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

#include <memory>

#include "gtest_include.h"

#include <QTemporaryFile>
#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include "test_utils.h"

#include "includes/scoped_ptr.h"
#include "playlist/songplaylistitem.h"

using std::make_unique;

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

class SongPlaylistItemTest : public ::testing::TestWithParam<const char*> {
 protected:
  SongPlaylistItemTest() : temp_file_(QString::fromUtf8(GetParam())) {}

  void SetUp() override {
    // SongPlaylistItem::Url() checks if the file exists, so we need a real file
    EXPECT_TRUE(temp_file_.open());

    absolute_file_name_ = QFileInfo(temp_file_.fileName()).absoluteFilePath();

    song_.Init(u"Title"_s, u"Artist"_s, u"Album"_s, 123);
    song_.set_url(QUrl::fromLocalFile(absolute_file_name_));

    item_ = make_unique<SongPlaylistItem>(song_);

    if (!absolute_file_name_.startsWith(u'/'))
      absolute_file_name_.prepend(u'/');
  }

  Song song_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QTemporaryFile temp_file_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString absolute_file_name_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  ScopedPtr<SongPlaylistItem> item_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
};

INSTANTIATE_TEST_SUITE_P(RealFiles, SongPlaylistItemTest, testing::Values(  // clazy:exclude=function-args-by-value,clazy-non-pod-global-static
    "normalfile.flac",
    "file with spaces.flac",
    "file with # hash.flac",
    "file with ? question.flac"
));

TEST_P(SongPlaylistItemTest, Url) {
  QUrl expected;
  expected.setScheme(u"file"_s);
  expected.setPath(absolute_file_name_);

  EXPECT_EQ(expected, item_->OriginalUrl());
}


}  //namespace

