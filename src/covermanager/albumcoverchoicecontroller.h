/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QSet>
#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>
#include <QImage>

#include "core/song.h"
#include "settings/collectionsettingspage.h"

class QFileDialog;
class QAction;
class QDragEnterEvent;
class QDropEvent;

class Application;
class AlbumCoverFetcher;
class AlbumCoverSearcher;
class CoverFromURLDialog;
struct CoverSearchStatistics;

// Controller for the common album cover related menu options.
class AlbumCoverChoiceController : public QWidget {
  Q_OBJECT

 public:
  static const char *kLoadImageFileFilter;
  static const char *kSaveImageFileFilter;
  static const char *kAllFilesFilter;

  explicit AlbumCoverChoiceController(QWidget *parent = nullptr);
  ~AlbumCoverChoiceController();

  void Init(Application *app);
  void ReloadSettings();

  // Getters for all QActions implemented by this controller.

  QAction *cover_from_file_action() const { return cover_from_file_; }
  QAction *cover_to_file_action() const { return cover_to_file_; }
  QAction *cover_from_url_action() const { return cover_from_url_; }
  QAction *search_for_cover_action() const { return search_for_cover_; }
  QAction *unset_cover_action() const { return unset_cover_; }
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
  QUrl LoadCoverFromFile(Song *song);

  // Shows a dialog that allows user to save the given image on disk.
  // The image is supposed to be the cover of the given song's album.
  void SaveCoverToFileManual(const Song &song, const QImage &image);

  // Downloads the cover from an URL given by user.
  // This returns the downloaded image or null image if something went wrong for example when user cancelled the dialog.
  QUrl LoadCoverFromURL(Song *song);

  // Lets the user choose a cover among all that have been found on last.fm.
  // Returns the chosen cover or null cover if user didn't choose anything.
  QUrl SearchForCover(Song *song);

  // Returns a path which indicates that the cover has been unset manually.
  QUrl UnsetCover(Song *song);

  // Shows the cover of given song in it's original size.
  void ShowCover(const Song &song);
  void ShowCover(const Song &song, const QImage &image);
  void ShowCover(const Song &song, const QPixmap &pixmap);

  // Search for covers automatically
  qint64 SearchCoverAutomatically(const Song &song);

  // Saves the chosen cover as manual cover path of this song in collection.
  void SaveCoverToSong(Song *song, const QUrl &cover_url);

  // Saves the cover that the user picked through a drag and drop operation.
  QUrl SaveCover(Song *song, const QDropEvent *e);

  // Saves the given image in album directory or cache as a cover for 'album artist' - 'album'. The method returns path of the image.
  QUrl SaveCoverToFileAutomatic(const Song *song, const QUrl &cover_url, const QImage &image, const bool overwrite = false);
  QUrl SaveCoverToFileAutomatic(const Song::Source source, const QString &artist, const QString &album, const QString &album_id, const QString &album_dir, const QUrl &cover_url, const QImage &image, const bool overwrite = false);

  static bool CanAcceptDrag(const QDragEnterEvent *e);

 signals:
  void AutomaticCoverSearchDone();

 private slots:
  void AlbumCoverFetched(const quint64 id, const QUrl &cover_url, const QImage &image, const CoverSearchStatistics &statistics);

 private:
  QString GetInitialPathForFileDialog(const Song &song, const QString &filename);

  static bool IsKnownImageExtension(const QString &suffix);
  static QSet<QString> *sImageExtensions;

  Application *app_;
  AlbumCoverSearcher *cover_searcher_;
  AlbumCoverFetcher *cover_fetcher_;

  QFileDialog *save_file_dialog_;
  CoverFromURLDialog *cover_from_url_dialog_;

  QAction *cover_from_file_;
  QAction *cover_to_file_;
  QAction *separator_;
  QAction *cover_from_url_;
  QAction *search_for_cover_;
  QAction *unset_cover_;
  QAction *show_cover_;
  QAction *search_cover_auto_;

  QMap<quint64, Song> cover_fetching_tasks_;

  bool cover_album_dir_;
  CollectionSettingsPage::SaveCover cover_filename_;
  QString cover_pattern_;
  bool cover_overwrite_;
  bool cover_lowercase_;
  bool cover_replace_spaces_;

};

#endif  // ALBUMCOVERCHOICECONTROLLER_H
