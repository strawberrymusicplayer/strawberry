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

#include "config.h"

#include "gtest_include.h"

#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "transcoder/transcodedialog.h"

using namespace Qt::Literals::StringLiterals;

namespace {

QStringList ListedFilenames(const TranscodeDialog &dialog) {

  QStringList filenames;
  const QTreeWidget *files = dialog.findChild<QTreeWidget*>();
  if (!files) return filenames;
  for (int i = 0; i < files->topLevelItemCount(); ++i) {
    filenames << files->topLevelItem(i)->data(0, Qt::UserRole).toString();
  }
  return filenames;

}

}  // namespace

// Regression test: the tracks of a CUE sheet are in one audio file, and the file was added once for each track, so the whole file was transcoded once for each track.
TEST(TranscodeDialogTest, FileAddedOnce) {

  TranscodeDialog dialog(nullptr);
  dialog.SetFilenames(QStringList() << u"/music/album.flac"_s << u"/music/album.flac"_s << u"/music/song.flac"_s);
  EXPECT_EQ(QStringList() << u"/music/album.flac"_s << u"/music/song.flac"_s, ListedFilenames(dialog));

  // Files which are already listed aren't added again.
  dialog.SetFilenames(QStringList() << u"/music/song.flac"_s << u"/music/other.flac"_s);
  EXPECT_EQ(QStringList() << u"/music/album.flac"_s << u"/music/song.flac"_s << u"/music/other.flac"_s, ListedFilenames(dialog));

}
