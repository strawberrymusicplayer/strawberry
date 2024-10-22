/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ALBUMCOVERCHOICECONTROLLER_H
#define ALBUMCOVERCHOICECONTROLLER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QPair>
#include <QSet>
#include <QList>
#include <QMap>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QMutex>

#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "utilities/coveroptions.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverimageresult.h"

class QFileDialog;
class QAction;
class QActionGroup;
class QMenu;
class QDragEnterEvent;
class QDropEvent;

class NetworkAccessManager;
class CollectionBackend;
class AlbumCoverLoader;
class CurrentAlbumCoverLoader;
class CoverProviders;
class AlbumCoverFetcher;
class AlbumCoverSearcher;
class CoverFromURLDialog;
struct CoverSearchStatistics;
class StreamingServices;

// Controller for the common album cover related menu options.
class AlbumCoverChoiceController : public QWidget {
  Q_OBJECT

 public:
  explicit AlbumCoverChoiceController(QWidget *parent = nullptr);
  ~AlbumCoverChoiceController() override;

  void Init(const SharedPtr<NetworkAccessManager> network,
            const SharedPtr<TagReaderClient> tagreader_client,
            const SharedPtr<CollectionBackend> collection_backend,
            const SharedPtr<AlbumCoverLoader> albumcover_loader,
            const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
            const SharedPtr<CoverProviders> cover_providers,
            const SharedPtr<StreamingServices> streaming_services);

  void ReloadSettings();

  CoverOptions::CoverType get_save_album_cover_type() const { return (save_embedded_cover_override_ ? CoverOptions::CoverType::Embedded : cover_options_.cover_type); }
  CoverOptions::CoverType get_collection_save_album_cover_type() const { return cover_options_.cover_type; }

  // Getters for all QActions implemented by this controller.

  QAction *cover_from_file_action() const { return cover_from_file_; }
  QAction *cover_to_file_action() const { return cover_to_file_; }
  QAction *cover_from_url_action() const { return cover_from_url_; }
  QAction *search_for_cover_action() const { return search_for_cover_; }
  QAction *unset_cover_action() const { return unset_cover_; }
  QAction *delete_cover_action() const { return delete_cover_; }
  QAction *clear_cover_action() const { return clear_cover_; }
  QAction *show_cover_action() const { return show_cover_; }
  QAction *search_cover_auto_action() const { return search_cover_auto_; }

  // Returns QAction* for every operation implemented by this controller.
  // The list contains QAction* for:
  // 1. loading cover from file
  // 2. loading cover from URL
  // 3. searching for cover using last.fm
  // 4. unsetting the cover manually
  // 5. showing the cover in original size
  QList<QAction*> GetAllActions();

  // All of the methods below require a currently selected song as an input parameter.
  // Also - LoadCoverFromFile, LoadCoverFromURL, SearchForCover, UnsetCover and SaveCover all update manual path of the given song in collection to the new cover.

  // Lets the user choose a cover from disk. If no cover will be chosen or the chosen cover will not be a proper image, this returns an empty string.
  // Otherwise, the path to the chosen cover will be returned.
  AlbumCoverImageResult LoadImageFromFile(Song *song);
  QUrl LoadCoverFromFile(Song *song);

  // Shows a dialog that allows user to save the given image on disk.
  // The image is supposed to be the cover of the given song's album.
  void SaveCoverToFileManual(const Song &song, const AlbumCoverImageResult &result);

  // Downloads the cover from an URL given by user.
  // This returns the downloaded image or null image if something went wrong for example when user cancelled the dialog.
  void LoadCoverFromURL(Song *song);
  AlbumCoverImageResult LoadImageFromURL();

  // Lets the user choose a cover among all that have been found on last.fm.
  // Returns the chosen cover or null cover if user didn't choose anything.
  void SearchForCover(Song *song);
  AlbumCoverImageResult SearchForImage(Song *song);

  void UnsetCover(Song *song);

  // Clears any album cover art associated with the song.
  void ClearCover(Song *song);

  // Physically deletes associated album covers from disk.
  bool DeleteCover(Song *song, const bool unset = false);

  // Shows the cover of given song in it's original size.
  void ShowCover(const Song &song, const QImage &image = QImage());
  void ShowCover(const Song &song, const QPixmap &pixmap);

  // Search for covers automatically
  quint64 SearchCoverAutomatically(const Song &song);

  // Saves the chosen cover as manual cover path of this song in collection.
  void SaveArtEmbeddedToSong(Song *song, const bool art_embedded);
  void SaveArtAutomaticToSong(Song *song, const QUrl &art_automatic);
  void SaveArtManualToSong(Song *song, const QUrl &art_manual);
  void ClearAlbumCoverForSong(Song *song);
  void UnsetAlbumCoverForSong(Song *song);

  // Saves the cover that the user picked through a drag and drop operation.
  void SaveCover(Song *song, const QDropEvent *e);

  // Saves the given image in album directory or cache as a cover for 'album artist' - 'album'. The method returns path of the image.
  QUrl SaveCoverAutomatic(Song *song, const AlbumCoverImageResult &result);

  QUrl SaveCoverToFileAutomatic(const Song *song, const AlbumCoverImageResult &result, const bool force_overwrite = false);
  QUrl SaveCoverToFileAutomatic(const Song::Source source, const QString &artist, const QString &album, const QString &album_id, const QString &album_dir, const AlbumCoverImageResult &result, const bool force_overwrite = false);

  void SaveCoverEmbeddedToCollectionSongs(const Song &song, const AlbumCoverImageResult &result);
  void SaveCoverEmbeddedToCollectionSongs(const Song &song, const QString &cover_filename, const QByteArray &image_data = QByteArray(), const QString &mime_type = QString());
  void SaveCoverEmbeddedToCollectionSongs(const QString &effective_albumartist, const QString &effective_album, const QString &cover_filename, const QByteArray &image_data = QByteArray(), const QString &mime_type = QString());
  void SaveCoverEmbeddedToSong(const Song &song, const QString &cover_filename, const QByteArray &image_data, const QString &mime_type = QString());

  static bool CanAcceptDrag(const QDragEnterEvent *e);

 public Q_SLOTS:
  void set_save_embedded_cover_override(const bool value) { save_embedded_cover_override_ = value; }

 private Q_SLOTS:
  void AlbumCoverFetched(const quint64 id, const AlbumCoverImageResult &result, const CoverSearchStatistics &statistics);
  void SaveEmbeddedCoverFinished(TagReaderReplyPtr reply, Song song, const bool art_embedded);

 Q_SIGNALS:
  void Error(const QString &error);
  void AutomaticCoverSearchDone();

 private:
  static QString GetInitialPathForFileDialog(const Song &song, const QString &filename);

  static bool IsKnownImageExtension(const QString &suffix);
  static QSet<QString> *sImageExtensions;

  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  SharedPtr<NetworkAccessManager> network_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<CollectionBackend> collection_backend_;
  SharedPtr<StreamingServices> streaming_services_;

  AlbumCoverSearcher *cover_searcher_;
  AlbumCoverFetcher *cover_fetcher_;

  QFileDialog *save_file_dialog_;
  CoverFromURLDialog *cover_from_url_dialog_;

  QAction *cover_from_file_;
  QAction *cover_to_file_;
  QAction *cover_from_url_;
  QAction *search_for_cover_;
  QAction *separator1_;
  QAction *unset_cover_;
  QAction *delete_cover_;
  QAction *clear_cover_;
  QAction *separator2_;
  QAction *show_cover_;
  QAction *search_cover_auto_;

  QMap<quint64, Song> cover_fetching_tasks_;
  QList<Song> cover_save_tasks_;
  QMutex mutex_cover_save_tasks_;

  CoverOptions cover_options_;
  bool save_embedded_cover_override_;

  AlbumCoverLoaderOptions::Types cover_types_;
};

#endif  // ALBUMCOVERCHOICECONTROLLER_H
