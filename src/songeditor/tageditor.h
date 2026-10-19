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

#ifndef TAGEDITOR_H
#define TAGEDITOR_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QString>
#include <QList>
#include <QImage>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/albumcoverimageresult.h"
#include "songeditor.h"

class QWidget;
class QMenu;
class QLabel;
class QEvent;
class QShowEvent;

class NetworkAccessManager;
class CollectionBackend;
class AlbumCoverLoader;
class CurrentAlbumCoverLoader;
class CoverProviders;
class LyricsProviders;
class StreamingServices;
class AlbumCoverChoiceController;
class Ui_TagEditor;
#ifdef HAVE_TAGFETCHER
class TrackSelectionDialog;
class TagFetcher;
#endif

// Edits the tags, the cover, the lyrics and the synchronized lyrics of songs.
class TagEditor : public SongEditor {
  Q_OBJECT

 public:
  explicit TagEditor(const SharedPtr<NetworkAccessManager> network,
                     const SharedPtr<TagReaderClient> tagreader_client,
                     const SharedPtr<CollectionBackend> collection_backend,
                     const SharedPtr<AlbumCoverLoader> albumcover_loader,
                     const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                     const SharedPtr<CoverProviders> cover_providers,
                     const SharedPtr<LyricsProviders> lyrics_providers,
                     const SharedPtr<StreamingServices> streaming_services,
                     QWidget *parent = nullptr);

  ~TagEditor() override;

 protected:
  bool eventFilter(QObject *o, QEvent *e) override;
  void showEvent(QShowEvent *e) override;

  // SongEditor
  QString SettingsGroup() const override;
  void Save() override;
  void UpdateSelection(const QModelIndexList &indexes) override;
  void LoadingChanged(const bool loading) override;
  void NoSongs() override;

 private Q_SLOTS:
  void ResetPlayStatistics();
  void SongRated(const float rating);
  void FetchTag();
  void FetchTagSongChosen(const Song &original_song, const Song &new_metadata);

  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &cover_result);

  void LoadCoverFromFile();
  void SaveCoverToFile();
  void LoadCoverFromURL();
  void SearchForCover();
  void UnsetCover();
  void ClearCover();
  void DeleteCover();
  void ShowCover();

  void SongSaveTagsComplete(TagReaderReplyPtr reply, const QString &filename, Song song, const SongEditor::UpdateCoverAction cover_action, const Song &song_lyrics_not_in_tags);

 private:
  Song *GetFirstSelected();
  void UpdateCover(const UpdateCoverAction cover_action, const AlbumCoverImageResult &cover_result = AlbumCoverImageResult());

  void UpdateSummaryTab(const Song &song);
  void UpdateStatisticsTab(const Song &song);

  QString GetArtSummary(const Song &song, const AlbumCoverLoaderResult::Type cover_type);
  QString GetArtSummary(const UpdateCoverAction cover_action);

  // Updates the cover of the saved song, then the collection is updated when all songs are saved.
  void SongSaveTagsFinished(const bool success, const QString &error, const QString &filename, Song song, const UpdateCoverAction cover_action, const Song &song_lyrics_not_in_tags);

  static void SetText(QLabel *label, const int value, const QString &suffix, const QString &def = QString());
  static void SetDate(QLabel *label, const qint64 time);

 private:
  static const char kArtDifferentHintText[];

  Ui_TagEditor *ui_;

  const SharedPtr<AlbumCoverLoader> albumcover_loader_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  const SharedPtr<CoverProviders> cover_providers_;

  AlbumCoverChoiceController *album_cover_choice_controller_;
#ifdef HAVE_TAGFETCHER
  TagFetcher *tag_fetcher_;
  TrackSelectionDialog *results_dialog_;
#endif

  QMenu *cover_menu_;

  const QImage image_no_cover_thumbnail_;

  quint64 summary_cover_art_id_;
  quint64 tags_cover_art_id_;
  QString summary_text_;
  QString tags_summary_text_;
  bool cover_art_is_set_;

  AlbumCoverLoaderOptions::Types cover_types_;
};

#endif  // TAGEDITOR_H
