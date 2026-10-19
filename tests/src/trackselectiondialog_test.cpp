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
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QFileDevice>
#include <QIODevice>
#include <QDateTime>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QListWidget>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSignalSpy>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "songeditor/trackselectiondialog.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

Song SongWithFile(const QString &filename, const QString &title) {
  Song song;
  song.set_url(QUrl::fromLocalFile(filename));
  song.set_title(title);
  return song;
}

// The same file can be selected twice, for example when it's in the playlist twice, the results are for all its rows.
TEST(TrackSelectionDialogTest, ResultsForFileSelectedTwice) {

  TrackSelectionDialog dialog(make_shared<TagReaderClient>());
  QListWidget *song_list = dialog.findChild<QListWidget*>(u"song_list"_s);
  QStackedWidget *stacked_widget = dialog.findChild<QStackedWidget*>(u"stacked_widget"_s);
  ASSERT_TRUE(song_list);
  ASSERT_TRUE(stacked_widget);

  const Song song = SongWithFile(u"/music/01.flac"_s, u"Title"_s);
  const Song other_song = SongWithFile(u"/music/02.flac"_s, u"Other"_s);
  dialog.Init(SongList() << song << other_song << song);
  ASSERT_EQ(3, song_list->count());

  dialog.FetchTagFinished(song, SongList() << SongWithFile(u"/music/01.flac"_s, u"Guessed title"_s));

  // Both rows of the file have the results, the other file is still loading.
  song_list->setCurrentRow(0);
  EXPECT_EQ(u"results_page"_s, stacked_widget->currentWidget()->objectName());
  song_list->setCurrentRow(2);
  EXPECT_EQ(u"results_page"_s, stacked_widget->currentWidget()->objectName());
  song_list->setCurrentRow(1);
  EXPECT_EQ(u"loading_page"_s, stacked_widget->currentWidget()->objectName());

  EXPECT_EQ(dialog.palette().text(), song_list->item(0)->foreground());
  EXPECT_EQ(dialog.palette().text(), song_list->item(2)->foreground());

}

QTreeWidgetItem *ResultItem(QTreeWidget *results, const int result_index) {

  for (int i = 0; i < results->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = results->topLevelItem(i);
    const QVariant id = item->data(0, Qt::UserRole);
    if (!id.isNull() && id.toInt() == result_index) {
      return item;
    }
  }

  return nullptr;

}

// The result selected in one row of a file is selected in all its rows, and the file is only chosen once.
TEST(TrackSelectionDialogTest, ResultSelectedForFileSelectedTwice) {

  TrackSelectionDialog dialog(make_shared<TagReaderClient>());
  QListWidget *song_list = dialog.findChild<QListWidget*>(u"song_list"_s);
  QTreeWidget *results = dialog.findChild<QTreeWidget*>(u"results"_s);
  ASSERT_TRUE(song_list);
  ASSERT_TRUE(results);

  const Song song = SongWithFile(u"/music/01.flac"_s, u"Title"_s);
  dialog.Init(SongList() << song << song);
  dialog.FetchTagFinished(song, SongList() << SongWithFile(u"/music/01.flac"_s, u"First guess"_s) << SongWithFile(u"/music/01.flac"_s, u"Second guess"_s));

  // Select the second result in the first row.
  song_list->setCurrentRow(0);
  QTreeWidgetItem *second_result = ResultItem(results, 1);
  ASSERT_TRUE(second_result);
  results->setCurrentItem(second_result);

  // The second row shows the same result as selected.
  song_list->setCurrentRow(1);
  ASSERT_TRUE(results->currentItem());
  EXPECT_EQ(1, results->currentItem()->data(0, Qt::UserRole).toInt());

  QSignalSpy spy_song_chosen(&dialog, &TrackSelectionDialog::SongChosen);
  dialog.accept();
  ASSERT_EQ(1, spy_song_chosen.count());
  EXPECT_EQ(u"Second guess"_s, spy_song_chosen.at(0).at(1).value<Song>().title());

}

// Selecting the original tags in one row of a file means its other rows don't change it either.
TEST(TrackSelectionDialogTest, OriginalTagsSelectedForFileSelectedTwice) {

  TrackSelectionDialog dialog(make_shared<TagReaderClient>());
  QListWidget *song_list = dialog.findChild<QListWidget*>(u"song_list"_s);
  QTreeWidget *results = dialog.findChild<QTreeWidget*>(u"results"_s);
  ASSERT_TRUE(song_list);
  ASSERT_TRUE(results);

  const Song song = SongWithFile(u"/music/01.flac"_s, u"Title"_s);
  dialog.Init(SongList() << song << song);
  dialog.FetchTagFinished(song, SongList() << SongWithFile(u"/music/01.flac"_s, u"Guess"_s));

  song_list->setCurrentRow(0);
  QTreeWidgetItem *original_tags = ResultItem(results, -1);
  ASSERT_TRUE(original_tags);
  results->setCurrentItem(original_tags);

  QSignalSpy spy_song_chosen(&dialog, &TrackSelectionDialog::SongChosen);
  dialog.accept();
  EXPECT_EQ(0, spy_song_chosen.count());

}

// Deleting the dialog while the tags are saved in the background, for example when quitting, waits for all the tags to be written.
TEST(TrackSelectionDialogTest, DeletingWhileSavingWritesAllTags) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  SharedPtr<TagReaderClient> tagreader_client = make_shared<TagReaderClient>();

  SongList songs;
  for (int i = 0; i < 50; ++i) {
    const QString filename = temp_dir.filePath(QStringLiteral("%1.mp3").arg(i));
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner));
    Song song(Song::Source::LocalFile);
    ASSERT_TRUE(tagreader_client->ReadFileBlocking(filename, &song).success());
    songs << song;
  }

  // The files have an old modification time, to see which files were saved when the dialog was deleted.
  const QDateTime old_mtime = QDateTime::currentDateTimeUtc().addDays(-1);
  for (const Song &song : std::as_const(songs)) {
    QFile file(song.url().toLocalFile());
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_TRUE(file.setFileTime(old_mtime, QFileDevice::FileModificationTime));
  }

  TrackSelectionDialog *dialog = new TrackSelectionDialog(tagreader_client);
  dialog->set_save_on_close(true);
  dialog->Init(songs);
  for (const Song &song : std::as_const(songs)) {
    Song guessed_song(song);
    guessed_song.set_title(u"Guessed title"_s);
    dialog->FetchTagFinished(song, SongList() << guessed_song);
  }
  dialog->accept();
  delete dialog;

  // All the files were saved when deleting the dialog returned, this is checked first, since saving would continue while the tags are read.
  int files_saved = 0;
  for (const Song &song : std::as_const(songs)) {
    if (QFileInfo(song.url().toLocalFile()).lastModified() > old_mtime) ++files_saved;
  }
  EXPECT_EQ(songs.count(), files_saved);

  // The pool thread can still finish its own bookkeeping after saving finished.
  EXPECT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));
  for (const Song &song : std::as_const(songs)) {
    Song saved_song(Song::Source::LocalFile);
    ASSERT_TRUE(tagreader_client->ReadFileBlocking(song.url().toLocalFile(), &saved_song).success());
    EXPECT_EQ(u"Guessed title"_s, saved_song.title());
  }

}

}  // namespace
