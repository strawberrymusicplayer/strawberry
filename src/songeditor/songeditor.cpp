/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <memory>
#include <optional>

#include <QtGlobal>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QList>
#include <QMap>
#include <QHash>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QFile>
#include <QSaveFile>
#include <QAbstractButton>
#include <QtEvents>

#include "constants/timeconstants.h"
#include "constants/collectionsettings.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/settings.h"
#include "utilities/fileutils.h"
#include "utilities/lrcutils.h"
#include "tagreader/tagreaderclient.h"
#include "widgets/busyindicator.h"
#include "widgets/lineedit.h"
#include "collection/collectionbackend.h"
#include "lyrics/lyricsfetcher.h"
#include "lyrics/lyricsprovider.h"
#include "lyricssearchdialog.h"
#include "lyricstab.h"
#include "songeditor.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kGeometry[] = "geometry";
constexpr char kCurrentTab[] = "current_tab";
constexpr char kLyricsSearchType[] = "lyrics_search_type";
}  // namespace

const char SongEditor::kTagsDifferentHintText[] = QT_TR_NOOP("(different across multiple songs)");

SongEditor::SongEditor(const SharedPtr<TagReaderClient> tagreader_client,
                       const SharedPtr<CollectionBackend> collection_backend,
                       const SharedPtr<LyricsProviders> lyrics_providers,
                       QWidget *parent)
    : QDialog(parent),
      tagreader_client_(tagreader_client),
      collection_backend_(collection_backend),
      ignore_edits_(false),
      save_tag_pending_(0),
      song_list_(nullptr),
      tab_widget_(nullptr),
      button_box_(nullptr),
      loading_indicator_(nullptr),
      lyrics_tab_(nullptr),
      synced_lyrics_tab_(nullptr),
      previous_button_(nullptr),
      next_button_(nullptr),
      lyrics_fetcher_(new LyricsFetcher(lyrics_providers, this)),
      lyrics_search_dialog_(new LyricsSearchDialog(lyrics_providers, this)),
      loading_(false),
      songs_pending_(false),
      lyrics_requests_count_(0),
      lyrics_search_type_(LyricsSearchType::Both),
      songs_generation_(0),
      save_lyrics_to_tags_(CollectionSettings::kDefaultSaveLyrics) {

  QObject::connect(lyrics_fetcher_, &LyricsFetcher::LyricsFetched, this, &SongEditor::UpdateLyrics);

  // Don't open the dialog again with the songs set while loading or saving when the dialog was closed.
  QObject::connect(this, &QDialog::rejected, this, &SongEditor::ClearPendingSongs);
  QObject::connect(this, &QDialog::rejected, this, &SongEditor::CancelFetchLyrics);

}

void SongEditor::InitSongEditor(QListWidget *song_list, QTabWidget *tab_widget, QDialogButtonBox *button_box, BusyIndicator *loading_indicator) {

  song_list_ = song_list;
  tab_widget_ = tab_widget;
  button_box_ = button_box;
  loading_indicator_ = loading_indicator;
  loading_indicator_->hide();

  lyrics_tab_ = new LyricsTab(false, tab_widget_);
  synced_lyrics_tab_ = new LyricsTab(true, tab_widget_);
  tab_widget_->addTab(lyrics_tab_, tr("Lyrics"));
  tab_widget_->addTab(synced_lyrics_tab_, tr("Synced lyrics"));

  QObject::connect(lyrics_tab_, &LyricsTab::StorageChanged, this, &SongEditor::LyricsStorageChanged);
  QObject::connect(synced_lyrics_tab_, &LyricsTab::StorageChanged, this, &SongEditor::SyncedLyricsStorageChanged);

  // The search controls are the same in both lyrics tabs, and search for what is selected in them, the selection is saved for the editor.
  {
    Settings s;
    s.beginGroup(SettingsGroup());
    const int search_type = s.value(kLyricsSearchType, static_cast<int>(LyricsSearchType::Both)).toInt();
    s.endGroup();
    if (search_type >= static_cast<int>(LyricsSearchType::Lyrics) && search_type <= static_cast<int>(LyricsSearchType::Both)) {
      lyrics_search_type_ = static_cast<LyricsSearchType>(search_type);
    }
  }
  for (LyricsTab *lyrics_tab : {lyrics_tab_, synced_lyrics_tab_}) {
    {
      const QSignalBlocker signal_blocker(lyrics_tab->search_type());
      lyrics_tab->search_type()->setCurrentIndex(lyrics_tab->search_type()->findData(static_cast<int>(lyrics_search_type_)));
    }
    QObject::connect(lyrics_tab, &LyricsTab::FetchLyrics, this, &SongEditor::FetchLyrics);
    QObject::connect(lyrics_tab, &LyricsTab::SearchLyrics, this, &SongEditor::SearchLyrics);
    QObject::connect(lyrics_tab, &LyricsTab::SearchTypeChanged, this, &SongEditor::SetLyricsSearchType);
  }

  // An editable field is one that has a label as a buddy.
  // The label is important because it gets turned bold when the field is changed.
  // Only the labels in the tabs, not the labels of child dialogs like the lyrics search dialog.
  const QList<QLabel*> labels = tab_widget_->findChildren<QLabel*>();
  for (QLabel *label : labels) {
    QWidget *widget = label->buddy();
    // Combo boxes have their own handling, the buddy is only set for accessibility.
    if (widget && !qobject_cast<QComboBox*>(widget)) {
      // Store information about the field
      fields_ << FieldData(label, widget, widget->objectName());  // clazy:exclude=reserve-candidates

      // Connect the edited signal
      if (LineEdit *lineedit = qobject_cast<LineEdit*>(widget)) {
        QObject::connect(lineedit, &LineEdit::textChanged, this, &SongEditor::FieldValueEdited);
        QObject::connect(lineedit, &LineEdit::Reset, this, &SongEditor::ResetField);
      }
      else if (TextEdit *textedit = qobject_cast<TextEdit*>(widget)) {
        QObject::connect(textedit, &TextEdit::textChanged, this, &SongEditor::FieldValueEdited);
        QObject::connect(textedit, &TextEdit::Reset, this, &SongEditor::ResetField);
      }
      else if (SpinBox *spinbox = qobject_cast<SpinBox*>(widget)) {
        QObject::connect(spinbox, QOverload<int>::of(&SpinBox::valueChanged), this, &SongEditor::FieldValueEdited);
        QObject::connect(spinbox, &SpinBox::Reset, this, &SongEditor::ResetField);
        spinbox->setFocusPolicy(Qt::StrongFocus);
        spinbox->installEventFilter(this);
      }
      else if (CheckBox *checkbox = qobject_cast<CheckBox*>(widget)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(checkbox, &QCheckBox::checkStateChanged, this, &SongEditor::FieldValueEdited);
#else
        QObject::connect(checkbox, &QCheckBox::stateChanged, this, &SongEditor::FieldValueEdited);
#endif
        QObject::connect(checkbox, &CheckBox::Reset, this, &SongEditor::ResetField);
      }
      else if (RatingBox *ratingbox = qobject_cast<RatingBox*>(widget)) {
        QObject::connect(ratingbox, &RatingWidget::RatingChanged, this, &SongEditor::FieldValueEdited);
        QObject::connect(ratingbox, &RatingBox::Reset, this, &SongEditor::ResetField);
      }
    }
  }

  QObject::connect(song_list_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SongEditor::SelectionChanged);
  QObject::connect(button_box_, &QDialogButtonBox::clicked, this, &SongEditor::ButtonClicked);

  // Add the next/previous buttons
  previous_button_ = new QPushButton(IconLoader::Load(u"go-previous"_s), tr("Previous"), this);
  next_button_ = new QPushButton(IconLoader::Load(u"go-next"_s), tr("Next"), this);
  button_box_->addButton(previous_button_, QDialogButtonBox::ResetRole);
  button_box_->addButton(next_button_, QDialogButtonBox::ResetRole);

  QObject::connect(previous_button_, &QPushButton::clicked, this, &SongEditor::PreviousSong);
  QObject::connect(next_button_, &QPushButton::clicked, this, &SongEditor::NextSong);

  // Set some shortcuts for the buttons
  new QShortcut(QKeySequence::Back, previous_button_, SLOT(click()));
  new QShortcut(QKeySequence::Forward, next_button_, SLOT(click()));
  new QShortcut(QKeySequence::MoveToPreviousPage, previous_button_, SLOT(click()));
  new QShortcut(QKeySequence::MoveToNextPage, next_button_, SLOT(click()));

  // Show the shortcuts as tooltips
  previous_button_->setToolTip(QStringLiteral("%1 (%2 / %3)").arg(
      previous_button_->text(),
      QKeySequence(QKeySequence::Back).toString(QKeySequence::NativeText),
      QKeySequence(QKeySequence::MoveToPreviousPage).toString(QKeySequence::NativeText)));
  next_button_->setToolTip(QStringLiteral("%1 (%2 / %3)").arg(
      next_button_->text(),
      QKeySequence(QKeySequence::Forward).toString(QKeySequence::NativeText),
      QKeySequence(QKeySequence::MoveToNextPage).toString(QKeySequence::NativeText)));

}

void SongEditor::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {

    // Set the dialog's height to the smallest possible
    resize(width(), sizeHint().height());

    // Restore the tab that was current last time.
    Settings s;
    s.beginGroup(SettingsGroup());
    if (s.contains(kGeometry)) {
      restoreGeometry(s.value(kGeometry).toByteArray());
    }
    tab_widget_->setCurrentIndex(s.value(kCurrentTab).toInt());
    s.endGroup();

  }

  QDialog::showEvent(e);

}

void SongEditor::hideEvent(QHideEvent *e) {

  // Save the current tab
  Settings s;
  s.beginGroup(SettingsGroup());
  s.setValue(kGeometry, saveGeometry());
  s.setValue(kCurrentTab, tab_widget_->currentIndex());
  s.endGroup();

  QDialog::hideEvent(e);

}

void SongEditor::accept() {

  // Validate edited synchronized lyrics up front, a failed write would otherwise still close the dialog and the edits would be lost.
  for (int i = 0; i < data_.count(); ++i) {
    const Data &ref = data_.at(i);
    if (ref.current_.synced_lyrics() != ref.original_.synced_lyrics() && !Utilities::ParseLRC(ref.current_.synced_lyrics()).has_value()) {
      song_list_->setCurrentRow(i);
      tab_widget_->setCurrentWidget(synced_lyrics_tab_);
      QMessageBox::critical(this, tr("Invalid synchronized lyrics"), tr("The synchronized lyrics for %1 are not valid LRC. Lyric lines must start with a timestamp, for example [01:23.45]. Only blank lines and metadata tags such as [ar:Artist] can be without one.").arg(ref.current_.basefilename()));
      return;
    }
  }

  // Show the loading indicator
  if (!SetLoading(tr("Saving tracks") + u"..."_s)) return;

  // Lyrics fetched after saving started would not be saved.
  CancelFetchLyrics();

  Save();

}

bool SongEditor::SetLoading(const QString &message) {

  const bool loading = !message.isEmpty();
  if (loading == loading_) return false;
  loading_ = loading;

  button_box_->setEnabled(!loading);
  tab_widget_->setEnabled(!loading);
  song_list_->setEnabled(!loading);
  loading_indicator_->setVisible(loading);
  loading_indicator_->set_text(message);

  LoadingChanged(loading);

  return true;

}

void SongEditor::SetSongsFinished() {

  QFutureWatcher<QList<Data>> *watcher = static_cast<QFutureWatcher<QList<Data>>*>(sender());
  QList<Data> result_data = watcher->result();
  watcher->deleteLater();

  if (!SetLoading(QString())) return;

  // Other songs were set while loading these, load those instead.
  if (songs_pending_) {
    SetPendingSongs();
    return;
  }

  data_ = result_data;

  if (data_.count() == 0) {
    // If there were no valid songs, disable everything
    song_list_->setEnabled(false);
    tab_widget_->setEnabled(false);
    NoSongs();
    SetSongListVisibility(false);
    return;
  }

  // Add the filenames to the list, streams can be without a filename.
  for (const Data &tag_data : std::as_const(data_)) {
    song_list_->addItem(tag_data.current_.basefilename().isEmpty() ? tag_data.current_.url().toString() : tag_data.current_.basefilename());
  }

  // Select all
  song_list_->setCurrentRow(0);
  song_list_->selectAll();

  // Hide the list if there's only one song in it
  SetSongListVisibility(data_.count() != 1);

}

QModelIndexList SongEditor::SelectedIndexes() const {

  return song_list_->selectionModel()->selectedIndexes();

}

void SongEditor::SelectionChanged() {

  // Fetching lyrics continues, the lyrics are set for the songs they were fetched for, not for the selected songs.

  const QModelIndexList indexes = SelectedIndexes();
  if (indexes.isEmpty()) return;

  // Set the editable fields
  UpdateUI(indexes);

  bool lyrics_enabled = false;
  bool synced_lyrics_enabled = false;
  for (const QModelIndex &idx : indexes) {
    const Song &song = data_.at(idx.row()).original_;
    if (LyricsStorageAvailable(song, false)) {
      lyrics_enabled = true;
    }
    if (LyricsStorageAvailable(song, true)) {
      synced_lyrics_enabled = true;
    }
  }

  // The storage of the lyrics can be different for each song, it is only changed for a single song, otherwise each song keeps its storage.
  for (LyricsTab *lyrics_tab : {lyrics_tab_, synced_lyrics_tab_}) {
    const bool enabled = lyrics_tab->synced() ? synced_lyrics_enabled : lyrics_enabled;
    lyrics_tab->lyrics()->setEnabled(enabled);
    lyrics_tab->storage()->setEnabled(enabled && indexes.count() == 1);
    if (indexes.count() == 1) {
      UpdateStorageComboBox(lyrics_tab->storage(), data_.at(indexes.first().row()), lyrics_tab->synced());
    }
  }

  // The lyrics can be completed automatically for multiple songs, choosing the lyrics from a search is only for a single song.
  UpdateLyricsSearchControls(indexes);

  UpdateSelection(indexes);

}

void SongEditor::PrepareLyricsForSave(Data *tag_data, bool *lyrics_changed, bool *synced_lyrics_changed) {

  Data &ref = *tag_data;

  // The database is only preselected for lyrics not stored anywhere, if none were entered they are still not stored anywhere.
  // Lyrics cleared on purpose with the database selected are kept as empty lyrics in the database, so they are not restored from the tags or an LRC file.
  if (ref.stored_lyrics_storage_type_ == Song::StorageType::None && ref.original_.lyrics().isEmpty() && ref.current_.lyrics().isEmpty() && ref.current_.lyrics_storage_type() == Song::StorageType::Database) {
    ref.original_.set_lyrics_storage_type(Song::StorageType::None);
    ref.current_.set_lyrics_storage_type(Song::StorageType::None);
  }
  if (ref.stored_synced_lyrics_storage_type_ == Song::StorageType::None && ref.original_.synced_lyrics().isEmpty() && ref.current_.synced_lyrics().isEmpty() && ref.current_.synced_lyrics_storage_type() == Song::StorageType::Database) {
    ref.original_.set_synced_lyrics_storage_type(Song::StorageType::None);
    ref.current_.set_synced_lyrics_storage_type(Song::StorageType::None);
  }

  // Write the LRC file first, if that fails the synchronized lyrics are not saved anywhere, so the collection does not refer to an LRC file which was not written.
  if ((ref.current_.synced_lyrics() != ref.original_.synced_lyrics() || ref.current_.synced_lyrics_storage_type() != ref.original_.synced_lyrics_storage_type()) && ref.current_.synced_lyrics_storage_type() == Song::StorageType::File && !SaveSyncedLyricsToLrcFile(&ref.current_)) {
    Q_EMIT Error(tr("Could not write synchronized lyrics to LRC file for %1").arg(ref.current_.url().toLocalFile()));
    ref.current_.set_synced_lyrics(ref.original_.synced_lyrics());
    ref.current_.set_synced_lyrics_storage_type(ref.original_.synced_lyrics_storage_type());
    ref.current_.set_lrc_mtime(ref.original_.lrc_mtime());
  }
  // Cleared synchronized lyrics in an LRC file means the LRC file was removed above, so they are not stored anywhere now.
  else if (ref.current_.synced_lyrics().isEmpty() && ref.current_.synced_lyrics_storage_type() == Song::StorageType::File) {
    ref.current_.set_synced_lyrics_storage_type(Song::StorageType::None);
  }

  *lyrics_changed = ref.current_.lyrics() != ref.original_.lyrics() || ref.current_.lyrics_storage_type() != ref.original_.lyrics_storage_type();
  *synced_lyrics_changed = ref.current_.synced_lyrics() != ref.original_.synced_lyrics() || ref.current_.synced_lyrics_storage_type() != ref.original_.synced_lyrics_storage_type();

}

void SongEditor::SongSaveFinished(const bool success, const QString &error, const QString &filename, const Song &song, const Song &song_lyrics_not_in_tags) {

  --save_tag_pending_;

  if (success) {
    if (song.is_local_collection_song()) {
      collection_songs_.insert(song.id(), song);
    }
  }
  else {
    if (error.isEmpty()) {
      Q_EMIT Error(tr("Could not write metadata to %1").arg(filename));
    }
    else {
      Q_EMIT Error(tr("Could not write metadata to %1: %2").arg(filename, error));
    }
    // The original song with only the lyrics saved to the database or an LRC file, which are saved even though writing the tags failed.
    if (song_lyrics_not_in_tags.is_valid() && song_lyrics_not_in_tags.is_local_collection_song()) {
      collection_songs_.insert(song_lyrics_not_in_tags.id(), song_lyrics_not_in_tags);
    }
  }

  if (save_tag_pending_ <= 0) SaveDataFinished();

}

QVariant SongEditor::Data::value(const Song &song, const QString &id) {

  if (id == "title"_L1) return song.title();
  if (id == "titlesort"_L1) return song.titlesort();
  if (id == "artist"_L1) return song.artist();
  if (id == "artistsort"_L1) return song.artistsort();
  if (id == "album"_L1) return song.album();
  if (id == "albumsort"_L1) return song.albumsort();
  if (id == "albumartist"_L1) return song.albumartist();
  if (id == "albumartistsort"_L1) return song.albumartistsort();
  if (id == "composer"_L1) return song.composer();
  if (id == "composersort"_L1) return song.composersort();
  if (id == "performer"_L1) return song.performer();
  if (id == "performersort"_L1) return song.performersort();
  if (id == "grouping"_L1) return song.grouping();
  if (id == "genre"_L1) return song.genre();
  if (id == "comment"_L1) return song.comment();
  if (id == "lyrics"_L1) return song.lyrics();
  if (id == "synced_lyrics"_L1) return song.synced_lyrics();
  if (id == "track"_L1) return song.track();
  if (id == "disc"_L1) return song.disc();
  if (id == "year"_L1) return song.year();
  if (id == "compilation"_L1) return song.compilation();
  if (id == "rating"_L1) { return song.rating(); }
  qLog(Warning) << "Unknown ID" << id;
  return QVariant();

}

void SongEditor::Data::set_value(const QString &id, const QVariant &value) {

  if (id == "title"_L1) current_.set_title(value.toString());
  else if (id == "titlesort"_L1) current_.set_titlesort(value.toString());
  else if (id == "artist"_L1) current_.set_artist(value.toString());
  else if (id == "artistsort"_L1) current_.set_artistsort(value.toString());
  else if (id == "album"_L1) current_.set_album(value.toString());
  else if (id == "albumsort"_L1) current_.set_albumsort(value.toString());
  else if (id == "albumartist"_L1) current_.set_albumartist(value.toString());
  else if (id == "albumartistsort"_L1) current_.set_albumartistsort(value.toString());
  else if (id == "composer"_L1) current_.set_composer(value.toString());
  else if (id == "composersort"_L1) current_.set_composersort(value.toString());
  else if (id == "performer"_L1) current_.set_performer(value.toString());
  else if (id == "performersort"_L1) current_.set_performersort(value.toString());
  else if (id == "grouping"_L1) current_.set_grouping(value.toString());
  else if (id == "genre"_L1) current_.set_genre(value.toString());
  else if (id == "comment"_L1) current_.set_comment(value.toString());
  else if (id == "lyrics"_L1) current_.set_lyrics(value.toString());
  else if (id == "synced_lyrics"_L1) current_.set_synced_lyrics(value.toString());
  else if (id == "track"_L1) current_.set_track(value.toInt());
  else if (id == "disc"_L1) current_.set_disc(value.toInt());
  else if (id == "year"_L1) current_.set_year(value.toInt());
  else if (id == "compilation"_L1) current_.set_compilation(value.toBool());
  else if (id == "rating"_L1) { current_.set_rating(value.toFloat()); }
  else qLog(Warning) << "Unknown ID" << id;

}

PlaylistItemPtrList SongEditor::playlist_items() const {

  if (playlist_items_.isEmpty()) return PlaylistItemPtrList();

  PlaylistItemPtrList items;
  items.reserve(data_.count());
  for (const Data &d : data_) {
    items << playlist_items_.value(d.index_);
  }

  return items;

}

SongList SongEditor::original_songs() const {

  SongList result;
  result.reserve(data_.count());
  for (const Data &d : data_) {
    result << d.original_;
  }

  return result;

}

SongList SongEditor::songs() const {

  SongList result;
  result.reserve(data_.count());
  for (const Data &d : data_) {
    result << d.current_;
  }

  return result;

}

QList<SongEditor::Data> SongEditor::LoadData(const SongList &songs) const {

  QList<Data> ret;

  for (qsizetype i = 0; i < songs.count(); ++i) {
    const Song &song = songs.at(i);
    if (!song.IsEditable()) continue;
    // Streams have no file to reload the tags from, their metadata is applied to the playlist item.
    if (song.is_stream()) {
      Data tag_data(song, i);
      SetAvailableStorageTypes(&tag_data.original_, save_lyrics_to_tags_);
      tag_data.current_ = tag_data.original_;
      ret << tag_data;
      continue;
    }
    // Try reloading the tags from file
    Song copy(song);
    const TagReaderResult result = tagreader_client_->ReadFileBlocking(copy.url().toLocalFile(), &copy);
    if (result.success() && copy.is_valid()) {
      copy.MergeUserSetData(song, false, false);
      // The tags were read again above, lyrics which are not stored in the tags are only in the song from the collection.
      // Empty lyrics in the database are lyrics cleared on purpose, so they are not replaced by lyrics in the tags.
      if (song.lyrics_storage_type() == Song::StorageType::Database) {
        copy.set_lyrics(song.lyrics());
        copy.set_lyrics_storage_type(song.lyrics_storage_type());
      }
      if (song.synced_lyrics_storage_type() == Song::StorageType::Database || (song.synced_lyrics_storage_type() == Song::StorageType::File && !song.synced_lyrics().isEmpty())) {
        copy.set_synced_lyrics(song.synced_lyrics());
        copy.set_synced_lyrics_storage_type(song.synced_lyrics_storage_type());
      }
      Data tag_data(copy, i);
      SetAvailableStorageTypes(&tag_data.original_, save_lyrics_to_tags_);
      tag_data.current_ = tag_data.original_;
      ret << tag_data;
    }
  }

  return ret;

}

bool SongEditor::SetSongs(const SongList &s, const PlaylistItemPtrList &items) {

  // Show the loading indicator
  // While the previous songs are still loading or saving, keep the songs and set them when it's finished, so they are not ignored.
  if (!SetLoading(tr("Loading tracks") + u"..."_s)) {
    songs_pending_ = true;
    pending_songs_ = s;
    pending_playlist_items_ = items;
    return false;
  }

  // The fetched lyrics are for the songs which are replaced.
  CancelFetchLyrics();
  ++songs_generation_;

  data_.clear();
  playlist_items_ = items;
  song_list_->clear();
  collection_songs_.clear();
  collection_lyrics_songs_.clear();
  collection_synced_lyrics_songs_.clear();

  {
    Settings settings;
    settings.beginGroup(CollectionSettings::kSettingsGroup);
    save_lyrics_to_tags_ = settings.value(CollectionSettings::kSaveLyrics, CollectionSettings::kDefaultSaveLyrics).toBool();
    settings.endGroup();
  }

  // Reload tags in the background
  QFuture<QList<Data>> future = QtConcurrent::run(&SongEditor::LoadData, this, s);
  QFutureWatcher<QList<Data>> *watcher = new QFutureWatcher<QList<Data>>(this);
  QObject::connect(watcher, &QFutureWatcher<QList<Data>>::finished, this, &SongEditor::SetSongsFinished);
  watcher->setFuture(future);

  return true;

}

void SongEditor::SetSongListVisibility(bool visible) {

  song_list_->setVisible(visible);
  previous_button_->setEnabled(visible);
  next_button_->setEnabled(visible);

}

bool SongEditor::DoesValueVary(const QModelIndexList &sel, const QString &id) const {

  QVariant value = data_[sel.first().row()].current_value(id);
  for (int i = 1; i < sel.count(); ++i) {
    if (value != data_[sel[i].row()].current_value(id)) return true;
  }
  return false;

}

bool SongEditor::IsValueModified(const QModelIndexList &sel, const QString &id) const {

  if (id == u"track"_s || id == u"disc"_s || id == u"year"_s) {
    return std::any_of(sel.begin(), sel.end(), [this, id](const QModelIndex &i) {
      const int original = data_[i.row()].original_value(id).toInt();
      const int current = data_[i.row()].current_value(id).toInt();
      return original != current && (original != -1 || current != 0);
    });
  }
  else if (id == u"rating"_s) {
    return std::any_of(sel.begin(), sel.end(), [this, id](const QModelIndex &i) {
      const float original = data_[i.row()].original_value(id).toFloat();
      const float current = data_[i.row()].current_value(id).toFloat();
      return original != current && (original != -1 || current != 0);
    });
  }
  return std::any_of(sel.begin(), sel.end(), [this, id](const QModelIndex &i) { return data_[i.row()].original_value(id) != data_[i.row()].current_value(id); });

}

void SongEditor::InitFieldValue(const FieldData &field, const QModelIndexList &sel) {

  const bool varies = DoesValueVary(sel, field.id_);

  if (ExtendedEditor *editor = dynamic_cast<ExtendedEditor*>(field.editor_)) {
    editor->clear();
    editor->clear_hint();
    if (varies) {
      editor->set_hint(tr(kTagsDifferentHintText));
      editor->set_partially();
    }
    else {
      editor->set_value(data_.value(sel.value(0).row()).current_value(field.id_));
    }
  }
  else if (field.editor_) {
    qLog(Error) << "Missing editor for" << field.editor_->objectName();
  }

  UpdateModifiedField(field, sel);

}

void SongEditor::UpdateFieldValue(const FieldData &field, const QModelIndexList &sel) {

  // Get the value from the field
  QVariant value;

  if (ExtendedEditor *editor = dynamic_cast<ExtendedEditor*>(field.editor_)) {
    value = editor->value();
  }
  else if (field.editor_) {
    qLog(Error) << "Missing editor for" << field.editor_->objectName();
  }

  // Did we get it?
  if (!value.isValid()) {
    return;
  }

  // Set it in each selected song
  for (const QModelIndex &i : sel) {
    data_[i.row()].set_value(field.id_, value);
  }

  UpdateModifiedField(field, sel);

}

void SongEditor::UpdateModifiedField(const FieldData &field, const QModelIndexList &sel) {

  const bool modified = IsValueModified(sel, field.id_);

  // Update the boldness
  QFont new_font(font());
  new_font.setBold(modified);
  field.label_->setFont(new_font);
  if (field.editor_) {
    if (ExtendedEditor *editor = dynamic_cast<ExtendedEditor*>(field.editor_)) {
      editor->set_font(new_font);
      editor->set_reset_button(modified);
    }
    else {
      field.editor_->setFont(new_font);
    }
  }

}

void SongEditor::ResetFieldValue(const FieldData &field, const QModelIndexList &sel) {

  // Reset each selected song
  for (const QModelIndex &i : sel) {
    Data tag_data = data_.value(i.row());
    tag_data.set_value(field.id_, tag_data.original_value(field.id_));
    data_[i.row()] = tag_data;
  }

  // Reset the field
  InitFieldValue(field, sel);

}

void SongEditor::UpdateUI(const QModelIndexList &indexes) {

  ignore_edits_ = true;
  for (const FieldData &field : std::as_const(fields_)) {
    InitFieldValue(field, indexes);
  }
  ignore_edits_ = false;

}

void SongEditor::FieldValueEdited() {

  if (ignore_edits_) return;

  const QModelIndexList sel = song_list_->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) {
    return;
  }

  QWidget *w = qobject_cast<QWidget*>(sender());

  // Find the field
  for (const FieldData &field : std::as_const(fields_)) {
    if (field.editor_ == w) {
      UpdateFieldValue(field, sel);
      return;
    }
  }

}

void SongEditor::ResetField() {

  const QModelIndexList sel = song_list_->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) {
    return;
  }

  QWidget *w = qobject_cast<QWidget*>(sender());

  // Find the field
  for (const FieldData &field : std::as_const(fields_)) {
    if (field.editor_ == w) {
      ignore_edits_ = true;
      ResetFieldValue(field, sel);
      ignore_edits_ = false;
      return;
    }
  }

}

void SongEditor::NextSong() {

  if (song_list_->count() == 0) {
    return;
  }

  int row = (song_list_->currentRow() + 1) % song_list_->count();
  song_list_->setCurrentRow(row);

}

void SongEditor::PreviousSong() {

  if (song_list_->count() == 0) {
    return;
  }

  int row = (song_list_->currentRow() - 1 + song_list_->count()) % song_list_->count();
  song_list_->setCurrentRow(row);

}

void SongEditor::ButtonClicked(QAbstractButton *button) {

  if (button == button_box_->button(QDialogButtonBox::Discard)) {
    reject();
  }

}

void SongEditor::SaveDataFinished() {

  // The collection songs as they were saved, with what was saved when saving failed, to update the songs set while saving.
  QMap<int, Song> saved_collection_songs = collection_songs_;
  saved_collection_songs.insert(collection_lyrics_songs_);
  saved_collection_songs.insert(collection_synced_lyrics_songs_);

  if (!collection_songs_.isEmpty()) {
    collection_backend_->AddOrUpdateSongsAsync(collection_songs_.values());
    collection_songs_.clear();
  }

  // Only the changed lyrics of these songs were saved, other changes to the songs since they were loaded, like the playcount or the other lyrics, are kept.
  if (!collection_lyrics_songs_.isEmpty() || !collection_synced_lyrics_songs_.isEmpty()) {
    collection_backend_->UpdateSongsLyricsAsync(collection_lyrics_songs_.values(), collection_synced_lyrics_songs_.values());
    collection_lyrics_songs_.clear();
    collection_synced_lyrics_songs_.clear();
  }

  if (!SetLoading(QString())) return;

  QDialog::accept();

  // Other songs were set while saving, open the dialog again with those.
  SetPendingSongs(saved_collection_songs);

}

void SongEditor::SetPendingSongs(const QMap<int, Song> &saved_collection_songs) {

  if (!songs_pending_) return;

  SongList songs = pending_songs_;
  const PlaylistItemPtrList items = pending_playlist_items_;
  ClearPendingSongs();

  // The songs were set before the save finished, so songs which were saved have the metadata from before.
  // The tags are read again from the files, but the lyrics stored in the database and the statistics are taken from these songs, so use the saved songs instead.
  for (qint64 i = 0; i < songs.count(); ++i) {
    Song &song = songs[i];
    // Streams are not read again, and they are not in the collection, the edits to them were applied to their playlist items when the dialog was accepted.
    if (song.is_stream()) {
      const PlaylistItemPtr item = items.value(i);
      if (item) {
        song = item->OriginalMetadata();
      }
      continue;
    }
    if (song.id() == -1 || !saved_collection_songs.contains(song.id())) continue;
    const Song &saved_song = saved_collection_songs[song.id()];
    if (saved_song.source() == song.source() && saved_song.url() == song.url()) {
      song = saved_song;
    }
  }

  if (!SetSongs(songs, items)) return;

  Q_EMIT PendingSongsSet();

  show();
  raise();

}

void SongEditor::ClearPendingSongs() {

  songs_pending_ = false;
  pending_songs_.clear();
  pending_playlist_items_.clear();

}

void SongEditor::FetchLyrics() {

  const QModelIndexList indexes = SelectedIndexes();
  if (indexes.isEmpty()) return;

  CancelFetchLyrics();

  // The best lyrics are used for each selected song which is missing them, the lyrics a song has are not replaced.
  // The songs which can't save the lyrics are skipped, so they are not counted as found.
  // Lyrics and synchronized lyrics are searched separately, since only some providers have synchronized lyrics.
  for (const QModelIndex &idx : indexes) {
    const Song &song = data_.at(idx.row()).current_;
    for (const bool synced : {false, true}) {
      if (synced ? !LyricsSearchTypeHasSyncedLyrics(lyrics_search_type_) : !LyricsSearchTypeHasLyrics(lyrics_search_type_)) continue;
      if (!(synced ? song.synced_lyrics() : song.lyrics()).isEmpty()) continue;
      if (!LyricsStorageAvailable(song, synced)) continue;
      const quint64 id = lyrics_fetcher_->Search(song.effective_albumartist(), song.artist(), song.album(), song.title(), song.length_nanosec() / kNsecPerSec, synced);
      lyrics_requests_.insert(id, LyricsRequest{idx.row(), synced});
      lyrics_rows_searched_.insert(idx.row());
    }
  }
  lyrics_requests_count_ = static_cast<int>(lyrics_requests_.count());

  if (lyrics_requests_count_ == 0) {
    SetLyricsFetchStatus(indexes.count() == 1 ? tr("The song already has lyrics.") : tr("The selected songs already have lyrics."));
    return;
  }

  // The status is not shown in the lyrics field, since the field is the lyrics of the song, so the status would be saved as the lyrics.
  SetLyricsFetchStatus(tr("loading..."));

}

void SongEditor::UpdateLyrics(const quint64 id, const LyricsSearchResult &result) {

  if (!lyrics_requests_.contains(id)) return;
  const LyricsRequest request = lyrics_requests_.take(id);

  const QString lyrics = request.synced ? result.synced_lyrics : result.lyrics;
  const QString field_id = request.synced ? u"synced_lyrics"_s : u"lyrics"_s;
  // Lyrics entered while searching are not replaced.
  if (!lyrics.isEmpty() && request.row < data_.count() && data_.at(request.row).current_value(field_id).toString().isEmpty()) {
    lyrics_rows_found_.insert(request.row);
    SetFieldValue(request.row, field_id, lyrics);
  }

  UpdateLyricsFetchStatus();

}

void SongEditor::UpdateLyricsFetchStatus() {

  if (lyrics_requests_count_ == 0) {
    SetLyricsFetchStatus(QString());
    return;
  }

  if (!lyrics_requests_.isEmpty()) {
    SetLyricsFetchStatus(tr("loading... (%1 of %2)").arg(lyrics_requests_count_ - lyrics_requests_.count()).arg(lyrics_requests_count_));
    return;
  }

  if (lyrics_rows_searched_.count() == 1) {
    SetLyricsFetchStatus(lyrics_rows_found_.isEmpty() ? tr("Not found.") : QString());
  }
  else {
    SetLyricsFetchStatus(tr("Found lyrics for %1 of %2 songs.").arg(lyrics_rows_found_.count()).arg(lyrics_rows_searched_.count()));
  }

}

void SongEditor::SetLyricsFetchStatus(const QString &text) {

  lyrics_tab_->status()->setText(text);
  synced_lyrics_tab_->status()->setText(text);

}

void SongEditor::CancelFetchLyrics() {

  lyrics_fetcher_->Clear();
  lyrics_requests_.clear();
  lyrics_requests_count_ = 0;
  lyrics_rows_searched_.clear();
  lyrics_rows_found_.clear();
  SetLyricsFetchStatus(QString());

}

void SongEditor::CancelFetchLyricsForSong(const int row, const bool synced) {

  bool fetch_cancelled = false;
  for (QHash<quint64, LyricsRequest>::iterator it = lyrics_requests_.begin(); it != lyrics_requests_.end();) {
    if (it.value().row == row && it.value().synced == synced) {
      it = lyrics_requests_.erase(it);
      --lyrics_requests_count_;
      fetch_cancelled = true;
    }
    else {
      ++it;
    }
  }

  if (fetch_cancelled) {
    // The song is only counted when lyrics are still searched for it, or lyrics were already found for it.
    if (!lyrics_rows_found_.contains(row) && std::none_of(lyrics_requests_.cbegin(), lyrics_requests_.cend(), [row](const LyricsRequest &request) { return request.row == row; })) {
      lyrics_rows_searched_.remove(row);
    }
    UpdateLyricsFetchStatus();
  }

}

void SongEditor::SearchLyrics() {

  const QModelIndexList indexes = SelectedIndexes();
  if (indexes.count() != 1) return;

  const int row = indexes.first().row();
  const Song song = data_.at(row).current_;
  const bool lyrics_available = LyricsSearchTypeHasLyrics(lyrics_search_type_) && LyricsStorageAvailable(song, false);
  const bool synced_lyrics_available = LyricsSearchTypeHasSyncedLyrics(lyrics_search_type_) && LyricsStorageAvailable(song, true);
  if (!lyrics_available && !synced_lyrics_available) return;

  const quint64 songs_generation = songs_generation_;
  const std::optional<LyricsSearchResult> result = lyrics_search_dialog_->Exec(song, lyrics_search_type_);
  if (!result.has_value()) return;

  // The songs were replaced while the dialog was open, the row is another song now.
  if (songs_generation_ != songs_generation) return;

  // The songs are still loading after they were replaced.
  if (row >= data_.count()) return;

  // The chosen lyrics replace the lyrics of the song, and lyrics being completed automatically for the song don't replace them.
  if (lyrics_available && !result->lyrics.isEmpty()) {
    CancelFetchLyricsForSong(row, false);
    SetFieldValue(row, u"lyrics"_s, result->lyrics);
  }
  if (synced_lyrics_available && !result->synced_lyrics.isEmpty()) {
    CancelFetchLyricsForSong(row, true);
    SetFieldValue(row, u"synced_lyrics"_s, result->synced_lyrics);
  }

}

void SongEditor::SetLyricsSearchType(const int index) {

  // The signal is from the lyrics tab the search type was changed in.
  LyricsTab *changed_lyrics_tab = qobject_cast<LyricsTab*>(sender());
  if (!changed_lyrics_tab) return;

  lyrics_search_type_ = static_cast<LyricsSearchType>(changed_lyrics_tab->search_type()->itemData(index).toInt());

  // The search controls are the same in both lyrics tabs.
  for (LyricsTab *lyrics_tab : {lyrics_tab_, synced_lyrics_tab_}) {
    const QSignalBlocker signal_blocker(lyrics_tab->search_type());
    lyrics_tab->search_type()->setCurrentIndex(lyrics_tab->search_type()->findData(static_cast<int>(lyrics_search_type_)));
  }

  Settings s;
  s.beginGroup(SettingsGroup());
  s.setValue(kLyricsSearchType, static_cast<int>(lyrics_search_type_));
  s.endGroup();

  UpdateLyricsSearchControls(SelectedIndexes());

}

void SongEditor::UpdateLyricsSearchControls(const QModelIndexList &indexes) {

  // The lyrics are only searched for when they can be saved for at least one of the songs, choosing the lyrics from a search is only for a single song.
  bool search_available = false;
  for (const QModelIndex &idx : indexes) {
    const Song &song = data_.at(idx.row()).original_;
    if ((LyricsSearchTypeHasLyrics(lyrics_search_type_) && LyricsStorageAvailable(song, false)) || (LyricsSearchTypeHasSyncedLyrics(lyrics_search_type_) && LyricsStorageAvailable(song, true))) {
      search_available = true;
      break;
    }
  }

  for (LyricsTab *lyrics_tab : {lyrics_tab_, synced_lyrics_tab_}) {
    lyrics_tab->fetch_button()->setEnabled(search_available);
    lyrics_tab->search_button()->setEnabled(search_available && indexes.count() == 1);
  }

}

void SongEditor::SetFieldValue(const int row, const QString &id, const QString &value) {

  if (row < 0 || row >= data_.count()) return;

  data_[row].set_value(id, value);

  // Update the field when the song is selected, with multiple songs it shows whether the values are different.
  const QModelIndexList indexes = song_list_->selectionModel()->selectedIndexes();
  bool selected = false;
  for (const QModelIndex &idx : indexes) {
    if (idx.row() == row) {
      selected = true;
      break;
    }
  }
  if (!selected) return;

  for (const FieldData &field : std::as_const(fields_)) {
    if (field.id_ == id) {
      ignore_edits_ = true;
      InitFieldValue(field, indexes);
      ignore_edits_ = false;
      break;
    }
  }

}

void SongEditor::LyricsStorageChanged(const int index) {

  StorageComboBoxChanged(lyrics_tab_->storage(), index, false);

}

void SongEditor::SyncedLyricsStorageChanged(const int index) {

  StorageComboBoxChanged(synced_lyrics_tab_->storage(), index, true);

}

void SongEditor::StorageComboBoxChanged(QComboBox *combobox, const int index, const bool synced) {

  const QModelIndexList indexes = song_list_->selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  const Song::StorageType storage_type = static_cast<Song::StorageType>(combobox->itemData(index).toInt());
  for (const QModelIndex &idx : indexes) {
    if (synced) {
      data_[idx.row()].current_.set_synced_lyrics_storage_type(storage_type);
    }
    else {
      data_[idx.row()].current_.set_lyrics_storage_type(storage_type);
    }
  }

  UpdateStorageComboBox(combobox, data_.at(indexes.first().row()), synced);

}

bool SongEditor::LyricsStorageAvailable(const Song &song, const bool synced) {

  if (synced) {
    return StorageTypeAvailable(song, Song::StorageType::Tag, true) || StorageTypeAvailable(song, Song::StorageType::File, true) || StorageTypeAvailable(song, Song::StorageType::Database, true);
  }

  return StorageTypeAvailable(song, Song::StorageType::Tag, false) || StorageTypeAvailable(song, Song::StorageType::Database, false);

}

bool SongEditor::StorageTypeAvailable(const Song &song, const Song::StorageType storage_type, const bool synced) {

  switch (storage_type) {
    case Song::StorageType::None:
      return false;
    case Song::StorageType::Database:
      return song.is_local_collection_song();
    case Song::StorageType::Tag:
      return synced ? song.synced_lyrics_supported() : song.lyrics_supported();
    case Song::StorageType::File:
      // LRC files are only for synchronized lyrics, and are not used for CUE tracks since a single LRC file would be for all tracks.
      // Only for collection songs, since only the collection watcher reads LRC files, other songs would lose the lyrics when they are reloaded from the file.
      return synced && song.is_local_collection_song() && !song.has_cue();
  }

  return false;

}

void SongEditor::SetAvailableStorageTypes(Song *song, const bool save_lyrics_to_tags) {

  // Pick the first available storage type if the song's is not available, for example for songs without lyrics, songs not in the collection or file types without lyrics tags.
  // The collection settings decide whether the database or the tags are preferred, an LRC file is only the default when the lyrics are already stored there.
  const Song::StorageType preferred_storage_type = save_lyrics_to_tags ? Song::StorageType::Tag : Song::StorageType::Database;
  const Song::StorageType other_storage_type = save_lyrics_to_tags ? Song::StorageType::Database : Song::StorageType::Tag;
  if (!StorageTypeAvailable(*song, song->lyrics_storage_type(), false)) {
    for (const Song::StorageType storage_type : {preferred_storage_type, other_storage_type}) {
      if (StorageTypeAvailable(*song, storage_type, false)) {
        song->set_lyrics_storage_type(storage_type);
        break;
      }
    }
  }

  if (!StorageTypeAvailable(*song, song->synced_lyrics_storage_type(), true)) {
    for (const Song::StorageType storage_type : {preferred_storage_type, other_storage_type, Song::StorageType::File}) {
      if (StorageTypeAvailable(*song, storage_type, true)) {
        song->set_synced_lyrics_storage_type(storage_type);
        break;
      }
    }
  }

}

void SongEditor::UpdateStorageComboBox(QComboBox *combobox, const Data &tag_data, const bool synced) {

  if (QStandardItemModel *model = qobject_cast<QStandardItemModel*>(combobox->model())) {
    for (int i = 0; i < combobox->count(); ++i) {
      const Song::StorageType storage_type = static_cast<Song::StorageType>(combobox->itemData(i).toInt());
      model->item(i)->setEnabled(StorageTypeAvailable(tag_data.current_, storage_type, synced));
    }
  }

  const Song::StorageType original_storage_type = synced ? tag_data.original_.synced_lyrics_storage_type() : tag_data.original_.lyrics_storage_type();
  const Song::StorageType current_storage_type = synced ? tag_data.current_.synced_lyrics_storage_type() : tag_data.current_.lyrics_storage_type();
  {
    const QSignalBlocker signal_blocker(combobox);
    combobox->setCurrentIndex(combobox->findData(static_cast<int>(current_storage_type)));
  }

  // Bold when changed, like the labels of the other fields.
  QFont combobox_font(font());
  combobox_font.setBold(current_storage_type != original_storage_type);
  combobox->setFont(combobox_font);

}

bool SongEditor::SaveSyncedLyricsToLrcFile(Song *song) {

  const QString lrc_filename = Utilities::LRCFilename(song->url().toLocalFile());

  // Cleared synchronized lyrics stored in an LRC file means removing the file.
  if (song->synced_lyrics().isEmpty()) {
    if (QFile::exists(lrc_filename) && !QFile::remove(lrc_filename)) {
      qLog(Error) << "Could not remove LRC file" << lrc_filename;
      return false;
    }
    song->set_lrc_mtime(0);
    return true;
  }

  // The collection watcher only reads LRC files with timed lines, so do not write a file it would ignore.
  const std::optional<Utilities::LRCLines> lrc_lines = Utilities::ParseLRC(song->synced_lyrics());
  if (!lrc_lines.has_value() || lrc_lines->isEmpty()) {
    qLog(Error) << "Not writing LRC file" << lrc_filename << "for synchronized lyrics which are not valid LRC";
    return false;
  }

  QSaveFile file(lrc_filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qLog(Error) << "Could not open LRC file" << lrc_filename << "for writing:" << file.errorString();
    return false;
  }
  // A short write is a failure too, QSaveFile discards the file if it is not committed.
  const QByteArray data = song->synced_lyrics().toUtf8();
  if (file.write(data) != data.size() || !file.commit()) {
    qLog(Error) << "Could not write LRC file" << lrc_filename << ":" << file.errorString();
    return false;
  }

  // Store the LRC file's mtime, so the collection watcher does not see the new file as changed.
  song->set_lrc_mtime(Utilities::FileMtimeMsec(lrc_filename));

  return true;

}

void SongEditor::SaveLyricsData(const int row, const bool lyrics_changed, const bool synced_lyrics_changed) {

  if (!lyrics_changed && !synced_lyrics_changed) return;

  const Data &tag_data = data_.at(row);

  // The song as it was loaded, with only the changed lyrics, so nothing else is saved.
  Song song(tag_data.original_);
  if (lyrics_changed) {
    song.set_lyrics(tag_data.current_.lyrics());
    song.set_lyrics_storage_type(tag_data.current_.lyrics_storage_type());
  }
  if (synced_lyrics_changed) {
    song.set_synced_lyrics(tag_data.current_.synced_lyrics());
    song.set_synced_lyrics_storage_type(tag_data.current_.synced_lyrics_storage_type());
    song.set_lrc_mtime(tag_data.current_.lrc_mtime());
  }

  const bool save_lyrics_tags = lyrics_changed && song.lyrics_storage_type() == Song::StorageType::Tag;
  const bool save_synced_lyrics_tags = synced_lyrics_changed && song.synced_lyrics_storage_type() == Song::StorageType::Tag;

  if ((!save_lyrics_tags && !save_synced_lyrics_tags) || song.is_stream()) {
    // Lyrics stored in the database or in an LRC file only need the collection to be updated.
    AddCollectionLyricsSong(song, lyrics_changed, synced_lyrics_changed);
    return;
  }

  ++save_tag_pending_;
  SaveLyricsTags(row, song.url().toLocalFile(), song, lyrics_changed, synced_lyrics_changed, save_lyrics_tags, save_synced_lyrics_tags);

}

void SongEditor::SaveLyricsTags(const int row, const QString &filename, const Song &song, const bool lyrics_changed, const bool synced_lyrics_changed, const bool write_lyrics_tag, const bool write_synced_lyrics_tag, const bool lyrics_tag_written) {

  // Only the lyrics tags are written, first the lyrics, then the synchronized lyrics, the song is finished when both are written or one of them failed.
  TagReaderReplyPtr reply = write_lyrics_tag ? tagreader_client_->SaveSongLyricsAsync(filename, song.lyrics()) : tagreader_client_->SaveSongSyncedLyricsAsync(filename, song.synced_lyrics());
  SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
  *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, row, filename, song, lyrics_changed, synced_lyrics_changed, write_lyrics_tag, write_synced_lyrics_tag, lyrics_tag_written, connection]() {
    QObject::disconnect(*connection);
    if (reply->success() && write_lyrics_tag && write_synced_lyrics_tag) {
      SaveLyricsTags(row, filename, song, lyrics_changed, synced_lyrics_changed, false, true, true);
      return;
    }
    SaveLyricsTagsFinished(row, reply->success(), reply->error(), filename, song, lyrics_changed, synced_lyrics_changed, lyrics_tag_written || (write_lyrics_tag && reply->success()));
  }, Qt::QueuedConnection);

}

void SongEditor::SaveLyricsTagsFinished(const int row, const bool success, const QString &error, const QString &filename, const Song &song, const bool lyrics_changed, const bool synced_lyrics_changed, const bool lyrics_tag_written) {

  --save_tag_pending_;

  if (success) {
    AddCollectionLyricsSong(song, lyrics_changed, synced_lyrics_changed);
  }
  else {
    if (error.isEmpty()) {
      Q_EMIT Error(tr("Could not write metadata to %1").arg(filename));
    }
    else {
      Q_EMIT Error(tr("Could not write metadata to %1: %2").arg(filename, error));
    }
    // The lyrics saved to the database or an LRC file, and the lyrics tag written before the synchronized lyrics tag failed, are saved even though writing the tags failed.
    const bool lyrics_saved = lyrics_changed && (lyrics_tag_written || song.lyrics_storage_type() != Song::StorageType::Tag);
    const bool synced_lyrics_saved = synced_lyrics_changed && song.synced_lyrics_storage_type() != Song::StorageType::Tag;
    AddCollectionLyricsSong(song, lyrics_saved, synced_lyrics_saved);
    RevertLyrics(row, lyrics_changed && !lyrics_saved, synced_lyrics_changed && !synced_lyrics_saved);
  }

  if (save_tag_pending_ <= 0) SaveDataFinished();

}

void SongEditor::RevertLyrics(const int row, const bool lyrics, const bool synced_lyrics) {

  if (row < 0 || row >= data_.count()) return;

  Data &ref = data_[row];
  if (lyrics) {
    ref.current_.set_lyrics(ref.original_.lyrics());
    ref.current_.set_lyrics_storage_type(ref.original_.lyrics_storage_type());
  }
  if (synced_lyrics) {
    ref.current_.set_synced_lyrics(ref.original_.synced_lyrics());
    ref.current_.set_synced_lyrics_storage_type(ref.original_.synced_lyrics_storage_type());
    ref.current_.set_lrc_mtime(ref.original_.lrc_mtime());
  }

}

void SongEditor::AddCollectionLyricsSong(const Song &song, const bool lyrics_changed, const bool synced_lyrics_changed) {

  if (!song.is_local_collection_song()) return;

  if (lyrics_changed) {
    collection_lyrics_songs_.insert(song.id(), song);
  }
  if (synced_lyrics_changed) {
    collection_synced_lyrics_songs_.insert(song.id(), song);
  }

}

