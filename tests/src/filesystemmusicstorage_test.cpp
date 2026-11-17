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

#include <memory>

#include <QtGlobal>

#ifdef Q_OS_UNIX
#  include <sys/stat.h>
#  include <unistd.h>
#endif

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
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

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

// On a case-sensitive filesystem, a destination which only differs in case is another file, which is overwritten.
TEST_F(FilesystemMusicStorageTest, MoveWithOverwriteReplacesFileWithOtherCase) {

  // Only the case of a file in the collection directory is changed, since directories above it can be on other filesystems.
  ASSERT_TRUE(CreateFile(root_path_ + u"/case"_s));
  if (QFile::exists(root_path_ + u"/CASE"_s)) {
    GTEST_SKIP() << "The filesystem is not case-sensitive.";
  }
  ASSERT_TRUE(QFile::remove(root_path_ + u"/case"_s));

  ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
  ASSERT_TRUE(CreateFile(root_path_ + u"/Artist/Album/Song.flac"_s, QByteArray("new data")));
  ASSERT_TRUE(CreateFile(root_path_ + u"/Artist/Album/song.flac"_s, QByteArray("old data")));

  MusicStorage::CopyJob job;
  job.source_ = root_path_ + u"/Artist/Album/Song.flac"_s;
  job.original_root_ = root_path_;
  job.destination_ = u"Artist/Album/song.flac"_s;
  job.overwrite_ = true;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("new data"), ReadFile(root_path_ + u"/Artist/Album/song.flac"_s));
  EXPECT_EQ(QStringList() << u"song.flac"_s, QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

}

// A destination which is a link to the same file under another name is overwritten.
TEST_F(FilesystemMusicStorageTest, MoveWithOverwriteReplacesHardLinkToSameFile) {

#ifndef Q_OS_UNIX
  GTEST_SKIP() << "Hard links are created with link().";
#else
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/01.flac"_s, QByteArray("song data")));
  ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
  ASSERT_EQ(0, ::link(QFile::encodeName(source_path_ + u"/Album/01.flac"_s).constData(), QFile::encodeName(root_path_ + u"/Artist/Album/01.flac"_s).constData()));

  MusicStorage::CopyJob job;
  job.source_ = source_path_ + u"/Album/01.flac"_s;
  job.original_root_ = source_path_;
  job.destination_ = u"Artist/Album/01.flac"_s;
  job.overwrite_ = true;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/Artist/Album/01.flac"_s));
  EXPECT_FALSE(QFile::exists(source_path_ + u"/Album/01.flac"_s));
#endif

}

#ifdef Q_OS_UNIX
// The source is a link to the destination, so the destination is the file itself, it's not removed for overwriting.
TEST_F(FilesystemMusicStorageTest, MoveLinkToItsTargetKeepsFile) {

  ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
  ASSERT_TRUE(CreateFile(root_path_ + u"/Artist/Album/song.flac"_s, QByteArray("song data")));
  ASSERT_TRUE(QFile::link(root_path_ + u"/Artist/Album/song.flac"_s, root_path_ + u"/Artist/Album/link.flac"_s));

  MusicStorage::CopyJob job;
  job.source_ = root_path_ + u"/Artist/Album/link.flac"_s;
  job.original_root_ = root_path_;
  job.destination_ = u"Artist/Album/song.flac"_s;
  job.overwrite_ = true;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/Artist/Album/song.flac"_s));
  EXPECT_FALSE(QFileInfo(root_path_ + u"/Artist/Album/song.flac"_s).isSymLink());

}
#endif

#ifdef Q_OS_UNIX
// Hard links which only differ in case on a case-sensitive filesystem, Song.flac is the source and song.flac the destination.
class FilesystemMusicStorageHardLinkWithOtherCaseTest : public FilesystemMusicStorageTest {
 protected:
  void SetUp() override {
    FilesystemMusicStorageTest::SetUp();
    // Only the case of a file in the collection directory is changed, since directories above it can be on other filesystems.
    ASSERT_TRUE(CreateFile(root_path_ + u"/case"_s));
    if (QFile::exists(root_path_ + u"/CASE"_s)) {
      GTEST_SKIP() << "The filesystem is not case-sensitive.";
    }
    ASSERT_TRUE(QFile::remove(root_path_ + u"/case"_s));
    ASSERT_TRUE(QDir().mkpath(root_path_ + u"/Artist/Album"_s));
    ASSERT_TRUE(CreateFile(SourceFilename(), QByteArray("song data")));
    ASSERT_EQ(0, ::link(QFile::encodeName(SourceFilename()).constData(), QFile::encodeName(DestinationFilename()).constData()));
  }

  QString SourceFilename() const { return root_path_ + u"/Artist/Album/Song.flac"_s; }
  QString DestinationFilename() const { return root_path_ + u"/Artist/Album/song.flac"_s; }

  bool CopySong(const bool remove_original, const bool overwrite) {
    MusicStorage::CopyJob job;
    job.source_ = SourceFilename();
    job.original_root_ = root_path_;
    job.destination_ = u"Artist/Album/song.flac"_s;
    job.overwrite_ = overwrite;
    job.remove_original_ = remove_original;
    FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
    QString error_text;
    return storage.CopyToStorage(job, error_text);
  }

  static ino_t Inode(const QString &filename) {
    struct stat file_stat {};
    if (::stat(QFile::encodeName(filename).constData(), &file_stat) != 0) return 0;
    return file_stat.st_ino;
  }

  QStringList AlbumEntries() const {
    return QDir(root_path_ + u"/Artist/Album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
  }
};

// Regression test: the hard links were taken as the same directory entry, so copying reported success without copying.
TEST_F(FilesystemMusicStorageHardLinkWithOtherCaseTest, CopyWithoutOverwriteFails) {

  EXPECT_FALSE(CopySong(false, false));

}

// Regression test: the hard links were taken as the same directory entry, so the destination was left linked to the source.
TEST_F(FilesystemMusicStorageHardLinkWithOtherCaseTest, CopyWithOverwriteReplacesDestination) {

  EXPECT_TRUE(CopySong(false, true));

  EXPECT_EQ(QByteArray("song data"), ReadFile(DestinationFilename()));
  EXPECT_NE(Inode(SourceFilename()), Inode(DestinationFilename()));

}

// Regression test: the hard links were taken as the same directory entry, so the source was not moved.
TEST_F(FilesystemMusicStorageHardLinkWithOtherCaseTest, MoveWithOverwriteReplacesDestination) {

  EXPECT_TRUE(CopySong(true, true));

  EXPECT_EQ(QByteArray("song data"), ReadFile(DestinationFilename()));
  EXPECT_EQ(QStringList() << u"song.flac"_s, AlbumEntries());

}

// Regression test: the directory can be searched but not read, so it was not found that the hard links are separate entries, and copying reported success without copying.
TEST_F(FilesystemMusicStorageHardLinkWithOtherCaseTest, CopyFailsWhenDirectoryCanNotBeRead) {

  if (::geteuid() == 0) {
    GTEST_SKIP() << "Root can read the directory.";
  }

  const QString album_path = root_path_ + u"/Artist/Album"_s;
  ASSERT_EQ(0, ::chmod(QFile::encodeName(album_path).constData(), S_IWUSR | S_IXUSR));
  const auto restore_permissions = qScopeGuard([album_path]() { ::chmod(QFile::encodeName(album_path).constData(), S_IRWXU); });

  EXPECT_FALSE(CopySong(false, true));

  EXPECT_EQ(Inode(SourceFilename()), Inode(DestinationFilename()));

}

// The cover is not removed for overwriting when it can't be found whether the destination is the cover itself.
TEST_F(FilesystemMusicStorageHardLinkWithOtherCaseTest, CopyFailsWhenCoverDirectoryCanNotBeRead) {

  if (::geteuid() == 0) {
    GTEST_SKIP() << "Root can read the directory.";
  }

  const QString album_path = root_path_ + u"/Artist/Album"_s;
  ASSERT_TRUE(CreateFile(album_path + u"/Cover.jpg"_s, QByteArray("cover data")));
  ASSERT_EQ(0, ::link(QFile::encodeName(album_path + u"/Cover.jpg"_s).constData(), QFile::encodeName(album_path + u"/cover.jpg"_s).constData()));
  ASSERT_TRUE(CreateFile(source_path_ + u"/Album/other.flac"_s));
  ASSERT_EQ(0, ::chmod(QFile::encodeName(album_path).constData(), S_IWUSR | S_IXUSR));
  const auto restore_permissions = qScopeGuard([album_path]() { ::chmod(QFile::encodeName(album_path).constData(), S_IRWXU); });

  MusicStorage::CopyJob job;
  job.source_ = source_path_ + u"/Album/other.flac"_s;
  job.destination_ = u"Artist/Album/other.flac"_s;
  job.overwrite_ = true;
  job.albumcover_ = true;
  job.cover_source_ = album_path + u"/Cover.jpg"_s;
  job.cover_dest_ = u"Artist/Album/cover.jpg"_s;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_FALSE(storage.CopyToStorage(job, error_text));

  EXPECT_EQ(Inode(album_path + u"/Cover.jpg"_s), Inode(album_path + u"/cover.jpg"_s));

}

// Hard links in directories which only differ in case are separate entries, also when the filenames are the same.
TEST_F(FilesystemMusicStorageHardLinkWithOtherCaseTest, CopyWithOverwriteReplacesHardLinkInDirectoryWithOtherCase) {

  const QString other_album_path = root_path_ + u"/artist/album"_s;
  ASSERT_TRUE(QDir().mkpath(other_album_path));
  ASSERT_EQ(0, ::link(QFile::encodeName(SourceFilename()).constData(), QFile::encodeName(other_album_path + u"/Song.flac"_s).constData()));

  MusicStorage::CopyJob job;
  job.source_ = SourceFilename();
  job.destination_ = u"artist/album/Song.flac"_s;
  job.overwrite_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("song data"), ReadFile(other_album_path + u"/Song.flac"_s));
  EXPECT_NE(Inode(SourceFilename()), Inode(other_album_path + u"/Song.flac"_s));

}
#endif

// The tests of case-insensitive filesystems run in a case-insensitive directory given by this environment variable.
// FilesystemMusicStorageTest.CaseInsensitiveFilesystem runs them on a case-insensitive tmpfs, mounted in a user and mount namespace, so it doesn't need root.
constexpr char kCaseInsensitiveDirEnv[] = "STRAWBERRY_TEST_CASE_INSENSITIVE_DIR";

class FilesystemMusicStorageCaseInsensitiveTest : public FilesystemMusicStorageTest {
 protected:
  void SetUp() override {
    const QString case_insensitive_path = qEnvironmentVariable(kCaseInsensitiveDirEnv);
    if (case_insensitive_path.isEmpty()) {
      GTEST_SKIP() << "Runs on a case-insensitive filesystem from FilesystemMusicStorageTest.CaseInsensitiveFilesystem.";
    }
    // The files are created in a directory of their own, which is removed afterwards, so nothing else in the directory is changed.
    case_insensitive_temp_dir_ = std::make_unique<QTemporaryDir>(case_insensitive_path + u"/strawberry_test-XXXXXX"_s);
    ASSERT_TRUE(case_insensitive_temp_dir_->isValid());
    root_path_ = case_insensitive_temp_dir_->path();
    ASSERT_TRUE(QDir().mkpath(root_path_ + u"/the artist/the album"_s));
    ASSERT_TRUE(CreateFile(root_path_ + u"/the artist/the album/01 song.flac"_s, QByteArray("song data")));
    ASSERT_TRUE(CreateFile(root_path_ + u"/the artist/the album/cover.jpg"_s, QByteArray("cover data")));
    // Make sure the directory really is case-insensitive.
    ASSERT_TRUE(QFile::exists(root_path_ + u"/THE ARTIST/THE ALBUM/01 SONG.FLAC"_s));
  }

  // Organizes the song to the same path, where only the case is changed.
  bool OrganizeWithOtherCase(const bool remove_original, const bool overwrite = true, const QString &destination = u"The Artist/The Album/01 Song.flac"_s, const QString &source = u"the artist/the album/01 song.flac"_s, const QString &cover_destination = u"The Artist/The Album/Cover.jpg"_s) {
    MusicStorage::CopyJob job;
    job.source_ = root_path_ + u'/' + source;
    job.original_root_ = root_path_;
    job.destination_ = destination;
    job.overwrite_ = overwrite;
    job.remove_original_ = remove_original;
    job.albumcover_ = true;
    job.cover_source_ = root_path_ + u"/the artist/the album/cover.jpg"_s;
    job.cover_dest_ = cover_destination;
    FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
    QString error_text;
    const bool success = storage.CopyToStorage(job, error_text);
    EXPECT_TRUE(error_text.isEmpty()) << error_text.toStdString();
    return success;
  }

  QStringList AlbumEntries() const {
    return QDir(root_path_ + u"/the artist/the album"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
  }

#ifdef Q_OS_UNIX
  // Returns true when hard links can be created in the directory, which is not possible on all case-insensitive filesystems, for example FAT.
  bool CanCreateHardLinks() const {
    const QString probe_filename = root_path_ + u"/hard-link-probe"_s;
    const QString probe_link_filename = root_path_ + u"/hard-link-probe-link"_s;
    if (!CreateFile(probe_filename)) return false;
    const bool success = ::link(QFile::encodeName(probe_filename).constData(), QFile::encodeName(probe_link_filename).constData()) == 0;
    QFile::remove(probe_link_filename);
    QFile::remove(probe_filename);
    return success;
  }

  // Returns true when symbolic links can be created in the directory, which is not possible on all case-insensitive filesystems, for example FAT.
  bool CanCreateSymbolicLinks() const {
    const QString probe_link_filename = root_path_ + u"/symbolic-link-probe"_s;
    const bool success = QFile::link(root_path_ + u"/the artist"_s, probe_link_filename);
    QFile::remove(probe_link_filename);
    return success;
  }
#endif

  std::unique_ptr<QTemporaryDir> case_insensitive_temp_dir_;
};

// Regression test: the destination was removed for overwriting, which was the song itself, and then it could not be moved.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, MoveChangingOnlyCaseKeepsSong) {

  EXPECT_TRUE(OrganizeWithOtherCase(true));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/The Artist/The Album/01 Song.flac"_s));
  EXPECT_EQ(QByteArray("cover data"), ReadFile(root_path_ + u"/The Artist/The Album/cover.jpg"_s));
  // Whether the case of the filename changes depends on the filesystem, a case-insensitive tmpfs can keep the old name, so the names are compared in lowercase.
  QStringList entries = AlbumEntries();
  for (QString &entry : entries) entry = entry.toLower();
  EXPECT_EQ(QStringList() << u"01 song.flac"_s << u"cover.jpg"_s, entries);

}

// Regression test: only the case of the directories changes, the filename is the same.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, MoveChangingOnlyCaseOfDirectoriesKeepsSong) {

  EXPECT_TRUE(OrganizeWithOtherCase(true, true, u"The Artist/The Album/01 song.flac"_s));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/The Artist/The Album/01 song.flac"_s));
  EXPECT_EQ(2, AlbumEntries().count());

}

// The destination is the song itself, so it's not a file which would be overwritten.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, MoveChangingOnlyCaseWithoutOverwrite) {

  EXPECT_TRUE(OrganizeWithOtherCase(true, false));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/The Artist/The Album/01 Song.flac"_s));
  EXPECT_EQ(2, AlbumEntries().count());

}

// Regression test: the destination was removed for overwriting, which was the song itself.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, CopyChangingOnlyCaseKeepsSong) {

  EXPECT_TRUE(OrganizeWithOtherCase(false));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/the artist/the album/01 song.flac"_s));
  EXPECT_EQ(QByteArray("cover data"), ReadFile(root_path_ + u"/the artist/the album/cover.jpg"_s));
  EXPECT_EQ(QStringList() << u"01 song.flac"_s << u"cover.jpg"_s, AlbumEntries());

}

// Regression test: the source has another case than the name in the directory, and the destination has the same case, so the destination was taken as another file and removed for overwriting.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, MoveWithSourceInOtherCaseKeepsSong) {

  EXPECT_TRUE(OrganizeWithOtherCase(true, true, u"the artist/the album/01 song.flac"_s, u"The Artist/The Album/01 Song.flac"_s));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/the artist/the album/01 song.flac"_s));
  EXPECT_EQ(2, AlbumEntries().count());

}

// Regression test: copying the song to itself returned before the cover was copied.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, CopyChangingOnlyCaseCopiesCover) {

  EXPECT_TRUE(OrganizeWithOtherCase(false, false, u"The Artist/The Album/01 Song.flac"_s, u"the artist/the album/01 song.flac"_s, u"The Artist/The Album/folder.jpg"_s));

  EXPECT_EQ(QByteArray("song data"), ReadFile(root_path_ + u"/the artist/the album/01 song.flac"_s));
  EXPECT_EQ(QByteArray("cover data"), ReadFile(root_path_ + u"/the artist/the album/folder.jpg"_s));
  EXPECT_EQ(QStringList() << u"01 song.flac"_s << u"cover.jpg"_s << u"folder.jpg"_s, AlbumEntries());

}

#ifdef Q_OS_UNIX
// Regression test: a case-insensitive directory can contain a case-sensitive directory, the hard links in it were taken as the same entry, since the case of the directory changes, so copying reported success without copying.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, CopyToHardLinkInCaseSensitiveDirectory) {

  if (!CanCreateHardLinks()) {
    GTEST_SKIP() << "Hard links can't be created on the filesystem.";
  }

  const QString album_path = root_path_ + u"/Album"_s;
  ASSERT_TRUE(QDir().mkpath(album_path));
  // The casefold flag can only be removed from an empty directory.
  if (QProcess::execute(u"chattr"_s, QStringList() << u"-F"_s << album_path) != 0) {
    GTEST_SKIP() << "The casefold flag could not be removed.";
  }
  ASSERT_TRUE(CreateFile(album_path + u"/Song.flac"_s, QByteArray("song data")));
  ASSERT_EQ(0, ::link(QFile::encodeName(album_path + u"/Song.flac"_s).constData(), QFile::encodeName(album_path + u"/song.flac"_s).constData()));

  MusicStorage::CopyJob job;
  job.source_ = album_path + u"/Song.flac"_s;
  job.destination_ = u"album/song.flac"_s;
  job.overwrite_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  struct stat source_stat {};
  struct stat destination_stat {};
  ASSERT_EQ(0, ::stat(QFile::encodeName(album_path + u"/Song.flac"_s).constData(), &source_stat));
  ASSERT_EQ(0, ::stat(QFile::encodeName(album_path + u"/song.flac"_s).constData(), &destination_stat));
  EXPECT_NE(source_stat.st_ino, destination_stat.st_ino);

}
#endif

// Regression test: the paths were compared case-insensitively by Qt first, which doesn't find that "ß" and "SS" are the same, which Linux does, so the song was removed for overwriting.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, MoveToNameWithOtherCaseFoldingKeepsSong) {

  ASSERT_TRUE(CreateFile(root_path_ + u"/the artist/the album/Straße.flac"_s, QByteArray("other song data")));
  if (!QFile::exists(root_path_ + u"/the artist/the album/STRASSE.flac"_s)) {
    GTEST_SKIP() << "The filesystem doesn't fold \"ß\" to \"SS\".";
  }

  MusicStorage::CopyJob job;
  job.source_ = root_path_ + u"/the artist/the album/Straße.flac"_s;
  job.original_root_ = root_path_;
  job.destination_ = u"the artist/the album/STRASSE.flac"_s;
  job.overwrite_ = true;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  EXPECT_EQ(QByteArray("other song data"), ReadFile(root_path_ + u"/the artist/the album/STRASSE.flac"_s));
  EXPECT_EQ(3, AlbumEntries().count());

}

#ifdef Q_OS_UNIX
// Regression test: the link was taken as the song itself, since the link and the song are the same file, and the name of the link in another case is not in the directory, so the song was replaced by the link.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, MoveLinkInOtherCaseToItsTargetKeepsSong) {

  if (!CanCreateSymbolicLinks()) {
    GTEST_SKIP() << "Symbolic links can't be created on the filesystem.";
  }

  const QString album_path = root_path_ + u"/the artist/the album"_s;
  ASSERT_TRUE(QFile::link(album_path + u"/01 song.flac"_s, album_path + u"/link.flac"_s));

  MusicStorage::CopyJob job;
  job.source_ = album_path + u"/LINK.flac"_s;
  job.original_root_ = root_path_;
  job.destination_ = u"the artist/the album/01 song.flac"_s;
  job.remove_original_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  storage.CopyToStorage(job, error_text);

  EXPECT_EQ(QByteArray("song data"), ReadFile(album_path + u"/01 song.flac"_s));
  EXPECT_FALSE(QFileInfo(album_path + u"/01 song.flac"_s).isSymLink());

}

// Regression test: the hard link was taken as the song itself, since the name of the hard link in another case is not in the directory, so copying reported success without copying.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, CopyToHardLinkInOtherCaseWithoutOverwriteFails) {

  if (!CanCreateHardLinks()) {
    GTEST_SKIP() << "Hard links can't be created on the filesystem.";
  }

  const QString album_path = root_path_ + u"/the artist/the album"_s;
  ASSERT_EQ(0, ::link(QFile::encodeName(album_path + u"/01 song.flac"_s).constData(), QFile::encodeName(album_path + u"/backup.flac"_s).constData()));

  MusicStorage::CopyJob job;
  job.source_ = album_path + u"/01 song.flac"_s;
  job.destination_ = u"the artist/the album/BACKUP.flac"_s;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_FALSE(storage.CopyToStorage(job, error_text));

}

// Regression test: the hard link was taken as the song itself, so it was not replaced by a copy.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, CopyToHardLinkInOtherCaseWithOverwriteReplacesIt) {

  if (!CanCreateHardLinks()) {
    GTEST_SKIP() << "Hard links can't be created on the filesystem.";
  }

  const QString album_path = root_path_ + u"/the artist/the album"_s;
  ASSERT_EQ(0, ::link(QFile::encodeName(album_path + u"/01 song.flac"_s).constData(), QFile::encodeName(album_path + u"/backup.flac"_s).constData()));

  MusicStorage::CopyJob job;
  job.source_ = album_path + u"/01 song.flac"_s;
  job.destination_ = u"the artist/the album/BACKUP.flac"_s;
  job.overwrite_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_TRUE(storage.CopyToStorage(job, error_text)) << error_text.toStdString();

  struct stat song_stat {};
  struct stat backup_stat {};
  ASSERT_EQ(0, ::stat(QFile::encodeName(album_path + u"/01 song.flac"_s).constData(), &song_stat));
  ASSERT_EQ(0, ::stat(QFile::encodeName(album_path + u"/BACKUP.flac"_s).constData(), &backup_stat));
  EXPECT_NE(song_stat.st_ino, backup_stat.st_ino);
  EXPECT_EQ(QByteArray("song data"), ReadFile(album_path + u"/BACKUP.flac"_s));

}

// With hard links, it can't be found which entry a name is, when Qt doesn't find that it only differs in case, then the job fails instead of taking it as the song itself.
TEST_F(FilesystemMusicStorageCaseInsensitiveTest, CopyToHardLinkWithOtherCaseFoldingFails) {

  if (!CanCreateHardLinks()) {
    GTEST_SKIP() << "Hard links can't be created on the filesystem.";
  }

  const QString album_path = root_path_ + u"/the artist/the album"_s;
  ASSERT_EQ(0, ::link(QFile::encodeName(album_path + u"/01 song.flac"_s).constData(), QFile::encodeName(album_path + u"/Straße.flac"_s).constData()));
  if (!QFile::exists(album_path + u"/STRASSE.flac"_s)) {
    GTEST_SKIP() << "The filesystem doesn't fold \"ß\" to \"SS\".";
  }

  MusicStorage::CopyJob job;
  job.source_ = album_path + u"/01 song.flac"_s;
  job.destination_ = u"the artist/the album/STRASSE.flac"_s;
  job.overwrite_ = true;
  FilesystemMusicStorage storage(Song::Source::Collection, root_path_);
  QString error_text;
  EXPECT_FALSE(storage.CopyToStorage(job, error_text));
  EXPECT_FALSE(error_text.isEmpty());

  EXPECT_EQ(QByteArray("song data"), ReadFile(album_path + u"/01 song.flac"_s));

}
#endif

// Runs the tests of case-insensitive filesystems on a case-insensitive tmpfs, which Linux supports since 6.13, in a user and mount namespace.
TEST_F(FilesystemMusicStorageTest, CaseInsensitiveFilesystem) {

#ifdef Q_OS_LINUX
  const QString mount_path = temp_dir_.filePath(u"case-insensitive"_s);
  ASSERT_TRUE(QDir().mkpath(mount_path));
  // The tmpfs is only mounted in the namespace, so it's gone when the process in it exits.
  // A directory is case-insensitive when it's empty and the casefold flag is set on it.
  const QString script = u"mount -t tmpfs -o casefold tmpfs \"$1\" && mkdir \"$1/music\" && chattr +F \"$1/music\" || exit 77\n"_s
                         u"%1=\"$1/music\" exec \"$2\" --gtest_color=no --gtest_filter=FilesystemMusicStorageCaseInsensitiveTest.*"_s.arg(QLatin1String(kCaseInsensitiveDirEnv));
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  // The tests are run in one process, also when this test is run in shards.
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.remove(u"GTEST_TOTAL_SHARDS"_s);
  environment.remove(u"GTEST_SHARD_INDEX"_s);
  environment.remove(u"GTEST_SHARD_STATUS_FILE"_s);
  process.setProcessEnvironment(environment);
  process.start(u"unshare"_s, QStringList() << u"--user"_s << u"--map-root-user"_s << u"--mount"_s << u"sh"_s << u"-c"_s << script << u"sh"_s << mount_path << QCoreApplication::applicationFilePath());
  if (!process.waitForStarted()) {
    GTEST_SKIP() << "unshare is not available.";
  }
  ASSERT_TRUE(process.waitForFinished(60000));
  const QByteArray output = process.readAll();
  // unshare fails when user namespaces are not allowed, and the script exits with 77 when a case-insensitive tmpfs is not supported.
  if (process.exitCode() == 77 || !output.contains("FilesystemMusicStorageCaseInsensitiveTest")) {
    GTEST_SKIP() << "No case-insensitive filesystem: " << output.toStdString();
  }
  EXPECT_EQ(0, process.exitCode()) << output.toStdString();
  EXPECT_TRUE(output.contains("[  PASSED  ] 12 tests.")) << output.toStdString();
#else
  GTEST_SKIP() << "Case-insensitive tmpfs is only supported on Linux.";
#endif

}

}  // namespace
