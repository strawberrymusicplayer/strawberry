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

#include "fileview/fileview.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

TEST(FileViewTest, PathIsInsideRoot) {

  EXPECT_TRUE(FileView::PathIsInsideRoot(u"/home/user/Music"_s, u"/home/user/Music"_s));
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"/home/user/Music/Artist/Album"_s, u"/home/user/Music"_s));
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"/home/user/Music/Artist"_s, u"/home/user/Music/"_s));
  EXPECT_FALSE(FileView::PathIsInsideRoot(u"/home/user/MusicVideos"_s, u"/home/user/Music"_s));
  EXPECT_FALSE(FileView::PathIsInsideRoot(u"/home/user"_s, u"/home/user/Music"_s));
  EXPECT_FALSE(FileView::PathIsInsideRoot(QString(), u"/home/user/Music"_s));

}

TEST(FileViewTest, PathIsInsideRootDirectory) {

  // The root directory ends with a separator already.
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"/"_s, u"/"_s));
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"/home/user"_s, u"/"_s));

}

TEST(FileViewTest, PathIsInsideRootWindowsPaths) {

  // QDir::cleanPath() converts the backslashes to '/' on Windows, so compare the paths as they are after that on all platforms.
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"C:/Users/user/Music/Artist"_s, u"C:/Users/user/Music"_s));
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"C:/Users/user/Music"_s, u"C:/"_s));
  EXPECT_FALSE(FileView::PathIsInsideRoot(u"C:/Users/user/MusicVideos"_s, u"C:/Users/user/Music"_s));
#ifdef Q_OS_WIN32
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"C:\\Users\\user\\Music\\Artist"_s, u"C:\\Users\\user\\Music"_s));
  EXPECT_TRUE(FileView::PathIsInsideRoot(u"c:/users/user/music/Artist"_s, u"C:/Users/user/Music"_s));
#endif

}

}  // namespace
