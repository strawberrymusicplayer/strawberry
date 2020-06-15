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

#include <gtest/gtest.h>

#include <QTemporaryFile>
#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include "test_utils.h"

#include "playlist/songplaylistitem.h"

namespace {

class SongPlaylistItemTest : public ::testing::TestWithParam<const char*> {
 protected:
  SongPlaylistItemTest() : temp_file_(GetParam()) {}

  void SetUp() override {
    // SongPlaylistItem::Url() checks if the file exists, so we need a real file
    temp_file_.open();

    absolute_file_name_ = QFileInfo(temp_file_.fileName()).absoluteFilePath();

    song_.Init("Title", "Artist", "Album", 123);
    song_.set_url(QUrl::fromLocalFile(absolute_file_name_));

    item_.reset(new SongPlaylistItem(song_));

    if (!absolute_file_name_.startsWith('/'))
      absolute_file_name_.prepend('/');
  }

  Song song_;
  QTemporaryFile temp_file_;
  QString absolute_file_name_;
  std::unique_ptr<SongPlaylistItem> item_;
};

INSTANTIATE_TEST_CASE_P(RealFiles, SongPlaylistItemTest, testing::Values(
    "normalfile.flac",
    "file with spaces.flac",
    "file with # hash.flac",
    "file with ? question.flac"
));

TEST_P(SongPlaylistItemTest, Url) {
  QUrl expected;
  expected.setScheme("file");
  expected.setPath(absolute_file_name_);

  EXPECT_EQ(expected, item_->Url());
}


}  //namespace

