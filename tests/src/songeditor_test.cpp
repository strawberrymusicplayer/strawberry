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

#include <memory>

#include "gtest_include.h"
#include "test_utils.h"

#include <QCoreApplication>
#include <QThread>
#include <QThreadPool>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QString>
#include <QSignalSpy>
#include <QElapsedTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "lyrics/lyricsprovider.h"
#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricssearchrequest.h"
#include "lyrics/lyricssearchresult.h"
#include "widgets/lineedit.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "core/networkaccessmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "streaming/streamingservices.h"
#include "songeditor/tageditor.h"
#include "songeditor/lyricseditor.h"
#include "songeditor/lyricssearchtype.h"
#include "songeditor/lyricstab.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static

namespace {

// Returns lyrics and synchronized lyrics without using the network.
class FakeLyricsProvider : public LyricsProvider {
 public:
  explicit FakeLyricsProvider(QObject *parent = nullptr) : LyricsProvider(u"Fake"_s, true, false, true, nullptr, parent) {}

 protected:
  void StartSearch(const int id, const LyricsSearchRequest &request) override {
    LyricsSearchResult result(u"Found lyrics"_s);
    result.synced_lyrics = u"[00:01.00]Found synced lyrics"_s;
    result.artist = request.artist;
    result.title = request.title;
    QTimer::singleShot(0, this, [this, id, result]() { Q_EMIT SearchFinished(id, LyricsSearchResults() << result); });
  }
};

class SongEditorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tagreader_client_ = SharedPtr<TagReaderClient>(new TagReaderClient(), [](TagReaderClient *tagreader_client) { tagreader_client->deleteLater(); });
    tagreader_client_thread_ = new QThread();
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
    lyrics_providers_ = make_shared<LyricsProviders>();
    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<MemoryDatabase>(nullptr);
    collection_backend_ = make_shared<CollectionBackend>();
    collection_backend_->Init(database_, task_manager_, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  }

  void TearDown() override {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    delete tagreader_client_thread_;
  }

  // Copies the test MP3 file to the directory, and reads it.
  Song CreateSong(const QTemporaryDir &temp_dir, const QString &name = u"song.mp3"_s) const {
    const QString filename = temp_dir.filePath(name);
    if (!QFile::copy(u":/audio/strawberry.mp3"_s, filename)) return Song();
    QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    Song song(Song::Source::LocalFile);
    if (!tagreader_client_->ReadFileBlocking(filename, &song).success()) return Song();
    return song;
  }

  // Processes events until the condition is true, returns false on timeout.
  template <typename Condition>
  static bool WaitFor(Condition condition, const int timeout_msec = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > timeout_msec) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return true;
  }

  QThread *tagreader_client_thread_ = nullptr;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<LyricsProviders> lyrics_providers_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> collection_backend_;
};

// The lyrics editor only has the lyrics tabs, and only saves the lyrics to the tags.
TEST_F(SongEditorTest, SavesOnlyLyricsToTags) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_TRUE(song.IsEditable());
  const QString title = song.title();
  const QString artist = song.artist();

  LyricsEditor editor(tagreader_client_, collection_backend_, lyrics_providers_);

  // The lyrics and synchronized lyrics tabs.
  QTabWidget *tab_widget = editor.findChild<QTabWidget*>(u"tab_widget"_s);
  ASSERT_NE(nullptr, tab_widget);
  EXPECT_EQ(2, tab_widget->count());

  ASSERT_TRUE(editor.SetSongs(SongList() << song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  TextEdit *lyrics = editor.findChild<TextEdit*>(u"lyrics"_s);
  ASSERT_NE(nullptr, lyrics);
  ASSERT_TRUE(lyrics->isEnabled());
  lyrics->setPlainText(u"New lyrics"_s);
  EXPECT_EQ(u"New lyrics"_s, editor.songs().first().lyrics());

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  QSignalSpy spy_error(&editor, &SongEditor::Error);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_EQ(0, spy_error.count());

  Song saved_song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &saved_song).success());
  EXPECT_EQ(u"New lyrics"_s, saved_song.lyrics());
  EXPECT_EQ(title, saved_song.title());
  EXPECT_EQ(artist, saved_song.artist());

}

// The tag editor has the tags and the lyrics tabs, and saves both the tags and the lyrics.
TEST_F(SongEditorTest, TagEditorSavesTagsAndLyrics) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const Song song = CreateSong(temp_dir);
  ASSERT_TRUE(song.IsEditable());

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());

  // The summary, tags, lyrics and synchronized lyrics tabs.
  QTabWidget *tab_widget = editor.findChild<QTabWidget*>(u"tab_widget"_s);
  ASSERT_NE(nullptr, tab_widget);
  ASSERT_EQ(4, tab_widget->count());
  EXPECT_EQ(u"tab_summary"_s, tab_widget->widget(0)->objectName());
  EXPECT_EQ(u"tab_tags"_s, tab_widget->widget(1)->objectName());

  ASSERT_TRUE(editor.SetSongs(SongList() << song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  LineEdit *title = editor.findChild<LineEdit*>(u"title"_s);
  TextEdit *lyrics = editor.findChild<TextEdit*>(u"lyrics"_s);
  ASSERT_NE(nullptr, title);
  ASSERT_NE(nullptr, lyrics);
  title->setText(u"New title"_s);
  lyrics->setPlainText(u"New lyrics"_s);

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  QSignalSpy spy_error(&editor, &SongEditor::Error);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_EQ(0, spy_error.count());

  Song saved_song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(song.url().toLocalFile(), &saved_song).success());
  EXPECT_EQ(u"New title"_s, saved_song.title());
  EXPECT_EQ(u"New lyrics"_s, saved_song.lyrics());

  // The tag completers load their models from the collection backend in the background, wait for them before the backend is deleted.
  QThreadPool::globalInstance()->waitForDone();

}

// Completing the lyrics automatically only searches for the lyrics the songs are missing, and doesn't replace the lyrics they have.
TEST_F(SongEditorTest, CompleteLyricsOnlyForMissingLyrics) {

  qRegisterMetaType<LyricsSearchRequest>("LyricsSearchRequest");
  qRegisterMetaType<LyricsSearchResult>("LyricsSearchResult");
  qRegisterMetaType<LyricsSearchResults>("LyricsSearchResults");
  lyrics_providers_->AddProvider(new FakeLyricsProvider(&*lyrics_providers_));

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const Song song_a = CreateSong(temp_dir, u"a.mp3"_s);
  const Song song_b = CreateSong(temp_dir, u"b.mp3"_s);
  ASSERT_TRUE(song_a.IsEditable());
  ASSERT_TRUE(song_b.IsEditable());

  LyricsEditor editor(tagreader_client_, collection_backend_, lyrics_providers_);
  ASSERT_TRUE(editor.SetSongs(SongList() << song_a << song_b));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

  QListWidget *song_list = editor.findChild<QListWidget*>(u"song_list"_s);
  TextEdit *lyrics = editor.findChild<TextEdit*>(u"lyrics"_s);
  ASSERT_NE(nullptr, song_list);
  ASSERT_NE(nullptr, lyrics);

  // Song A has lyrics, song B has none, the test file has lyrics in its tags.
  song_list->setCurrentRow(0);
  lyrics->setPlainText(u"Existing lyrics"_s);
  song_list->setCurrentRow(1);
  lyrics->setPlainText(QString());
  ASSERT_TRUE(editor.songs().at(1).lyrics().isEmpty());
  song_list->selectAll();

  // Search for both, the search type is the same in both lyrics tabs.
  const QList<LyricsTab*> lyrics_tabs = editor.findChildren<LyricsTab*>();
  ASSERT_EQ(2, lyrics_tabs.count());
  const QList<QComboBox*> search_types = QList<QComboBox*>() << lyrics_tabs.at(0)->search_type() << lyrics_tabs.at(1)->search_type();
  search_types.at(0)->setCurrentIndex(search_types.at(0)->findData(static_cast<int>(LyricsSearchType::Lyrics)));
  EXPECT_EQ(static_cast<int>(LyricsSearchType::Lyrics), search_types.at(1)->currentData().toInt());
  search_types.at(1)->setCurrentIndex(search_types.at(1)->findData(static_cast<int>(LyricsSearchType::Both)));
  EXPECT_EQ(static_cast<int>(LyricsSearchType::Both), search_types.at(0)->currentData().toInt());

  const QList<QPushButton*> fetch_buttons = QList<QPushButton*>() << lyrics_tabs.at(0)->fetch_button() << lyrics_tabs.at(1)->fetch_button();
  ASSERT_TRUE(fetch_buttons.at(0)->isEnabled());
  fetch_buttons.at(0)->click();

  const QList<QLabel*> statuses = QList<QLabel*>() << lyrics_tabs.at(0)->status() << lyrics_tabs.at(1)->status();
  ASSERT_TRUE(WaitFor([&statuses]() { return statuses.at(0)->text().startsWith(u"Found lyrics for"_s); }));
  EXPECT_EQ(statuses.at(0)->text(), statuses.at(1)->text());
  EXPECT_EQ(u"Found lyrics for 2 of 2 songs."_s, statuses.at(0)->text());

  const SongList songs = editor.songs();
  ASSERT_EQ(2, songs.count());
  EXPECT_EQ(u"Existing lyrics"_s, songs.at(0).lyrics());
  EXPECT_EQ(u"[00:01.00]Found synced lyrics"_s, songs.at(0).synced_lyrics());
  EXPECT_EQ(u"Found lyrics"_s, songs.at(1).lyrics());
  EXPECT_EQ(u"[00:01.00]Found synced lyrics"_s, songs.at(1).synced_lyrics());

  // Nothing is missing anymore, so nothing is searched for.
  fetch_buttons.at(1)->click();
  EXPECT_EQ(u"The selected songs already have lyrics."_s, statuses.at(0)->text());

}

// Regression test: making the tag editor wider made the change art button wider and moved the summary to the right of the cover.
TEST_F(SongEditorTest, TagEditorCoverAreaKeepsSizeWhenWider) {

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  editor.show();

  QTabWidget *tab_widget = editor.findChild<QTabWidget*>(u"tab_widget"_s);
  QWidget *tab_tags = editor.findChild<QWidget*>(u"tab_tags"_s);
  QWidget *tags_art_button = editor.findChild<QWidget*>(u"tags_art_button"_s);
  QWidget *tags_summary = editor.findChild<QWidget*>(u"tags_summary"_s);
  ASSERT_NE(nullptr, tab_widget);
  ASSERT_NE(nullptr, tab_tags);
  ASSERT_NE(nullptr, tags_art_button);
  ASSERT_NE(nullptr, tags_summary);
  tab_widget->setCurrentWidget(tab_tags);

  editor.resize(800, editor.height());
  QCoreApplication::processEvents();
  const int button_width = tags_art_button->width();
  const int summary_x = tags_summary->x();
  const int summary_width = tags_summary->width();

  editor.resize(1600, editor.height());
  QCoreApplication::processEvents();
  EXPECT_EQ(button_width, tags_art_button->width());
  EXPECT_EQ(summary_x, tags_summary->x());
  EXPECT_GT(tags_summary->width(), summary_width);

  // The tag completers load their models from the collection backend in the background, wait for them before the backend is deleted.
  QThreadPool::globalInstance()->waitForDone();

}

// Regression test: saving lyrics in the lyrics editor saved the whole song as it was loaded to the collection, so changes to the song since it was loaded, like the playcount or the other lyrics, were lost.
TEST_F(SongEditorTest, LyricsEditorKeepsCollectionChangesWhenSavingLyrics) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song song = CreateSong(temp_dir);
  ASSERT_TRUE(song.is_valid());

  // Add the song to the collection.
  collection_backend_->AddDirectory(temp_dir.path());
  song.set_source(Song::Source::Collection);
  song.set_directory_id(1);
  collection_backend_->AddOrUpdateSongs(SongList() << song);
  SongList collection_songs = collection_backend_->GetAllSongs();
  ASSERT_EQ(1, collection_songs.count());
  const Song collection_song = collection_songs.first();
  ASSERT_TRUE(collection_song.is_local_collection_song());
  ASSERT_EQ(0, collection_song.playcount());

  LyricsEditor editor(tagreader_client_, collection_backend_, lyrics_providers_);
  ASSERT_TRUE(editor.SetSongs(SongList() << collection_song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  // The song is played, and gets synchronized lyrics from elsewhere, while the editor is open.
  collection_backend_->IncrementPlayCount(collection_song.id());
  collection_backend_->UpdateSongLyrics(collection_song.id(), true, u"[00:01.00]Other synced lyrics"_s, static_cast<int>(Song::StorageType::Database), false, false);

  // Save the lyrics to the database.
  const QList<LyricsTab*> lyrics_tabs = editor.findChildren<LyricsTab*>();
  ASSERT_EQ(2, lyrics_tabs.count());
  LyricsTab *lyrics_tab = lyrics_tabs.at(0)->synced() ? lyrics_tabs.at(1) : lyrics_tabs.at(0);
  QComboBox *storage = lyrics_tab->storage();
  storage->setCurrentIndex(storage->findData(static_cast<int>(Song::StorageType::Database)));
  ASSERT_EQ(Song::StorageType::Database, editor.songs().first().lyrics_storage_type());
  lyrics_tab->lyrics()->setPlainText(u"New lyrics"_s);

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  QSignalSpy spy_songs_changed(&*collection_backend_, &CollectionBackend::SongsChanged);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  ASSERT_TRUE(WaitFor([&spy_songs_changed]() { return spy_songs_changed.count() > 0; }));

  collection_songs = collection_backend_->GetAllSongs();
  ASSERT_EQ(1, collection_songs.count());
  EXPECT_EQ(u"New lyrics"_s, collection_songs.first().lyrics());
  EXPECT_EQ(Song::StorageType::Database, collection_songs.first().lyrics_storage_type());
  EXPECT_EQ(1, collection_songs.first().playcount());
  // Only the lyrics were saved, not the synchronized lyrics.
  EXPECT_EQ(u"[00:01.00]Other synced lyrics"_s, collection_songs.first().synced_lyrics());

}

// Regression test: lyrics which could not be written to the tags were kept in the edited songs, so the playlist got lyrics which were not saved anywhere.
TEST_F(SongEditorTest, LyricsEditorRevertsLyricsNotWrittenToTags) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"song.mp3"_s);
  ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));

  Song song(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(filename, &song).success());
  ASSERT_EQ(Song::StorageType::Tag, song.lyrics_storage_type());
  const QString lyrics = song.lyrics();

  LyricsEditor editor(tagreader_client_, collection_backend_, lyrics_providers_);
  ASSERT_TRUE(editor.SetSongs(SongList() << song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  // Writing the tags fails.
  ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::ReadUser));
  if (QFileInfo(filename).isWritable()) {
    GTEST_SKIP() << "The file is still writable, the test is running as root.";
  }

  TextEdit *lyrics_edit = editor.findChild<TextEdit*>(u"lyrics"_s);
  ASSERT_NE(nullptr, lyrics_edit);
  lyrics_edit->setPlainText(u"New lyrics"_s);

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  QSignalSpy spy_error(&editor, &SongEditor::Error);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_EQ(1, spy_error.count());

  // The songs only have the lyrics which were saved.
  EXPECT_EQ(lyrics, editor.songs().first().lyrics());
  EXPECT_EQ(Song::StorageType::Tag, editor.songs().first().lyrics_storage_type());

}

}  // namespace
