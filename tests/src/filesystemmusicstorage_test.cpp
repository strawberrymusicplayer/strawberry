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

#include <QByteArray>
#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTemporaryDir>

#include "core/song.h"
#include "core/musicstorage.h"
#include "core/filesystemmusicstorage.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

class FilesystemMusicStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    source_path_ = temp_dir_.filePath(u"source"_s);
    root_path_ = temp_dir_.filePath(u"collection"_s);
    ASSERT_TRUE(QDir().mkpath(source_path_ + u"/Album"_s));
    ASSERT_TRUE(QDir().mkpath(root_path_));
  }

  static bool CreateFile(const QString &filename, const QByteArray &data = QByteArray("data")) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const bool success = file.write(data) == data.size();
    file.close();
    return success;
  }

  static QByteArray ReadFile(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
  }

  // Moves a song from the source album directory to Artist/Album in the collection, with the given cover.
  bool MoveSong(const QString &filename, const QString &cover_filename) {
    MusicStorage::CopyJob job;
    job.source_ = source_path_ + u"/Album/"_s + filename;
    job.destination_ = u"Artist/Album/"_s + filename;
    job.remove_original_ = true;
    job.albumcover_ = true;
    job.cover_source_ = cover_filename;
    job.cover_dest_ = u"Artist/Album/"_s + QFileInfo(cover_filename).fileName();
    FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
    QString error_text;
    return storage.CopyToStorage(job, error_text);
  }

  QTemporaryDir temp_dir_;
  QString source_path_;
  QString root_path_;
};

TEST_F(FilesystemMusicStorageTest, MoveKeepsCoverSharedWithOtherSongs) {

  const QString cover_filename = source_path_ + u"/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/01.flac"_s));
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/02.flac"_s));
  ASSERT_TRUE(CreateFile(cover_filename));

  ASSERT_TRUE(MoveSong(u"01.flac"_s, cover_filename));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_FALSE(QFile::exists(source_path_ + u"/Album/01.flac"_s));
  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/cover.jpg"_s));
  // The song left behind still has its cover.
  EXPECT_TRUE(QFile::exists(cover_filename));

}

TEST_F(FilesystemMusicStorageTest, MoveLastSongRemovesCoverAndDirectory) {

  const QString cover_filename = source_path_ + u"/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/01.flac"_s));
  ASSERT_TRUE(CreateFile(cover_filename));

  ASSERT_TRUE(MoveSong(u"01.flac"_s, cover_filename));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/cover.jpg"_s));
  EXPECT_FALSE(QFile::exists(cover_filename));
  EXPECT_FALSE(QDir(source_path_ + u"/Album"_s).exists());

}

TEST_F(FilesystemMusicStorageTest, MoveLastSongRemovesCoverCopiedByPreviousSong) {

  const QString cover_filename = source_path_ + u"/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/01.flac"_s));
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/02.flac"_s));
  ASSERT_TRUE(CreateFile(cover_filename));

  // The first song copies the cover, the last song finds the same cover in the destination.
  ASSERT_TRUE(MoveSong(u"01.flac"_s, cover_filename));
  ASSERT_TRUE(MoveSong(u"02.flac"_s, cover_filename));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/cover.jpg"_s));
  EXPECT_FALSE(QFile::exists(cover_filename));
  EXPECT_FALSE(QDir(source_path_ + u"/Album"_s).exists());

}

TEST_F(FilesystemMusicStorageTest, MoveLastSongKeepsCoverWhenDestinationHasOtherCover) {

  const QString cover_filename = source_path_ + u"/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/01.flac"_s));
  ASSERT_TRUE(CreateFile(cover_filename, QByteArray("source cover")));

  // The destination already has another cover with the same filename, which is not overwritten.
  ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
  ASSERT_TRUE(CreateFile(root_path_ + u"/Artist/Album/cover.jpg"_s, QByteArray("destination cover")));

  ASSERT_TRUE(MoveSong(u"01.flac"_s, cover_filename));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_EQ(QByteArray("destination cover"), ReadFile(root_path_ + u"/Artist/Album/cover.jpg"_s));
  // The cover was not copied, so it's kept.
  EXPECT_EQ(QByteArray("source cover"), ReadFile(cover_filename));

}

TEST_F(FilesystemMusicStorageTest, MoveTranscodedRemovesOriginal) {

  // The source is the transcoded temporary file, the original is removed instead of it.
  const QString transcode_path = temp_dir_.filePath(u"cache/organize"_s);
  ASSERT_TRUE(QDir().mkpath(transcode_path));
  const QString transcoded_filename = transcode_path + u"/01.mp3"_s;
  const QString original_filename = source_path_ + u"/Album/01.flac"_s;
  const QString cover_filename = source_path_ + u"/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(transcoded_filename, QByteArray("transcoded")));
  ASSERT_TRUE(CreateFile(original_filename, QByteArray("original")));
  ASSERT_TRUE(CreateFile(cover_filename));

  MusicStorage::CopyJob job;
  job.source_ = transcoded_filename;
  job.original_source_ = original_filename;
  job.destination_ = u"Artist/Album/01.mp3"_s;
  job.remove_original_ = true;
  job.albumcover_ = true;
  job.cover_source_ = cover_filename;
  job.cover_dest_ = u"Artist/Album/cover.jpg"_s;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text));

  EXPECT_EQ(QByteArray("transcoded"), ReadFile(root_path_ + u"/Artist/Album/01.mp3"_s));
  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/cover.jpg"_s));
  EXPECT_FALSE(QFile::exists(original_filename));
  // The directory of the original is removed, not the directory of the transcoded file.
  EXPECT_FALSE(QDir(source_path_ + u"/Album"_s).exists());
  EXPECT_TRUE(QDir(transcode_path).exists());

}

TEST_F(FilesystemMusicStorageTest, MoveKeepsCoverOutsideSongDirectory) {

  // A cover in the cache is shared with all the songs of the album.
  const QString cache_path = temp_dir_.filePath(u"cache"_s);
  ASSERT_TRUE(QDir().mkpath(cache_path));
  const QString cover_filename = cache_path + u"/0123456789abcdef.jpg"_s;
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/01.flac"_s));
  ASSERT_TRUE(CreateFile(cover_filename));

  ASSERT_TRUE(MoveSong(u"01.flac"_s, cover_filename));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/0123456789abcdef.jpg"_s));
  EXPECT_TRUE(QFile::exists(cover_filename));

}

TEST_F(FilesystemMusicStorageTest, MoveInSameDirectoryWithOverwriteKeepsCover) {

  ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
  const QString cover_filename = root_path_ + u"/Artist/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(root_path_ + u"/Artist/Album/01.flac"_s));
  ASSERT_TRUE(CreateFile(cover_filename));

  // The cover is already the cover in the destination directory, overwriting it would remove it.
  MusicStorage::CopyJob job;
  job.source_ = root_path_ + u"/Artist/Album/01.flac"_s;
  job.destination_ = u"Artist/Album/01 Title.flac"_s;
  job.remove_original_ = true;
  job.overwrite_ = true;
  job.albumcover_ = true;
  job.cover_source_ = cover_filename;
  job.cover_dest_ = u"Artist/Album/cover.jpg"_s;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01 Title.flac"_s));
  EXPECT_TRUE(QFile::exists(cover_filename));

}

}  // namespace
