/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>

#include "includes/shared_ptr.h"
#include "core/musicstorage.h"
#include "core/song.h"
#include "device/deviceview.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class TestStorage : public MusicStorage {
 public:
  Song::Source source() const override { return Song::Source::Device; }
  bool CopyToStorage(const CopyJob &job, QString &error_text) override {
    Q_UNUSED(job)
    Q_UNUSED(error_text)
    return true;
  }
  bool DeleteFromStorage(const DeleteJob &job) override {
    Q_UNUSED(job)
    return true;
  }
};

Song MakeSong(const QString &title) {
  Song song;
  song.Init(title, u"Artist"_s, u"Album"_s, 123);
  return song;
}

// Regression test: the songs selected from more than one device were all deleted from the device of the first selected song.
TEST(DeviceViewTest, SongsAreGroupedByTheirStorage) {

  const SharedPtr<MusicStorage> storage1 = make_shared<TestStorage>();
  const SharedPtr<MusicStorage> storage2 = make_shared<TestStorage>();

  DeviceView::StorageSongsList list;
  DeviceView::AddStorageSongs(&list, storage1, SongList() << MakeSong(u"First"_s));
  DeviceView::AddStorageSongs(&list, storage2, SongList() << MakeSong(u"Second"_s));
  DeviceView::AddStorageSongs(&list, storage1, SongList() << MakeSong(u"Third"_s));

  ASSERT_EQ(2, list.count());
  EXPECT_EQ(storage1, list[0].storage);
  ASSERT_EQ(2, list[0].songs.count());
  EXPECT_EQ(u"First"_s, list[0].songs[0].title());
  EXPECT_EQ(u"Third"_s, list[0].songs[1].title());
  EXPECT_EQ(storage2, list[1].storage);
  ASSERT_EQ(1, list[1].songs.count());
  EXPECT_EQ(u"Second"_s, list[1].songs[0].title());

}

TEST(DeviceViewTest, SongsWithoutStorageAreIgnored) {

  DeviceView::StorageSongsList list;
  DeviceView::AddStorageSongs(&list, SharedPtr<MusicStorage>(), SongList() << MakeSong(u"First"_s));

  EXPECT_TRUE(list.isEmpty());

}

}  // namespace
