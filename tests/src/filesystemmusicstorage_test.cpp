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
#include <QStorageInfo>
#include <QScopeGuard>

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
    job.original_root_ = source_path_;
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
  job.original_root_ = source_path_;
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

TEST_F(FilesystemMusicStorageTest, MoveRemovesEmptyDirectoriesUpToOriginalRoot) {

  // Reorganizing inside the collection directory removes the old empty directories, but not the collection directory itself, or the empty directories above it.
  ASSERT_TRUE(QDir(source_path_).removeRecursively());
  const QString original_filename = root_path_ + u"/Old Artist/Old Album/01.flac"_s;
  ASSERT_TRUE(QDir().mkpath(QFileInfo(original_filename).path()));
  ASSERT_TRUE(CreateFile(original_filename));

  MusicStorage::CopyJob job;
  job.source_ = original_filename;
  job.original_root_ = root_path_;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_FALSE(QDir(root_path_ + u"/Old Artist"_s).exists());
  EXPECT_TRUE(QDir(root_path_).exists());

  // Moving the last song out of the collection directory leaves the collection directory and the empty directories above it.
  const QString destination_path = temp_dir_.filePath(u"destination"_s);
  ASSERT_TRUE(QDir().mkpath(destination_path));
  MusicStorage::CopyJob job_out;
  job_out.source_ = root_path_ + u"/Artist/Album/01.flac"_s;
  job_out.original_root_ = root_path_;
  job_out.destination_ = u"01.flac"_s;
  job_out.remove_original_ = true;
  FilesystemMusicStorage storage_out(Song::Source::Collection, destination_path);
  ASSERT_TRUE(storage_out.CopyToStorage(job_out, error_text));

  EXPECT_TRUE(QFile::exists(destination_path + u"/01.flac"_s));
  EXPECT_FALSE(QDir(root_path_ + u"/Artist"_s).exists());
  EXPECT_TRUE(QDir(root_path_).exists());
  EXPECT_TRUE(QDir(temp_dir_.path()).exists());

}

TEST_F(FilesystemMusicStorageTest, MoveWithoutOriginalRootKeepsDirectories) {

  // A file outside the collection and device directories, like one in the downloads directory, doesn't remove any directories.
  const QString original_filename = source_path_ + u"/Album/01.flac"_s;
  ASSERT_TRUE(CreateFile(original_filename));

  MusicStorage::CopyJob job;
  job.source_ = original_filename;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_FALSE(QFile::exists(original_filename));
  EXPECT_TRUE(QDir(source_path_ + u"/Album"_s).exists());

}

#ifndef Q_OS_WIN32
TEST_F(FilesystemMusicStorageTest, MoveKeepsDirectoryOutsideOriginalRootLinkedFromInside) {

  // The album directory is outside the source root, but linked from inside it, so it's not removed.
  const QString external_path = temp_dir_.filePath(u"external"_s);
  ASSERT_TRUE(QDir().mkpath(external_path + u"/Album"_s));
  ASSERT_TRUE(QFile::link(external_path, source_path_ + u"/linked"_s));
  const QString original_filename = source_path_ + u"/linked/Album/01.flac"_s;
  ASSERT_TRUE(CreateFile(original_filename));

  MusicStorage::CopyJob job;
  job.source_ = original_filename;
  job.original_root_ = source_path_;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text));

  EXPECT_TRUE(QFile::exists(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_FALSE(QFile::exists(external_path + u"/Album/01.flac"_s));
  EXPECT_TRUE(QDir(external_path + u"/Album"_s).exists());

}
#endif

// A song moved to another filesystem is copied, then the original is removed.
TEST_F(FilesystemMusicStorageTest, MoveToOtherFilesystem) {

  QTemporaryDir other_temp_dir(u"/dev/shm/strawberry_test-XXXXXX"_s);
  if (!other_temp_dir.isValid() || QStorageInfo(other_temp_dir.path()).rootPath() == QStorageInfo(root_path_).rootPath()) {
    GTEST_SKIP() << "No other filesystem for the source.";
  }

  const QString source_path = other_temp_dir.filePath(u"Album"_s);
  ASSERT_TRUE(QDir().mkpath(source_path));
  ASSERT_TRUE(CreateFile(source_path + u"/01.flac"_s, QByteArray("song data")));
  const QFile::Permissions permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser | QFile::ReadGroup;
  ASSERT_TRUE(QFile::setPermissions(source_path + u"/01.flac"_s, permissions));
  // Not all permissions are supported on all platforms, so compare with the permissions the source has.
  const QFile::Permissions source_permissions = QFile::permissions(source_path + u"/01.flac"_s);

  MusicStorage::CopyJob job;
  job.source_ = source_path + u"/01.flac"_s;
  job.original_root_ = other_temp_dir.path();
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_EQ(source_permissions, QFile::permissions(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_FALSE(QFile::exists(source_path + u"/01.flac"_s));
  EXPECT_FALSE(QFileInfo::exists(source_path));
  // No temporary file of the copy is left.
  EXPECT_EQ(QStringList() << u"01.flac"_s, QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

}

// A link on the same filesystem as the destination is renamed, also when the file it links to is on another filesystem, it's not replaced with a copy of the file.
TEST_F(FilesystemMusicStorageTest, MoveLinkToFileOnOtherFilesystem) {

  QTemporaryDir other_temp_dir(u"/dev/shm/strawberry_test-XXXXXX"_s);
  if (!other_temp_dir.isValid() || QStorageInfo(other_temp_dir.path()).rootPath() == QStorageInfo(root_path_).rootPath()) {
    GTEST_SKIP() << "No other filesystem for the linked file.";
  }

  const QString target_filename = other_temp_dir.filePath(u"01.flac"_s);
  ASSERT_TRUE(CreateFile(target_filename, QByteArray("song data")));
  ASSERT_TRUE(QFile::link(target_filename, source_path_ + u"/Album/01.flac"_s));

  MusicStorage::CopyJob job;
  job.source_ = source_path_ + u"/Album/01.flac"_s;
  job.original_root_ = source_path_;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  const QFileInfo dest_fileinfo(root_path_ + u"/Artist/Album/01.flac"_s);
  EXPECT_TRUE(dest_fileinfo.isSymLink());
  EXPECT_EQ(target_filename, dest_fileinfo.symLinkTarget());
  EXPECT_TRUE(QFile::exists(target_filename));
  EXPECT_FALSE(QFileInfo(source_path_ + u"/Album/01.flac"_s).isSymLink());

}

// A copied song and its cover are copied to temporary files which are renamed, the source is kept, and no temporary file is left.
TEST_F(FilesystemMusicStorageTest, CopyKeepsSourceAndPermissions) {

  const QString filename = source_path_ + u"/Album/01.flac"_s;
  const QString cover_filename = source_path_ + u"/Album/cover.jpg"_s;
  ASSERT_TRUE(CreateFile(filename, QByteArray("song data")));
  ASSERT_TRUE(CreateFile(cover_filename, QByteArray("cover data")));
  const QFile::Permissions permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser | QFile::ReadGroup;
  ASSERT_TRUE(QFile::setPermissions(filename, permissions));
  // Not all permissions are supported on all platforms, so compare with the permissions the source has.
  const QFile::Permissions source_permissions = QFile::permissions(filename);

  MusicStorage::CopyJob job;
  job.source_ = filename;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.albumcover_ = true;
  job.cover_source_ = cover_filename;
  job.cover_dest_ = u"Artist/Album/cover.jpg"_s;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_EQ(source_permissions, QFile::permissions(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_EQ(QByteArray("cover data"), ReadFile(root_path_ + u"/Artist/Album/cover.jpg"_s));
  EXPECT_TRUE(QFile::exists(filename));
  EXPECT_TRUE(QFile::exists(cover_filename));
  EXPECT_EQ(QStringList() << u"01.flac"_s << u"cover.jpg"_s, QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name));

  // Copying again doesn't replace the copy.
  ASSERT_TRUE(CreateFile(filename, QByteArray("other song data")));
  EXPECT_FALSE(storage.CopyToStorage(job, error_text));
  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/Artist/Album/01.flac"_s));

}

// The filename is made from the tags, so it can be as long as the filesystem allows, the temporary file for copying it must not have a longer name.
TEST_F(FilesystemMusicStorageTest, CopyLongFilename) {

  const QString filename = source_path_ + u"/Album/01.flac"_s;
  ASSERT_TRUE(CreateFile(filename, QByteArray("song data")));
  // 250 bytes, the names are limited to 255 bytes on most filesystems.
  const QString long_filename = QString(80, QChar(0x97F3)) + u"long.flac"_s + QString(1, u'x');
  ASSERT_EQ(250, long_filename.toUtf8().size());

  MusicStorage::CopyJob job;
  job.source_ = filename;
  job.destination_ = u"Artist/Album/"_s + long_filename;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  ASSERT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/Artist/Album/"_s + long_filename));
  EXPECT_EQ(QStringList() << long_filename, QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

}

// When the song can't be renamed, and it's copied instead, but the original can't be removed, the copy is removed, and the original is kept.
TEST_F(FilesystemMusicStorageTest, MoveFromReadOnlyDirectoryKeepsOriginal) {

  const QString filename = source_path_ + u"/Album/01.flac"_s;
  ASSERT_TRUE(CreateFile(filename, QByteArray("song data")));
  const QString album_path = source_path_ + u"/Album"_s;
  const QFile::Permissions album_permissions = QFile::permissions(album_path);
  ASSERT_TRUE(QFile::setPermissions(album_path, QFile::ReadOwner | QFile::ExeOwner | QFile::ReadUser | QFile::ExeUser));
  const auto restore_permissions = qScopeGuard([&album_path, album_permissions]() { QFile::setPermissions(album_path, album_permissions); });
  {
    QFile file(album_path + u"/test"_s);
    if (file.open(QIODevice::WriteOnly)) {
      file.remove();
      GTEST_SKIP() << "The directory is still writable, the test is running as root.";
    }
  }

  MusicStorage::CopyJob job;
  job.source_ = filename;
  job.original_root_ = source_path_;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_FALSE(storage.CopyToStorage(job, error_text));

  EXPECT_EQ(QByteArray("song data"), ReadFile(filename));
  EXPECT_EQ(QStringList(), QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

}

// A song is never moved over an existing file, also not over a link to a file which doesn't exist, which is not seen as an existing destination before moving.
TEST_F(FilesystemMusicStorageTest, MoveDoesNotReplaceExistingLink) {

#ifdef Q_OS_WIN
  GTEST_SKIP() << "QFile::link() creates a shortcut with the .lnk suffix on Windows, not a link at the destination.";
#endif

  const QString filename = source_path_ + u"/Album/01.flac"_s;
  ASSERT_TRUE(CreateFile(filename, QByteArray("song data")));
  const QString dest_filename = root_path_ + u"/Artist/Album/01.flac"_s;
  ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
  ASSERT_TRUE(QFile::link(temp_dir_.filePath(u"missing.flac"_s), dest_filename));

  MusicStorage::CopyJob job;
  job.source_ = filename;
  job.original_root_ = source_path_;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_FALSE(storage.CopyToStorage(job, error_text));

  EXPECT_EQ(QByteArray("song data"), ReadFile(filename));
  EXPECT_TRUE(QFileInfo(dest_filename).isSymLink());
  EXPECT_EQ(QStringList() << u"01.flac"_s, QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot));

}

}  // namespace
