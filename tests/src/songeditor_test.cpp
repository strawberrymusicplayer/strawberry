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
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QString>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QAction>
#include <QImage>
#include <QMimeData>
#include <QDropEvent>
#include <QCheckBox>
#include <QScopeGuard>
#include <QDir>
#include <QStringList>
#include <QDialogButtonBox>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "utilities/imageutils.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/savetagcoverdata.h"
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
#include "covermanager/albumcoverchoicecontroller.h"
#include "utilities/coverutils.h"
#include "utilities/coveroptions.h"

#include "deleteconfirmationanswer.h"

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

  // Drops the image on the cover of the tag editor with only the image, not the image data, like an image dropped from a browser.
  static void DropImageOnCover(TagEditor *editor, const QImage &image) {
    QMimeData mime_data;
    mime_data.setImageData(image);
    QWidget *tags_art = editor->findChild<QWidget*>(u"tags_art"_s);
    ASSERT_NE(nullptr, tags_art);
    // The drop goes to the widget which accepted entering the drag.
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    QDragEnterEvent drag_enter_event(QPointF(1, 1), Qt::CopyAction, &mime_data, Qt::LeftButton, Qt::NoModifier);
#else
    QDragEnterEvent drag_enter_event(QPoint(1, 1), Qt::CopyAction, &mime_data, Qt::LeftButton, Qt::NoModifier);
#endif
    QCoreApplication::sendEvent(tags_art, &drag_enter_event);
    ASSERT_TRUE(drag_enter_event.isAccepted());
    QDropEvent drop_event(QPointF(1, 1), Qt::CopyAction, &mime_data, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(tags_art, &drop_event);
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

// Regression test: an image dropped on the cover has no image data, and empty cover data removes the embedded cover, so the embedded cover was removed instead of replaced.
TEST_F(SongEditorTest, TagEditorEmbedsDroppedCover) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song song = CreateSong(temp_dir);
  ASSERT_TRUE(song.IsEditable());
  ASSERT_TRUE(song.save_embedded_cover_supported());

  // The song has an embedded cover, which is replaced by the dropped image.
  QImage original_cover(11, 13, QImage::Format_RGB32);
  original_cover.fill(Qt::blue);
  ASSERT_TRUE(tagreader_client_->SaveCoverBlocking(song.url().toLocalFile(), SaveTagCoverData(QString(), ImageUtils::SaveImageToJpegData(original_cover), u"image/jpeg"_s)).success());
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(song.url().toLocalFile(), &song).success());
  ASSERT_TRUE(song.art_embedded());

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  QCheckBox *checkbox_embedded_cover = editor.findChild<QCheckBox*>(u"checkbox_embedded_cover"_s);
  ASSERT_NE(nullptr, checkbox_embedded_cover);
  checkbox_embedded_cover->setChecked(true);

  // A dropped image, for example from a browser, only has the image, not the image data.
  QImage dropped_cover(37, 23, QImage::Format_RGB32);
  dropped_cover.fill(Qt::red);
  ASSERT_NE(dropped_cover.size(), original_cover.size());
  ASSERT_NO_FATAL_FAILURE(DropImageOnCover(&editor, dropped_cover));

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  QSignalSpy spy_error(&editor, &SongEditor::Error);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_EQ(0, spy_error.count());

  QImage saved_cover;
  ASSERT_TRUE(tagreader_client_->LoadCoverImageBlocking(song.url().toLocalFile(), saved_cover).success());
  ASSERT_FALSE(saved_cover.isNull());
  EXPECT_EQ(dropped_cover.size(), saved_cover.size());

  QThreadPool::globalInstance()->waitForDone();

}

// A dropped image which can't be saved keeps the existing cover, the cover chosen in the editor was cleared when the image was dropped, and was saved like that.
// The image is embedded, or saved to a file when the cover isn't embedded.
// The cover is cleared first, which also clears the embedded and automatic cover in the editor.
TEST_F(SongEditorTest, TagEditorKeepsCoverWhenDroppedCoverCantBeSaved) {

  // The cover file is saved in the image cache, which is only redirected for the tests on Linux and BSD, keep it out of the users own cache on Windows and macOS.
  QStandardPaths::setTestModeEnabled(true);

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song song = CreateSong(temp_dir);
  ASSERT_TRUE(song.IsEditable());
  ASSERT_TRUE(song.save_embedded_cover_supported());
  ASSERT_FALSE(song.effective_albumartist().isEmpty());
  ASSERT_FALSE(song.album().isEmpty());

  QImage original_cover(11, 13, QImage::Format_RGB32);
  original_cover.fill(Qt::blue);
  ASSERT_TRUE(tagreader_client_->SaveCoverBlocking(song.url().toLocalFile(), SaveTagCoverData(QString(), ImageUtils::SaveImageToJpegData(original_cover), u"image/jpeg"_s)).success());
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(song.url().toLocalFile(), &song).success());
  ASSERT_TRUE(song.art_embedded());
  const QUrl art_automatic = QUrl::fromLocalFile(temp_dir.filePath(u"automatic.jpg"_s));
  const QUrl art_manual = QUrl::fromLocalFile(temp_dir.filePath(u"manual.jpg"_s));
  song.set_art_automatic(art_automatic);
  constexpr qint64 art_automatic_mtime = 1234567890000;
  song.set_art_automatic_mtime(art_automatic_mtime);
  song.set_art_manual(art_manual);
  song.set_art_unset(true);

  // A collection song, the collection is updated with the cover when saving.
  collection_backend_->AddDirectory(temp_dir.path());
  song.set_source(Song::Source::Collection);
  song.set_directory_id(1);
  collection_backend_->AddOrUpdateSongs(SongList() << song);
  const SongList collection_songs = collection_backend_->GetAllSongs();
  ASSERT_EQ(1, collection_songs.count());
  song = collection_songs.first();
  ASSERT_TRUE(song.art_embedded());
  ASSERT_EQ(art_automatic, song.art_automatic());
  ASSERT_EQ(art_automatic_mtime, song.art_automatic_mtime());
  ASSERT_EQ(art_manual, song.art_manual());
  ASSERT_TRUE(song.art_unset());

  // JPEG images can't be wider than 65535 pixels.
  QImage dropped_cover(70000, 1, QImage::Format_RGB32);
  dropped_cover.fill(Qt::red);
  ASSERT_TRUE(ImageUtils::SaveImageToJpegData(dropped_cover).isEmpty());

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);

  for (const bool embedded : {true, false}) {
    SCOPED_TRACE(embedded ? "Embedded cover" : "Cover file");

    TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
    ASSERT_TRUE(editor.SetSongs(SongList() << song));
    ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

    AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
    ASSERT_NE(nullptr, album_cover_choice_controller);
    QAction *clear_cover_action = album_cover_choice_controller->clear_cover_action();
    ASSERT_TRUE(clear_cover_action->isEnabled());
    clear_cover_action->trigger();
    ASSERT_FALSE(editor.songs().first().art_embedded());
    ASSERT_TRUE(editor.songs().first().art_automatic().isEmpty());

    ASSERT_NO_FATAL_FAILURE(DropImageOnCover(&editor, dropped_cover));

    // Changing the cover sets the checkbox from the song, so it's set after dropping the image.
    QCheckBox *checkbox_embedded_cover = editor.findChild<QCheckBox*>(u"checkbox_embedded_cover"_s);
    ASSERT_NE(nullptr, checkbox_embedded_cover);
    checkbox_embedded_cover->setChecked(embedded);

    QSignalSpy spy_accepted(&editor, &QDialog::accepted);
    editor.accept();
    ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));

    // The songs of the editor update the playlist.
    const Song editor_song = editor.songs().first();
    EXPECT_TRUE(editor_song.art_embedded());
    EXPECT_EQ(art_automatic, editor_song.art_automatic());
    EXPECT_EQ(art_automatic_mtime, editor_song.art_automatic_mtime());
    EXPECT_EQ(art_manual, editor_song.art_manual());
    EXPECT_TRUE(editor_song.art_unset());
    const Song collection_song = collection_backend_->GetSongById(song.id());
    EXPECT_TRUE(collection_song.art_embedded());
    EXPECT_EQ(art_automatic, collection_song.art_automatic());
    EXPECT_EQ(art_automatic_mtime, collection_song.art_automatic_mtime());
    EXPECT_EQ(art_manual, collection_song.art_manual());
    EXPECT_TRUE(collection_song.art_unset());

    QImage saved_cover;
    ASSERT_TRUE(tagreader_client_->LoadCoverImageBlocking(song.url().toLocalFile(), saved_cover).success());
    EXPECT_EQ(original_cover.size(), saved_cover.size());

    QThreadPool::globalInstance()->waitForDone();
  }

  QStandardPaths::setTestModeEnabled(false);

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

// Deleting the dialog while the songs are loaded in the background, for example when quitting, stops loading them, and waits for it to stop.
TEST_F(SongEditorTest, DeletingWhileLoadingStopsLoading) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  // Reading all the songs takes as long as loading them.
  SongList songs;
  for (int i = 0; i < 2000; ++i) {
    const QString filename = temp_dir.filePath(QStringLiteral("%1.mp3").arg(i));
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    ASSERT_TRUE(QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
  }
  QElapsedTimer timer_read;
  timer_read.start();
  for (int i = 0; i < 2000; ++i) {
    Song song(Song::Source::LocalFile);
    ASSERT_TRUE(tagreader_client_->ReadFileBlocking(temp_dir.filePath(QStringLiteral("%1.mp3").arg(i)), &song).success());
    ASSERT_TRUE(song.IsEditable());
    songs << song;
  }
  const qint64 read_msec = timer_read.elapsed();

  ASSERT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));
  LyricsEditor *editor = new LyricsEditor(tagreader_client_, collection_backend_, lyrics_providers_);
  ASSERT_TRUE(editor->SetSongs(songs));
  QElapsedTimer timer_delete;
  timer_delete.start();
  delete editor;
  const qint64 delete_msec = timer_delete.elapsed();

  // Only the file being read when it was deleted is read.
  EXPECT_LT(delete_msec, read_msec / 4) << "Reading the songs took " << read_msec << " ms.";
  // The pool thread can still finish its own bookkeeping after the loading finished.
  EXPECT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));

}

// Regression test: deleting the cover deleted the user's own image in the album directory without asking.
// The cover is deleted for the other songs of the album too, so their cover files are asked about as well.
TEST_F(SongEditorTest, TagEditorAsksBeforeDeletingCoverOutsideCache) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  SongList songs;
  QStringList cover_filenames;
  for (const QString &name : {u"a"_s, u"b"_s}) {
    Song song = CreateSong(temp_dir, name + u".mp3"_s);
    ASSERT_TRUE(song.IsEditable());
    ASSERT_FALSE(song.album().isEmpty());
    song.set_source(Song::Source::Collection);
    const QString cover_filename = temp_dir.filePath(name + u".jpg"_s);
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    ASSERT_TRUE(image.save(cover_filename, "JPG"));
    song.set_art_manual(QUrl::fromLocalFile(cover_filename));
    songs << song;
    cover_filenames << cover_filename;
  }

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);

  // The cover files are kept when canceling, and deleted when saving after confirming.
  for (const QDialogButtonBox::StandardButton button : {QDialogButtonBox::Cancel, QDialogButtonBox::Yes}) {
    TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
    ASSERT_TRUE(editor.SetSongs(songs));
    ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

    // Only the first song is selected.
    // The track selection dialog of the tag fetcher has a song list with the same name.
    QListWidget *song_list = nullptr;
    const QList<QListWidget*> song_lists = editor.findChildren<QListWidget*>(u"song_list"_s);
    for (QListWidget *list : song_lists) {
      if (list->window() == &editor) song_list = list;
    }
    ASSERT_NE(nullptr, song_list);
    ASSERT_EQ(2, song_list->count());
    song_list->clearSelection();
    song_list->item(0)->setSelected(true);
    ASSERT_EQ(1, song_list->selectedItems().count());

    AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
    ASSERT_NE(nullptr, album_cover_choice_controller);
    // The cover is cleared first, the files deleted when saving are the files of the songs when the editor was opened.
    QAction *clear_cover_action = album_cover_choice_controller->clear_cover_action();
    ASSERT_TRUE(clear_cover_action->isEnabled());
    clear_cover_action->trigger();
    QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
    ASSERT_TRUE(delete_cover_action->isEnabled());
    {
      DeleteConfirmationAnswer answer(button);
      delete_cover_action->trigger();
      EXPECT_EQ(1, answer.shown());
      EXPECT_EQ(cover_filenames, answer.files());
    }

    QSignalSpy spy_accepted(&editor, &QDialog::accepted);
    editor.accept();
    ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
    for (const QString &cover_filename : std::as_const(cover_filenames)) {
      EXPECT_EQ(button != QDialogButtonBox::Yes, QFile::exists(cover_filename)) << cover_filename.toStdString();
    }

    // The tag completers load their models from the collection backend in the background, wait for them before the backend is deleted.
    QThreadPool::globalInstance()->waitForDone();
  }

}

// Regression test: a cover file which didn't exist when the user was asked, or which changed after the user was asked, was deleted when saving without asking.
TEST_F(SongEditorTest, TagEditorKeepsCoverFileCreatedOrChangedAfterAsking) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song song = CreateSong(temp_dir);
  ASSERT_TRUE(song.IsEditable());
  // The embedded cover is kept too.
  ASSERT_TRUE(song.save_embedded_cover_supported());
  QImage embedded_cover(11, 13, QImage::Format_RGB32);
  embedded_cover.fill(Qt::green);
  ASSERT_TRUE(tagreader_client_->SaveCoverBlocking(song.url().toLocalFile(), SaveTagCoverData(QString(), ImageUtils::SaveImageToJpegData(embedded_cover), u"image/jpeg"_s)).success());
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(song.url().toLocalFile(), &song).success());
  ASSERT_TRUE(song.art_embedded());
  song.set_source(Song::Source::Collection);
  // The automatic cover exists when the user is asked, and changes after, the manual cover is created after.
  const QString changed_cover_filename = temp_dir.filePath(u"automatic.jpg"_s);
  const QString created_cover_filename = temp_dir.filePath(u"manual.jpg"_s);
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(Qt::red);
  ASSERT_TRUE(image.save(changed_cover_filename, "JPG"));
  song.set_art_automatic(QUrl::fromLocalFile(changed_cover_filename));
  song.set_art_manual(QUrl::fromLocalFile(created_cover_filename));

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    delete_cover_action->trigger();
    EXPECT_EQ(1, answer.shown());
    EXPECT_EQ(QStringList() << changed_cover_filename, answer.files());
  }

  QImage new_image(16, 16, QImage::Format_RGB32);
  new_image.fill(Qt::blue);
  ASSERT_TRUE(new_image.save(changed_cover_filename, "JPG"));
  ASSERT_TRUE(new_image.save(created_cover_filename, "JPG"));

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_TRUE(QFile::exists(changed_cover_filename));
  EXPECT_TRUE(QFile::exists(created_cover_filename));
  // The song keeps the cover.
  EXPECT_TRUE(editor.songs().first().art_embedded());
  QImage saved_embedded_cover;
  ASSERT_TRUE(tagreader_client_->LoadCoverImageBlocking(song.url().toLocalFile(), saved_embedded_cover).success());
  EXPECT_EQ(embedded_cover.size(), saved_embedded_cover.size());
  EXPECT_EQ(QUrl::fromLocalFile(changed_cover_filename), editor.songs().first().art_automatic());
  EXPECT_EQ(QUrl::fromLocalFile(created_cover_filename), editor.songs().first().art_manual());

  QThreadPool::globalInstance()->waitForDone();

}

// Regression test: the songs could be replaced while the user was asked, and then the cover of the songs in the same rows was deleted.
TEST_F(SongEditorTest, TagEditorDoesNotDeleteCoverWhenSongsReplacedWhileAsking) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  SongList songs;
  QStringList cover_filenames;
  for (const QString &name : {u"a"_s, u"b"_s}) {
    Song song = CreateSong(temp_dir, name + u".mp3"_s);
    ASSERT_TRUE(song.IsEditable());
    song.set_source(Song::Source::Collection);
    const QString cover_filename = temp_dir.filePath(name + u".jpg"_s);
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    ASSERT_TRUE(image.save(cover_filename, "JPG"));
    song.set_art_manual(QUrl::fromLocalFile(cover_filename));
    songs << song;
    cover_filenames << cover_filename;
  }

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << songs.value(0)));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    // The other song is set while the user is asked about the cover of the first song.
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes, [&editor, &songs]() { editor.SetSongs(SongList() << songs.value(1)); });
    delete_cover_action->trigger();
    EXPECT_EQ(1, answer.shown());
    EXPECT_EQ(QStringList() << cover_filenames.value(0), answer.files());
  }
  ASSERT_TRUE(WaitFor([&editor, &songs]() { return editor.songs().count() == 1 && editor.songs().first().url() == songs.value(1).url(); }));

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  for (const QString &cover_filename : std::as_const(cover_filenames)) {
    EXPECT_TRUE(QFile::exists(cover_filename)) << cover_filename.toStdString();
  }

  QThreadPool::globalInstance()->waitForDone();

}

// The cover is deleted for the songs the user was asked about, also when the selected songs changed while the user was asked.
TEST_F(SongEditorTest, TagEditorDeletesCoverOfAskedSongs) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  SongList songs;
  QStringList cover_filenames;
  for (const QString &name : {u"a"_s, u"b"_s}) {
    Song song = CreateSong(temp_dir, name + u".mp3"_s);
    ASSERT_TRUE(song.IsEditable());
    song.set_source(Song::Source::Collection);
    const QString cover_filename = temp_dir.filePath(name + u".jpg"_s);
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    ASSERT_TRUE(image.save(cover_filename, "JPG"));
    song.set_art_manual(QUrl::fromLocalFile(cover_filename));
    songs << song;
    cover_filenames << cover_filename;
  }

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(songs));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

  // The track selection dialog of the tag fetcher has a song list with the same name.
  QListWidget *song_list = nullptr;
  const QList<QListWidget*> song_lists = editor.findChildren<QListWidget*>(u"song_list"_s);
  for (QListWidget *list : song_lists) {
    if (list->window() == &editor) song_list = list;
  }
  ASSERT_NE(nullptr, song_list);

  // Songs from different albums, so the cover is only deleted for the selected song.
  song_list->clearSelection();
  song_list->item(1)->setSelected(true);
  LineEdit *album = editor.findChild<LineEdit*>(u"album"_s);
  ASSERT_NE(nullptr, album);
  album->setText(u"Other album"_s);
  ASSERT_NE(editor.songs().value(0).album(), editor.songs().value(1).album());

  song_list->clearSelection();
  song_list->item(0)->setSelected(true);

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes, [song_list]() {
      song_list->clearSelection();
      song_list->item(1)->setSelected(true);
    });
    delete_cover_action->trigger();
    EXPECT_EQ(1, answer.shown());
    EXPECT_EQ(QStringList() << cover_filenames.value(0), answer.files());
  }

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_FALSE(QFile::exists(cover_filenames.value(0)));
  EXPECT_TRUE(QFile::exists(cover_filenames.value(1)));

  QThreadPool::globalInstance()->waitForDone();

}

// Regression test: songs can share a cover file, when another cover file of one of the songs changed after the user was asked, that song kept the cover, but the shared file was deleted with the other song.
TEST_F(SongEditorTest, TagEditorKeepsSharedCoverFileWhenCoverFileChangedAfterAsking) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(Qt::red);
  const QString shared_cover_filename = temp_dir.filePath(u"shared.jpg"_s);
  const QString changed_cover_filename = temp_dir.filePath(u"changed.jpg"_s);
  ASSERT_TRUE(image.save(shared_cover_filename, "JPG"));
  ASSERT_TRUE(image.save(changed_cover_filename, "JPG"));

  Song changed_song = CreateSong(temp_dir, u"a.mp3"_s);
  Song unchanged_song = CreateSong(temp_dir, u"b.mp3"_s);
  ASSERT_TRUE(changed_song.IsEditable());
  ASSERT_TRUE(unchanged_song.IsEditable());
  changed_song.set_source(Song::Source::Collection);
  unchanged_song.set_source(Song::Source::Collection);
  changed_song.set_art_automatic(QUrl::fromLocalFile(changed_cover_filename));
  changed_song.set_art_manual(QUrl::fromLocalFile(shared_cover_filename));
  unchanged_song.set_art_manual(QUrl::fromLocalFile(shared_cover_filename));

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << changed_song << unchanged_song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    delete_cover_action->trigger();
    EXPECT_EQ(1, answer.shown());
  }

  {
    QFile file(changed_cover_filename);
    ASSERT_TRUE(file.open(QIODevice::Append));
    ASSERT_EQ(3, file.write("new"));
  }

  // A kept cover is not embedded either, like a cover which is not embedded yet when the embedded cover checkbox is checked.
  // Changing the cover sets the checkbox from the song, so it's set after deleting the cover.
  QCheckBox *checkbox_embedded_cover = editor.findChild<QCheckBox*>(u"checkbox_embedded_cover"_s);
  ASSERT_NE(nullptr, checkbox_embedded_cover);
  checkbox_embedded_cover->setChecked(true);

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_TRUE(QFile::exists(shared_cover_filename));
  EXPECT_TRUE(QFile::exists(changed_cover_filename));
  EXPECT_EQ(QUrl::fromLocalFile(shared_cover_filename), editor.songs().value(0).art_manual());
  EXPECT_EQ(QUrl::fromLocalFile(shared_cover_filename), editor.songs().value(1).art_manual());
  for (const Song &editor_song : editor.songs()) {
    EXPECT_FALSE(editor_song.art_embedded());
    Song saved_song;
    ASSERT_TRUE(tagreader_client_->ReadFileBlocking(editor_song.url().toLocalFile(), &saved_song).success());
    EXPECT_FALSE(saved_song.art_embedded());
  }

  QThreadPool::globalInstance()->waitForDone();

}

// Regression test: the embedded cover checkbox is from the selected song, so deleting the cover of a selected song without an embedded cover kept the embedded cover of another song of the album, which was handled as deleted.
TEST_F(SongEditorTest, TagEditorDeletesEmbeddedCoverOfOtherSongsOfAlbum) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song selected_song = CreateSong(temp_dir, u"a.mp3"_s);
  Song embedded_song = CreateSong(temp_dir, u"b.mp3"_s);
  ASSERT_TRUE(selected_song.IsEditable());
  ASSERT_TRUE(embedded_song.save_embedded_cover_supported());
  ASSERT_EQ(selected_song.album(), embedded_song.album());
  QImage embedded_cover(11, 13, QImage::Format_RGB32);
  embedded_cover.fill(Qt::green);
  ASSERT_TRUE(tagreader_client_->SaveCoverBlocking(embedded_song.url().toLocalFile(), SaveTagCoverData(QString(), ImageUtils::SaveImageToJpegData(embedded_cover), u"image/jpeg"_s)).success());
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(embedded_song.url().toLocalFile(), &embedded_song).success());
  ASSERT_TRUE(embedded_song.art_embedded());
  selected_song.set_source(Song::Source::Collection);
  embedded_song.set_source(Song::Source::Collection);
  const QString cover_filename = temp_dir.filePath(u"cover.jpg"_s);
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(Qt::red);
  ASSERT_TRUE(image.save(cover_filename, "JPG"));
  selected_song.set_art_manual(QUrl::fromLocalFile(cover_filename));

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << selected_song << embedded_song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

  // The track selection dialog of the tag fetcher has a song list with the same name.
  QListWidget *song_list = nullptr;
  const QList<QListWidget*> song_lists = editor.findChildren<QListWidget*>(u"song_list"_s);
  for (QListWidget *list : song_lists) {
    if (list->window() == &editor) song_list = list;
  }
  ASSERT_NE(nullptr, song_list);
  song_list->clearSelection();
  song_list->item(0)->setSelected(true);

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    delete_cover_action->trigger();
    EXPECT_EQ(1, answer.shown());
  }
  QCheckBox *checkbox_embedded_cover = editor.findChild<QCheckBox*>(u"checkbox_embedded_cover"_s);
  ASSERT_NE(nullptr, checkbox_embedded_cover);
  checkbox_embedded_cover->setChecked(false);

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_FALSE(QFile::exists(cover_filename));
  EXPECT_FALSE(editor.songs().value(1).art_embedded());
  Song saved_song;
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(embedded_song.url().toLocalFile(), &saved_song).success());
  EXPECT_FALSE(saved_song.art_embedded());

  QThreadPool::globalInstance()->waitForDone();

}

// Regression test: a new cover saved for one album could be saved to the cover file which was deleted for another album, and then the new cover was deleted.
TEST_F(SongEditorTest, TagEditorKeepsNewCoverSavedToDeletedCoverFile) {

  // The cover file is saved in the image cache, which is only redirected for the tests on Linux and BSD, keep it out of the users own cache on Windows and macOS.
  QStandardPaths::setTestModeEnabled(true);

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song new_cover_song = CreateSong(temp_dir, u"a.mp3"_s);
  Song deleted_cover_song = CreateSong(temp_dir, u"b.mp3"_s);
  ASSERT_TRUE(new_cover_song.IsEditable());
  ASSERT_TRUE(deleted_cover_song.IsEditable());
  ASSERT_FALSE(new_cover_song.effective_albumartist().isEmpty());
  ASSERT_FALSE(new_cover_song.album().isEmpty());
  new_cover_song.set_source(Song::Source::Collection);
  deleted_cover_song.set_source(Song::Source::Collection);

  // The new cover of the first song is saved to the same file as the cover of the second song, the covers are saved in the album cover cache by default.
  CoverOptions cover_options;
  cover_options.cover_type = CoverOptions::CoverType::Cache;
  const QString cover_filename = CoverUtils::CoverFilePath(cover_options, new_cover_song.source(), new_cover_song.effective_albumartist(), new_cover_song.album(), QString(), QString(), QUrl(), u"jpg"_s);
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(Qt::red);
  ASSERT_TRUE(image.save(cover_filename, "JPG"));
  deleted_cover_song.set_art_manual(QUrl::fromLocalFile(cover_filename));

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << new_cover_song << deleted_cover_song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

  // The track selection dialog of the tag fetcher has a song list with the same name.
  QListWidget *song_list = nullptr;
  const QList<QListWidget*> song_lists = editor.findChildren<QListWidget*>(u"song_list"_s);
  for (QListWidget *list : song_lists) {
    if (list->window() == &editor) song_list = list;
  }
  ASSERT_NE(nullptr, song_list);

  // Songs from different albums, the cover of the second song is deleted, and the first song gets a new cover.
  song_list->clearSelection();
  song_list->item(1)->setSelected(true);
  LineEdit *album = editor.findChild<LineEdit*>(u"album"_s);
  ASSERT_NE(nullptr, album);
  album->setText(u"Other album"_s);
  ASSERT_NE(editor.songs().value(0).album(), editor.songs().value(1).album());

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    // The cover file is in the cache, so the user is not asked.
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    delete_cover_action->trigger();
    EXPECT_EQ(0, answer.shown());
  }

  song_list->clearSelection();
  song_list->item(0)->setSelected(true);
  QImage new_cover(37, 23, QImage::Format_RGB32);
  new_cover.fill(Qt::blue);
  ASSERT_NO_FATAL_FAILURE(DropImageOnCover(&editor, new_cover));
  // Changing the cover sets the checkbox from the song, so it's set after dropping the image.
  QCheckBox *checkbox_embedded_cover = editor.findChild<QCheckBox*>(u"checkbox_embedded_cover"_s);
  ASSERT_NE(nullptr, checkbox_embedded_cover);
  checkbox_embedded_cover->setChecked(false);

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_EQ(QUrl::fromLocalFile(cover_filename), editor.songs().value(0).art_manual());
  EXPECT_TRUE(editor.songs().value(1).art_manual().isEmpty());
  QImage saved_cover;
  ASSERT_TRUE(saved_cover.load(cover_filename));
  EXPECT_EQ(new_cover.size(), saved_cover.size());

  QFile::remove(cover_filename);
  QThreadPool::globalInstance()->waitForDone();

  QStandardPaths::setTestModeEnabled(false);

}

#ifndef Q_OS_WIN
// The cover files which could not be deleted are kept as cover of the song, and the errors are shown.
TEST_F(SongEditorTest, TagEditorKeepsCoverFileWhichCantBeDeleted) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  Song song = CreateSong(temp_dir);
  ASSERT_TRUE(song.IsEditable());
  song.set_source(Song::Source::Collection);
  // A file in a read-only directory can't be deleted.
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"covers"_s));
  const QString covers_path = temp_dir.filePath(u"covers"_s);
  const QString cover_filename = temp_dir.filePath(u"covers/cover.jpg"_s);
  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(Qt::red);
  ASSERT_TRUE(image.save(cover_filename, "JPG"));
  song.set_art_manual(QUrl::fromLocalFile(cover_filename));
  const QString automatic_cover_filename = temp_dir.filePath(u"covers/automatic.jpg"_s);
  ASSERT_TRUE(image.save(automatic_cover_filename, "JPG"));
  song.set_art_automatic(QUrl::fromLocalFile(automatic_cover_filename));
  ASSERT_TRUE(QFile::setPermissions(covers_path, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
  const auto restore_permissions = qScopeGuard([&covers_path]() { QFile::setPermissions(covers_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner); });
  {
    QFile file(temp_dir.filePath(u"covers/test"_s));
    if (file.open(QIODevice::WriteOnly)) {
      GTEST_SKIP() << "The directory can be written without permissions, like when running as root.";
    }
  }

  SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
  TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
  ASSERT_TRUE(editor.SetSongs(SongList() << song));
  ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 1; }));

  AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
  ASSERT_NE(nullptr, album_cover_choice_controller);
  QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
  ASSERT_TRUE(delete_cover_action->isEnabled());
  {
    DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
    delete_cover_action->trigger();
    EXPECT_EQ(1, answer.shown());
  }

  QSignalSpy spy_accepted(&editor, &QDialog::accepted);
  QSignalSpy spy_error(&editor, &SongEditor::Error);
  editor.accept();
  ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
  EXPECT_TRUE(QFile::exists(cover_filename));
  EXPECT_TRUE(QFile::exists(automatic_cover_filename));
  EXPECT_EQ(QUrl::fromLocalFile(cover_filename), editor.songs().first().art_manual());
  EXPECT_EQ(QUrl::fromLocalFile(automatic_cover_filename), editor.songs().first().art_automatic());
  EXPECT_EQ(2, spy_error.count());

  QThreadPool::globalInstance()->waitForDone();

}
#endif

// Regression test: the cover file of a song was embedded when saving, after the cover file was deleted for another album which shares it, then the cover was not embedded.
// The shared cover file was only read before deleting it when the paths were the same.
TEST_F(SongEditorTest, TagEditorEmbedsCoverFileDeletedForAnotherAlbum) {

  // The cover file can also be shared through another path, like through a directory with a symbolic link.
  struct SharedPath {
    const char *description;
    QString path;
  };
  QList<SharedPath> shared_paths = {{"The same path", QString()}, {"A path with ..", u"covers/../covers"_s}};
#ifndef Q_OS_WIN
  shared_paths << SharedPath{"A symbolic link to the directory", u"link"_s};
#endif
  for (const SharedPath &shared_path : std::as_const(shared_paths)) {
    SCOPED_TRACE(shared_path.description);

    QTemporaryDir temp_dir;
    ASSERT_TRUE(temp_dir.isValid());
    ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"covers"_s));
#ifndef Q_OS_WIN
    ASSERT_TRUE(QFile::link(temp_dir.filePath(u"covers"_s), temp_dir.filePath(u"link"_s)));
#endif
    Song embedded_cover_song = CreateSong(temp_dir, u"a.mp3"_s);
    Song deleted_cover_song = CreateSong(temp_dir, u"b.mp3"_s);
    ASSERT_TRUE(embedded_cover_song.save_embedded_cover_supported());
    ASSERT_FALSE(embedded_cover_song.art_embedded());
    const QString shared_cover_filename = temp_dir.filePath(u"covers/shared.jpg"_s);
    const QString embedded_cover_filename = shared_path.path.isEmpty() ? shared_cover_filename : temp_dir.filePath(shared_path.path + u"/shared.jpg"_s);
    QImage image(17, 19, QImage::Format_RGB32);
    image.fill(Qt::red);
    ASSERT_TRUE(image.save(shared_cover_filename, "JPG"));
    embedded_cover_song.set_source(Song::Source::Collection);
    embedded_cover_song.set_art_manual(QUrl::fromLocalFile(embedded_cover_filename));
    deleted_cover_song.set_source(Song::Source::Collection);
    deleted_cover_song.set_art_manual(QUrl::fromLocalFile(shared_cover_filename));

    SharedPtr<AlbumCoverLoader> albumcover_loader = make_shared<AlbumCoverLoader>(tagreader_client_);
    TagEditor editor(make_shared<NetworkAccessManager>(), tagreader_client_, collection_backend_, albumcover_loader, make_shared<CurrentAlbumCoverLoader>(albumcover_loader), make_shared<CoverProviders>(), lyrics_providers_, make_shared<StreamingServices>());
    ASSERT_TRUE(editor.SetSongs(SongList() << embedded_cover_song << deleted_cover_song));
    ASSERT_TRUE(WaitFor([&editor]() { return editor.songs().count() == 2; }));

    // The track selection dialog of the tag fetcher has a song list with the same name.
    QListWidget *song_list = nullptr;
    const QList<QListWidget*> song_lists = editor.findChildren<QListWidget*>(u"song_list"_s);
    for (QListWidget *list : song_lists) {
      if (list->window() == &editor) song_list = list;
    }
    ASSERT_NE(nullptr, song_list);

    // Songs from different albums, the cover of the second song is deleted.
    song_list->clearSelection();
    song_list->item(1)->setSelected(true);
    LineEdit *album = editor.findChild<LineEdit*>(u"album"_s);
    ASSERT_NE(nullptr, album);
    album->setText(u"Other album"_s);
    ASSERT_NE(editor.songs().value(0).album(), editor.songs().value(1).album());

    AlbumCoverChoiceController *album_cover_choice_controller = editor.findChild<AlbumCoverChoiceController*>();
    ASSERT_NE(nullptr, album_cover_choice_controller);
    QAction *delete_cover_action = album_cover_choice_controller->delete_cover_action();
    ASSERT_TRUE(delete_cover_action->isEnabled());
    {
      DeleteConfirmationAnswer answer(QDialogButtonBox::Yes);
      delete_cover_action->trigger();
      EXPECT_EQ(1, answer.shown());
    }

    // The cover of the first song is embedded when saving.
    song_list->clearSelection();
    song_list->item(0)->setSelected(true);
    QCheckBox *checkbox_embedded_cover = editor.findChild<QCheckBox*>(u"checkbox_embedded_cover"_s);
    ASSERT_NE(nullptr, checkbox_embedded_cover);
    checkbox_embedded_cover->setChecked(true);

    QSignalSpy spy_accepted(&editor, &QDialog::accepted);
    editor.accept();
    ASSERT_TRUE(WaitFor([&spy_accepted]() { return spy_accepted.count() > 0; }));
    EXPECT_FALSE(QFile::exists(shared_cover_filename));
    EXPECT_TRUE(editor.songs().value(1).art_manual().isEmpty());
    QImage embedded_cover;
    ASSERT_TRUE(tagreader_client_->LoadCoverImageBlocking(embedded_cover_song.url().toLocalFile(), embedded_cover).success());
    EXPECT_EQ(image.size(), embedded_cover.size());

    QThreadPool::globalInstance()->waitForDone();
  }

}

}  // namespace
