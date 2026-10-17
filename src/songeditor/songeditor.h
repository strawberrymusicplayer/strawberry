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

#ifndef SONGEDITOR_H
#define SONGEDITOR_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QDialog>
#include <QVariant>
#include <QString>
#include <QList>
#include <QMap>
#include <QHash>
#include <QSet>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "playlist/playlistitem.h"
#include "covermanager/albumcoverimageresult.h"
#include "lyrics/lyricssearchresult.h"
#include "lyricssearchtype.h"

class QWidget;
class QLabel;
class QAbstractButton;
class QPushButton;
class QComboBox;
class QListWidget;
class QTabWidget;
class QDialogButtonBox;
class QShowEvent;
class QHideEvent;

class CollectionBackend;
class LyricsProviders;
class LyricsFetcher;
class LyricsSearchDialog;
class BusyIndicator;
class LyricsTab;

// The base class of the tag editor and the lyrics editor.
// It loads the songs, handles the song list and the fields, the lyrics and synchronized lyrics tabs, and updates the collection after saving.
// The subclasses have their own UI, and save the songs in Save().
class SongEditor : public QDialog {
  Q_OBJECT

 public:
  explicit SongEditor(const SharedPtr<TagReaderClient> tagreader_client,
                      const SharedPtr<CollectionBackend> collection_backend,
                      const SharedPtr<LyricsProviders> lyrics_providers,
                      QWidget *parent = nullptr);

  // Returns false when the songs are not set yet, since the dialog is still loading or saving the previous songs.
  // They are set when it's finished then, and PendingSongsSet is emitted.
  bool SetSongs(const SongList &songs, const PlaylistItemPtrList &items = PlaylistItemPtrList());

  // Returns the playlist items of the songs in the dialog, in the same order as songs(), songs which could not be loaded are not in the dialog.
  PlaylistItemPtrList playlist_items() const;
  SongList songs() const;
  // Returns the songs as they were loaded into the dialog, in the same order as songs(), to find out which fields were edited.
  SongList original_songs() const;
  void accept() override;

 Q_SIGNALS:
  void Error(const QString &message);
  // Emitted when songs which were set while loading or saving the previous songs are set.
  void PendingSongsSet();

 protected:
  // Only used by the tag editor, it's here since it's part of the song data.
  enum class UpdateCoverAction {
    None = 0,
    Clear,
    Unset,
    Delete,
    New
  };
  struct Data {
    explicit Data(const Song &song = Song(), const qsizetype index = -1) : original_(song), current_(song), cover_action_(UpdateCoverAction::None), stored_lyrics_storage_type_(song.lyrics_storage_type()), stored_synced_lyrics_storage_type_(song.synced_lyrics_storage_type()), index_(index) {}

    static QVariant value(const Song &song, const QString &id);
    QVariant original_value(const QString &id) const { return value(original_, id); }
    QVariant current_value(const QString &id) const { return value(current_, id); }

    void set_value(const QString &id, const QVariant &value);

    Song original_;
    Song current_;
    // The cover is only changed by the tag editor.
    UpdateCoverAction cover_action_;
    AlbumCoverImageResult cover_result_;
    // Where the lyrics were stored before a storage type was preselected for the editor, None if they were not stored anywhere.
    Song::StorageType stored_lyrics_storage_type_;
    Song::StorageType stored_synced_lyrics_storage_type_;
    // The index of the song in the songs set in SetSongs(), to find its playlist item.
    qsizetype index_;
  };

  // Called by the subclasses after setting up their UI, the lyrics and synchronized lyrics tabs are added to the tab widget, and the fields are found in it.
  void InitSongEditor(QListWidget *song_list, QTabWidget *tab_widget, QDialogButtonBox *button_box, BusyIndicator *loading_indicator);

  // The settings group for the window geometry and the current tab.
  virtual QString SettingsGroup() const = 0;
  // Saves the songs, the loading indicator is shown, it calls SaveDataFinished() when it's finished, or SongSaveFinished() for each song saved to the file.
  virtual void Save() = 0;
  // Updates the UI of the subclass for the selected songs, after the fields and the lyrics tabs were updated.
  virtual void UpdateSelection(const QModelIndexList &indexes) { Q_UNUSED(indexes) }
  // The subclass can disable its own widgets while loading or saving.
  virtual void LoadingChanged(const bool loading) { Q_UNUSED(loading) }
  // No valid songs were loaded.
  virtual void NoSongs() {}

  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

  QModelIndexList SelectedIndexes() const;
  void SelectionChanged();
  void UpdateUI(const QModelIndexList &indexes);

  // Updates the lyrics storage and writes the LRC file for saving the song, returns whether the lyrics and synchronized lyrics were changed.
  void PrepareLyricsForSave(Data *tag_data, bool *lyrics_changed, bool *synced_lyrics_changed);
  // Saves only the lyrics of the song, the other fields are saved as they were loaded.
  void SaveLyricsData(const int row, const bool lyrics_changed, const bool synced_lyrics_changed);
  // Reverts the lyrics of the song which could not be saved, so the songs only have the lyrics which were saved when the editor is accepted.
  void RevertLyrics(const int row, const bool lyrics, const bool synced_lyrics);
  // A song was saved to the file, it's updated in the collection when all songs are saved.
  void SongSaveFinished(const bool success, const QString &error, const QString &filename, const Song &song, const Song &song_lyrics_not_in_tags);
  void SaveDataFinished();

  static bool StorageTypeAvailable(const Song &song, const Song::StorageType storage_type, const bool synced);
  // Returns true when the lyrics or synchronized lyrics of the song can be saved anywhere.
  static bool LyricsStorageAvailable(const Song &song, const bool synced);

  static const char kTagsDifferentHintText[];

  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<CollectionBackend> collection_backend_;

  QList<Data> data_;
  bool ignore_edits_;
  int save_tag_pending_;
  // The collection songs which were saved, they are updated in the collection when all songs are saved.
  QMap<int, Song> collection_songs_;
  // The collection songs with only the lyrics or the synchronized lyrics saved, only those are updated in the collection.
  QMap<int, Song> collection_lyrics_songs_;
  QMap<int, Song> collection_synced_lyrics_songs_;

 private Q_SLOTS:
  void SetSongsFinished();
  void SetPendingSongs(const QMap<int, Song> &saved_collection_songs = QMap<int, Song>());
  void ClearPendingSongs();
  void FieldValueEdited();
  void ResetField();
  void ButtonClicked(QAbstractButton *button);
  void PreviousSong();
  void NextSong();
  // Completes the missing lyrics, synchronized lyrics or both automatically for the selected songs.
  void FetchLyrics();
  void UpdateLyrics(const quint64 id, const LyricsSearchResult &result);
  // Lets the user choose the lyrics, synchronized lyrics or both from a search for the selected song.
  void SearchLyrics();
  void SetLyricsSearchType(const int index);
  void LyricsStorageChanged(const int index);
  void SyncedLyricsStorageChanged(const int index);
  void CancelFetchLyrics();

 private:
  struct FieldData {
    explicit FieldData(QLabel *label = nullptr, QWidget *editor = nullptr, const QString &id = QString())
        : label_(label), editor_(editor), id_(id) {}

    QLabel *label_;
    QWidget *editor_;
    QString id_;
  };

  bool DoesValueVary(const QModelIndexList &sel, const QString &id) const;
  bool IsValueModified(const QModelIndexList &sel, const QString &id) const;

  void InitFieldValue(const FieldData &field, const QModelIndexList &sel);
  void UpdateFieldValue(const FieldData &field, const QModelIndexList &sel);
  void UpdateModifiedField(const FieldData &field, const QModelIndexList &sel);
  void ResetFieldValue(const FieldData &field, const QModelIndexList &sel);

  bool SetLoading(const QString &message);
  void SetSongListVisibility(bool visible);

  // Called by QtConcurrentRun
  QList<Data> LoadData(const SongList &songs) const;

  // Sets the value of the field for the song in the row, and updates the field when the song is selected.
  void SetFieldValue(const int row, const QString &id, const QString &value);
  // Shows the progress or the result of completing the lyrics automatically, in both lyrics tabs.
  void UpdateLyricsFetchStatus();
  void SetLyricsFetchStatus(const QString &text);
  // Enables the search controls of both lyrics tabs for the selected songs and what to search for.
  void UpdateLyricsSearchControls(const QModelIndexList &indexes);
  // Removes the requests for completing the lyrics automatically for the song, so they don't replace the chosen lyrics.
  void CancelFetchLyricsForSong(const int row, const bool synced);

  static void SetAvailableStorageTypes(Song *song, const bool save_lyrics_to_tags);
  void UpdateStorageComboBox(QComboBox *combobox, const Data &tag_data, const bool synced);
  void StorageComboBoxChanged(QComboBox *combobox, const int index, const bool synced);
  static bool SaveSyncedLyricsToLrcFile(Song *song);
  // Writes only the lyrics and synchronized lyrics tags, first the lyrics, then the synchronized lyrics.
  // With lyrics_tag_written, the lyrics tag was already written before the synchronized lyrics tag.
  void SaveLyricsTags(const int row, const QString &filename, const Song &song, const bool lyrics_changed, const bool synced_lyrics_changed, const bool write_lyrics_tag, const bool write_synced_lyrics_tag, const bool lyrics_tag_written = false);
  void SaveLyricsTagsFinished(const int row, const bool success, const QString &error, const QString &filename, const Song &song, const bool lyrics_changed, const bool synced_lyrics_changed, const bool lyrics_tag_written);
  // Updates only the changed lyrics of the song in the collection when all songs are saved.
  void AddCollectionLyricsSong(const Song &song, const bool lyrics_changed, const bool synced_lyrics_changed);

 private:
  QListWidget *song_list_;
  QTabWidget *tab_widget_;
  QDialogButtonBox *button_box_;
  BusyIndicator *loading_indicator_;
  LyricsTab *lyrics_tab_;
  LyricsTab *synced_lyrics_tab_;
  QPushButton *previous_button_;
  QPushButton *next_button_;

  LyricsFetcher *lyrics_fetcher_;
  LyricsSearchDialog *lyrics_search_dialog_;

  bool loading_;

  PlaylistItemPtrList playlist_items_;
  // The songs set while loading or saving the previous songs, they are set when it's finished.
  bool songs_pending_;
  SongList pending_songs_;
  PlaylistItemPtrList pending_playlist_items_;
  QList<FieldData> fields_;

  // A request for completing the lyrics or the synchronized lyrics of the song in the row in data_.
  struct LyricsRequest {
    int row;
    bool synced;
  };
  QHash<quint64, LyricsRequest> lyrics_requests_;
  int lyrics_requests_count_;
  // The rows of the songs the lyrics are completed for, and of those the lyrics were found for.
  QSet<int> lyrics_rows_searched_;
  QSet<int> lyrics_rows_found_;
  // What to search for, it's the same in both lyrics tabs.
  LyricsSearchType lyrics_search_type_;
  // Changed when other songs are set, to know whether the songs were replaced while the lyrics search dialog was open.
  quint64 songs_generation_;

  // Default for new lyrics from the collection settings, read before loading the songs.
  bool save_lyrics_to_tags_;
};

#endif  // SONGEDITOR_H
