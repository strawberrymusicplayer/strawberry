/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef EDITTAGDIALOG_H
#define EDITTAGDIALOG_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QDialog>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QList>
#include <QMap>
#include <QImage>

#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "playlist/playlistitem.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/albumcoverimageresult.h"

class QWidget;
class QMenu;
class QLabel;
class QAbstractButton;
class QPushButton;
class QEvent;
class QShowEvent;
class QHideEvent;

class NetworkAccessManager;
class CollectionBackend;
class AlbumCoverLoader;
class CurrentAlbumCoverLoader;
class CoverProviders;
class LyricsProviders;
class StreamingServices;
class AlbumCoverChoiceController;
class Ui_EditTagDialog;
#ifdef HAVE_MUSICBRAINZ
class TrackSelectionDialog;
class TagFetcher;
#endif
class LyricsFetcher;

class EditTagDialog : public QDialog {
  Q_OBJECT

 public:
  explicit EditTagDialog(const SharedPtr<NetworkAccessManager> network,
                         const SharedPtr<TagReaderClient> tagreader_client,
                         const SharedPtr<CollectionBackend> collection_backend,
                         const SharedPtr<AlbumCoverLoader> albumcover_loader,
                         const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                         const SharedPtr<CoverProviders> cover_providers,
                         const SharedPtr<LyricsProviders> lyrics_providers,
                         const SharedPtr<StreamingServices> streaming_services,
                         QWidget *parent = nullptr);

  ~EditTagDialog() override;

  void SetSongs(const SongList &songs, const PlaylistItemPtrList &items = PlaylistItemPtrList());

  PlaylistItemPtrList playlist_items() const { return playlist_items_; }

  void accept() override;

 Q_SIGNALS:
  void Error(const QString &message);

 protected:
  bool eventFilter(QObject *o, QEvent *e) override;
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

 private:
  enum class UpdateCoverAction {
    None = 0,
    Clear,
    Unset,
    Delete,
    New
  };
  struct Data {
    explicit Data(const Song &song = Song()) : original_(song), current_(song), cover_action_(UpdateCoverAction::None) {}

    static QVariant value(const Song &song, const QString &id);
    QVariant original_value(const QString &id) const { return value(original_, id); }
    QVariant current_value(const QString &id) const { return value(current_, id); }

    void set_value(const QString &id, const QVariant &value);

    Song original_;
    Song current_;
    UpdateCoverAction cover_action_;
    AlbumCoverImageResult cover_result_;
  };

 private Q_SLOTS:
  void SetSongsFinished();
  void SaveDataFinished();

  void SelectionChanged();
  void FieldValueEdited();
  void ResetField();
  void ButtonClicked(QAbstractButton *button);
  void ResetPlayStatistics();
  void SongRated(const float rating);
  void FetchTag();
  void FetchTagSongChosen(const Song &original_song, const Song &new_metadata);
  void FetchLyrics();
  void UpdateLyrics(const quint64 id, const QString &provider, const QString &lyrics);

  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &cover_result);

  void LoadCoverFromFile();
  void SaveCoverToFile();
  void LoadCoverFromURL();
  void SearchForCover();
  void UnsetCover();
  void ClearCover();
  void DeleteCover();
  void ShowCover();

  void PreviousSong();
  void NextSong();

  void SongSaveTagsComplete(TagReaderReplyPtr reply, const QString &filename, Song song, const UpdateCoverAction cover_action);

 private:
  struct FieldData {
    explicit FieldData(QLabel *label = nullptr, QWidget *editor = nullptr, const QString &id = QString())
        : label_(label), editor_(editor), id_(id) {}

    QLabel *label_;
    QWidget *editor_;
    QString id_;
  };

  Song *GetFirstSelected();
  void UpdateCover(const UpdateCoverAction cover_action, const AlbumCoverImageResult &cover_result = AlbumCoverImageResult());

  bool DoesValueVary(const QModelIndexList &sel, const QString &id) const;
  bool IsValueModified(const QModelIndexList &sel, const QString &id) const;

  void InitFieldValue(const FieldData &field, const QModelIndexList &sel);
  void UpdateFieldValue(const FieldData &field, const QModelIndexList &sel);
  void UpdateModifiedField(const FieldData &field, const QModelIndexList &sel);
  void ResetFieldValue(const FieldData &field, const QModelIndexList &sel);

  void UpdateSummaryTab(const Song &song);
  void UpdateStatisticsTab(const Song &song);

  QString GetArtSummary(const Song &song, const AlbumCoverLoaderResult::Type cover_type);
  QString GetArtSummary(const UpdateCoverAction cover_action);

  void UpdateUI(const QModelIndexList &indexes);

  bool SetLoading(const QString &message);
  void SetSongListVisibility(bool visible);

  // Called by QtConcurrentRun
  QList<Data> LoadData(const SongList &songs) const;
  void SaveData();

  static void SetText(QLabel *label, const int value, const QString &suffix, const QString &def = QString());
  static void SetDate(QLabel *label, const qint64 time);

 private:
  static const char kTagsDifferentHintText[];
  static const char kArtDifferentHintText[];

  Ui_EditTagDialog *ui_;

  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<CollectionBackend> collection_backend_;
  const SharedPtr<AlbumCoverLoader> albumcover_loader_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  const SharedPtr<CoverProviders> cover_providers_;

  AlbumCoverChoiceController *album_cover_choice_controller_;
#ifdef HAVE_MUSICBRAINZ
  TagFetcher *tag_fetcher_;
  TrackSelectionDialog *results_dialog_;
#endif
  LyricsFetcher *lyrics_fetcher_;

  QMenu *cover_menu_;

  const QImage image_no_cover_thumbnail_;

  bool loading_;

  PlaylistItemPtrList playlist_items_;
  QList<Data> data_;
  QList<FieldData> fields_;

  bool ignore_edits_;

  quint64 summary_cover_art_id_;
  quint64 tags_cover_art_id_;
  bool cover_art_is_set_;

  QPushButton *previous_button_;
  QPushButton *next_button_;

  int save_tag_pending_;

  QMap<int, Song> collection_songs_;

  AlbumCoverLoaderOptions::Types cover_types_;

  qint64 lyrics_id_;
};

#endif  // EDITTAGDIALOG_H
