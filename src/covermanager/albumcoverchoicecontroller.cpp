/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QGuiApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QScreen>
#include <QWindow>
#include <QWidget>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QString>
#include <QRegularExpression>
#include <QUrl>
#include <QImage>
#include <QImageWriter>
#include <QPixmap>
#include <QIcon>
#include <QRect>
#include <QFileDialog>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QSettings>
#include <QtEvents>

#include "utilities/filenameconstants.h"
#include "utilities/strutils.h"
#include "utilities/mimeutils.h"
#include "utilities/imageutils.h"
#include "utilities/coveroptions.h"
#include "utilities/coverutils.h"
#include "core/application.h"
#include "core/song.h"
#include "core/iconloader.h"

#include "collection/collectionfilteroptions.h"
#include "collection/collectionbackend.h"
#include "settings/collectionsettingspage.h"
#include "internet/internetservices.h"
#include "internet/internetservice.h"
#include "albumcoverchoicecontroller.h"
#include "albumcoverfetcher.h"
#include "albumcoverloader.h"
#include "albumcoversearcher.h"
#include "albumcoverimageresult.h"
#include "coverfromurldialog.h"
#include "currentalbumcoverloader.h"

const char *AlbumCoverChoiceController::kLoadImageFileFilter = QT_TR_NOOP("Images (*.png *.jpg *.jpeg *.bmp *.gif *.xpm *.pbm *.pgm *.ppm *.xbm)");
const char *AlbumCoverChoiceController::kSaveImageFileFilter = QT_TR_NOOP("Images (*.png *.jpg *.jpeg *.bmp *.xpm *.pbm *.ppm *.xbm)");
const char *AlbumCoverChoiceController::kAllFilesFilter = QT_TR_NOOP("All files (*)");

QSet<QString> *AlbumCoverChoiceController::sImageExtensions = nullptr;

AlbumCoverChoiceController::AlbumCoverChoiceController(QWidget *parent)
    : QWidget(parent),
      app_(nullptr),
      cover_searcher_(nullptr),
      cover_fetcher_(nullptr),
      save_file_dialog_(nullptr),
      cover_from_url_dialog_(nullptr),
      cover_from_file_(nullptr),
      cover_to_file_(nullptr),
      cover_from_url_(nullptr),
      search_for_cover_(nullptr),
      separator1_(nullptr),
      unset_cover_(nullptr),
      delete_cover_(nullptr),
      clear_cover_(nullptr),
      separator2_(nullptr),
      show_cover_(nullptr),
      search_cover_auto_(nullptr),
      save_embedded_cover_override_(false) {

  cover_from_file_ = new QAction(IconLoader::Load("document-open"), tr("Load cover from disk..."), this);
  cover_to_file_ = new QAction(IconLoader::Load("document-save"), tr("Save cover to disk..."), this);
  cover_from_url_ = new QAction(IconLoader::Load("download"), tr("Load cover from URL..."), this);
  search_for_cover_ = new QAction(IconLoader::Load("search"), tr("Search for album covers..."), this);
  unset_cover_ = new QAction(IconLoader::Load("list-remove"), tr("Unset cover"), this);
  delete_cover_ = new QAction(IconLoader::Load("list-remove"), tr("Delete cover"), this);
  clear_cover_ = new QAction(IconLoader::Load("list-remove"), tr("Clear cover"), this);
  separator1_ = new QAction(this);
  separator1_->setSeparator(true);
  show_cover_ = new QAction(IconLoader::Load("zoom-in"), tr("Show fullsize..."), this);

  search_cover_auto_ = new QAction(tr("Search automatically"), this);
  search_cover_auto_->setCheckable(true);
  search_cover_auto_->setChecked(false);

  separator2_ = new QAction(this);
  separator2_->setSeparator(true);

  ReloadSettings();

}

AlbumCoverChoiceController::~AlbumCoverChoiceController() = default;

void AlbumCoverChoiceController::Init(Application *app) {

  app_ = app;

  cover_fetcher_ = new AlbumCoverFetcher(app_->cover_providers(), this);
  cover_searcher_ = new AlbumCoverSearcher(QIcon(":/pictures/cdcase.png"), app, this);
  cover_searcher_->Init(cover_fetcher_);

  QObject::connect(cover_fetcher_, &AlbumCoverFetcher::AlbumCoverFetched, this, &AlbumCoverChoiceController::AlbumCoverFetched);
  QObject::connect(app_->album_cover_loader(), &AlbumCoverLoader::SaveEmbeddedCoverAsyncFinished, this, &AlbumCoverChoiceController::SaveEmbeddedCoverAsyncFinished);

}

void AlbumCoverChoiceController::ReloadSettings() {

  QSettings s;
  s.beginGroup(CollectionSettingsPage::kSettingsGroup);
  cover_options_.cover_type = static_cast<CoverOptions::CoverType>(s.value("save_cover_type", static_cast<int>(CoverOptions::CoverType::Cache)).toInt());
  cover_options_.cover_filename = static_cast<CoverOptions::CoverFilename>(s.value("save_cover_filename", static_cast<int>(CoverOptions::CoverFilename::Pattern)).toInt());
  cover_options_.cover_pattern = s.value("cover_pattern", "%albumartist-%album").toString();
  cover_options_.cover_overwrite = s.value("cover_overwrite", false).toBool();
  cover_options_.cover_lowercase = s.value("cover_lowercase", false).toBool();
  cover_options_.cover_replace_spaces = s.value("cover_replace_spaces", false).toBool();
  s.endGroup();

}

QList<QAction*> AlbumCoverChoiceController::GetAllActions() {

  return QList<QAction*>() << show_cover_
                           << cover_to_file_
                           << separator1_
                           << cover_from_file_
                           << cover_from_url_
                           << search_for_cover_
                           << separator2_
                           << unset_cover_
                           << clear_cover_
                           << delete_cover_;

}

AlbumCoverImageResult AlbumCoverChoiceController::LoadImageFromFile(Song *song) {

  if (!song->url().isLocalFile()) return AlbumCoverImageResult();

  QString cover_file = QFileDialog::getOpenFileName(this, tr("Load cover from disk"), GetInitialPathForFileDialog(*song, QString()), tr(kLoadImageFileFilter) + ";;" + tr(kAllFilesFilter));

  if (cover_file.isEmpty()) return AlbumCoverImageResult();

  AlbumCoverImageResult result;
  QFile file(cover_file);
  if (file.open(QIODevice::ReadOnly)) {
    result.image_data = file.readAll();
    file.close();
    if (result.image_data.isEmpty()) {
      qLog(Error) << "Cover file" << cover_file << "is empty.";
      emit Error(tr("Cover file %1 is empty.").arg(cover_file));
    }
    else {
      result.mime_type = Utilities::MimeTypeFromData(result.image_data);
      result.image.loadFromData(result.image_data);
      result.cover_url = QUrl::fromLocalFile(cover_file);
    }
  }
  else {
    qLog(Error) << "Failed to open cover file" << cover_file << "for reading:" << file.errorString();
    emit Error(tr("Failed to open cover file %1 for reading: %2").arg(cover_file, file.errorString()));
  }

  return result;

}

QUrl AlbumCoverChoiceController::LoadCoverFromFile(Song *song) {

  if (!song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return QUrl();

  QString cover_file = QFileDialog::getOpenFileName(this, tr("Load cover from disk"), GetInitialPathForFileDialog(*song, QString()), tr(kLoadImageFileFilter) + ";;" + tr(kAllFilesFilter));

  if (cover_file.isEmpty()) return QUrl();

  if (QImage(cover_file).isNull()) return QUrl();

  switch (get_save_album_cover_type()) {
    case CoverOptions::CoverType::Embedded:
      if (song->save_embedded_cover_supported()) {
        SaveCoverEmbeddedAutomatic(*song, cover_file);
        return QUrl::fromLocalFile(Song::kEmbeddedCover);
      }
      [[fallthrough]];
    case CoverOptions::CoverType::Cache:
    case CoverOptions::CoverType::Album:{
      QUrl cover_url = QUrl::fromLocalFile(cover_file);
      SaveArtManualToSong(song, cover_url);
      return cover_url;
    }
  }

  return QUrl();

}

void AlbumCoverChoiceController::SaveCoverToFileManual(const Song &song, const AlbumCoverImageResult &result) {

  QString initial_file_name = "/";

  if (!song.effective_albumartist().isEmpty()) {
    initial_file_name = initial_file_name + song.effective_albumartist();
  }
  initial_file_name = initial_file_name + "-" + (song.effective_album().isEmpty() ? tr("unknown") : song.effective_album()) + ".jpg";
  initial_file_name = initial_file_name.toLower();
  initial_file_name.replace(QRegularExpression("\\s"), "-");
  initial_file_name.remove(QRegularExpression(QString(kInvalidFatCharactersRegex), QRegularExpression::CaseInsensitiveOption));

  QString save_filename = QFileDialog::getSaveFileName(this, tr("Save album cover"), GetInitialPathForFileDialog(song, initial_file_name), tr(kSaveImageFileFilter) + ";;" + tr(kAllFilesFilter));

  if (save_filename.isEmpty()) return;

  QFileInfo fileinfo(save_filename);
  if (fileinfo.suffix().isEmpty()) {
    save_filename.append(".jpg");
    fileinfo.setFile(save_filename);
  }

  if (!QImageWriter::supportedImageFormats().contains(fileinfo.completeSuffix().toUtf8().toLower())) {
    save_filename = Utilities::PathWithoutFilenameExtension(save_filename) + ".jpg";
    fileinfo.setFile(save_filename);
  }

  if (result.is_jpeg() && fileinfo.completeSuffix().compare("jpg", Qt::CaseInsensitive) == 0) {
    QFile file(save_filename);
    if (file.open(QIODevice::WriteOnly)) {
      if (file.write(result.image_data) <= 0) {
        qLog(Error) << "Failed writing cover to file" << save_filename << file.errorString();
        emit Error(tr("Failed writing cover to file %1: %2").arg(save_filename, file.errorString()));
      }
      file.close();
    }
    else {
      qLog(Error) << "Failed to open cover file" << save_filename << "for writing:" << file.errorString();
      emit Error(tr("Failed to open cover file %1 for writing: %2").arg(save_filename, file.errorString()));
    }
  }
  else {
    if (!result.image.save(save_filename)) {
      qLog(Error) << "Failed writing cover to file" << save_filename;
      emit Error(tr("Failed writing cover to file %1.").arg(save_filename));
    }
  }

}

QString AlbumCoverChoiceController::GetInitialPathForFileDialog(const Song &song, const QString &filename) {

  // Art automatic is first to show user which cover the album may be using now;
  // The song is using it if there's no manual path but we cannot use manual path here because it can contain cached paths
  if (!song.art_automatic().isEmpty() && !song.art_automatic().path().isEmpty() && !song.has_embedded_cover()) {
    if (song.art_automatic().scheme().isEmpty() && QFile::exists(QFileInfo(song.art_automatic().path()).path())) {
      return song.art_automatic().path();
    }
    else if (song.art_automatic().isLocalFile() && QFile::exists(QFileInfo(song.art_automatic().toLocalFile()).path())) {
      return song.art_automatic().toLocalFile();
    }
    // If no automatic art, start in the song's folder
  }
  else if (!song.url().isEmpty() && song.url().toLocalFile().contains('/')) {
    return song.url().toLocalFile().section('/', 0, -2) + filename;
    // Fallback - start in home
  }

  return QDir::home().absolutePath() + filename;

}

QUrl AlbumCoverChoiceController::LoadCoverFromURL(Song *song) {

  if (!song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return QUrl();

  AlbumCoverImageResult result = LoadImageFromURL();

  if (result.image.isNull()) {
    return QUrl();
  }
  else {
    return SaveCoverAutomatic(song, result);
  }

}

AlbumCoverImageResult AlbumCoverChoiceController::LoadImageFromURL() {

  if (!cover_from_url_dialog_) { cover_from_url_dialog_ = new CoverFromURLDialog(this); }

  return cover_from_url_dialog_->Exec();

}

QUrl AlbumCoverChoiceController::SearchForCover(Song *song) {

  if (!song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return QUrl();

  // Get something sensible to stick in the search box
  AlbumCoverImageResult result = SearchForImage(song);
  if (result.is_valid()) {
    return SaveCoverAutomatic(song, result);
  }
  else {
    return QUrl();
  }

}

AlbumCoverImageResult AlbumCoverChoiceController::SearchForImage(Song *song) {

  if (!song->url().isLocalFile()) return AlbumCoverImageResult();

  QString album = song->effective_album();
  album = album.remove(Song::kAlbumRemoveDisc).remove(Song::kAlbumRemoveMisc);

  // Get something sensible to stick in the search box
  return cover_searcher_->Exec(song->effective_albumartist(), album);

}

QUrl AlbumCoverChoiceController::UnsetCover(Song *song, const bool clear_art_automatic) {

  if (!song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return QUrl();

  QUrl cover_url = QUrl::fromLocalFile(Song::kManuallyUnsetCover);
  SaveArtManualToSong(song, cover_url, clear_art_automatic);

  return cover_url;

}

void AlbumCoverChoiceController::ClearCover(Song *song, const bool clear_art_automatic) {

  if (!song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return;

  song->clear_art_manual();
  if (clear_art_automatic) song->clear_art_automatic();
  SaveArtManualToSong(song, QUrl(), clear_art_automatic);

}

bool AlbumCoverChoiceController::DeleteCover(Song *song, const bool manually_unset) {

  if (!song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return false;

  if (song->has_embedded_cover() && song->save_embedded_cover_supported()) {
    SaveCoverEmbeddedAutomatic(*song, AlbumCoverImageResult());
  }

  QString art_automatic;
  QString art_manual;
  if (song->art_automatic().isValid() && song->art_automatic().isLocalFile()) {
    art_automatic = song->art_automatic().toLocalFile();
  }
  if (song->art_manual().isValid() && song->art_manual().isLocalFile()) {
    art_manual = song->art_manual().toLocalFile();
  }

  bool success = true;

  if (!art_automatic.isEmpty()) {
    QFile file(art_automatic);
    if (file.exists()) {
      if (file.remove()) {
        song->clear_art_automatic();
        if (art_automatic == art_manual) song->clear_art_manual();
      }
      else {
        success = false;
        qLog(Error) << "Failed to delete cover file" << art_automatic << file.errorString();
        emit Error(tr("Failed to delete cover file %1: %2").arg(art_automatic, file.errorString()));
      }
    }
    else song->clear_art_automatic();
  }
  else song->clear_art_automatic();

  if (!art_manual.isEmpty()) {
    QFile file(art_manual);
    if (file.exists()) {
      if (file.remove()) {
        song->clear_art_manual();
        if (art_automatic == art_manual) song->clear_art_automatic();
      }
      else {
        success = false;
        qLog(Error) << "Failed to delete cover file" << art_manual << file.errorString();
        emit Error(tr("Failed to delete cover file %1: %2").arg(art_manual, file.errorString()));
      }
    }
    else song->clear_art_manual();
  }
  else song->clear_art_manual();

  if (success) {
    if (manually_unset) UnsetCover(song, true);
    else ClearCover(song, true);
  }

  return success;

}

void AlbumCoverChoiceController::ShowCover(const Song &song, const QImage &image) {

  if (image.isNull()) {
    if ((song.art_manual().isValid() && song.art_manual().isLocalFile() && QFile::exists(song.art_manual().toLocalFile())) ||
        (song.art_automatic().isValid() && song.art_automatic().isLocalFile() && QFile::exists(song.art_automatic().toLocalFile())) ||
        song.has_embedded_cover()
    ) {
      QPixmap pixmap = ImageUtils::TryLoadPixmap(song.art_automatic(), song.art_manual(), song.url());
      if (!pixmap.isNull()) {
          pixmap.setDevicePixelRatio(devicePixelRatioF());
          ShowCover(song, pixmap);
      }
    }
  }
  else {
    QPixmap pixmap = QPixmap::fromImage(image);
    if (!pixmap.isNull()) {
        pixmap.setDevicePixelRatio(devicePixelRatioF());
        ShowCover(song, pixmap);
    }
  }

}

void AlbumCoverChoiceController::ShowCover(const Song &song, const QPixmap &pixmap) {

  QDialog *dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose, true);

  // Use Artist - Album as the window title
  QString title_text(song.effective_albumartist());
  if (!song.effective_album().isEmpty()) title_text += " - " + song.effective_album();

  QLabel *label = new QLabel(dialog);
  label->setPixmap(pixmap);

  // Add (WxHpx) to the title before possibly resizing
  title_text += " (" + QString::number(pixmap.width()) + "x" + QString::number(pixmap.height()) + "px)";

  // If the cover is larger than the screen, resize the window 85% seems to be enough to account for title bar and taskbar etc.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  QScreen *screen = QWidget::screen();
#else
  QScreen *screen = (window() && window()->windowHandle() ? window()->windowHandle()->screen() : QGuiApplication::primaryScreen());
#endif
  QRect screenGeometry = screen->availableGeometry();
  int desktop_height = screenGeometry.height();
  int desktop_width = screenGeometry.width();

  // Resize differently if monitor is in portrait mode
  if (desktop_width < desktop_height) {
    const int new_width = static_cast<int>(static_cast<double>(desktop_width) * 0.95);
    if (new_width < pixmap.width()) {
      label->setPixmap(pixmap.scaledToWidth(new_width * pixmap.devicePixelRatioF(), Qt::SmoothTransformation));
    }
  }
  else {
    const int new_height = static_cast<int>(static_cast<double>(desktop_height) * 0.85);
    if (new_height < pixmap.height()) {
      label->setPixmap(pixmap.scaledToHeight(new_height * pixmap.devicePixelRatioF(), Qt::SmoothTransformation));
    }
  }

  dialog->setWindowTitle(title_text);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
  dialog->setFixedSize(label->pixmap(Qt::ReturnByValue).size() / pixmap.devicePixelRatioF());
#else
  dialog->setFixedSize(label->pixmap()->size() / pixmap.devicePixelRatioF());
#endif
  dialog->show();

}

quint64 AlbumCoverChoiceController::SearchCoverAutomatically(const Song &song) {

  quint64 id = cover_fetcher_->FetchAlbumCover(song.effective_albumartist(), song.album(), song.title(), true);

  cover_fetching_tasks_[id] = song;

  return id;

}

void AlbumCoverChoiceController::AlbumCoverFetched(const quint64 id, const AlbumCoverImageResult &result, const CoverSearchStatistics &statistics) {

  Q_UNUSED(statistics);

  Song song;
  if (cover_fetching_tasks_.contains(id)) {
    song = cover_fetching_tasks_.take(id);
  }

  if (result.is_valid()) {
    SaveCoverAutomatic(&song, result);
  }

  emit AutomaticCoverSearchDone();

}

void AlbumCoverChoiceController::SaveArtAutomaticToSong(Song *song, const QUrl &art_automatic) {

  if (!song->is_valid()) return;

  song->set_art_automatic(art_automatic);
  if (song->has_embedded_cover()) {
    song->clear_art_manual();
  }

  if (song->source() == Song::Source::Collection) {
    app_->collection_backend()->UpdateAutomaticAlbumArtAsync(song->effective_albumartist(), song->album(), art_automatic, song->has_embedded_cover());
  }

  if (*song == app_->current_albumcover_loader()->last_song()) {
    app_->current_albumcover_loader()->LoadAlbumCover(*song);
  }

}

void AlbumCoverChoiceController::SaveArtManualToSong(Song *song, const QUrl &art_manual, const bool clear_art_automatic) {

  if (!song->is_valid()) return;

  song->set_art_manual(art_manual);
  if (clear_art_automatic) song->clear_art_automatic();

  // Update the backends.
  switch (song->source()) {
    case Song::Source::Collection:
      app_->collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual, clear_art_automatic);
      break;
    case Song::Source::LocalFile:
    case Song::Source::CDDA:
    case Song::Source::Device:
    case Song::Source::Stream:
    case Song::Source::RadioParadise:
    case Song::Source::SomaFM:
    case Song::Source::Unknown:
      break;
    case Song::Source::Tidal:
    case Song::Source::Qobuz:
    case Song::Source::Subsonic:
      InternetService *service = app_->internet_services()->ServiceBySource(song->source());
      if (!service) break;
      if (service->artists_collection_backend()) {
        service->artists_collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual, clear_art_automatic);
      }
      if (service->albums_collection_backend()) {
        service->albums_collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual, clear_art_automatic);
      }
      if (service->songs_collection_backend()) {
        service->songs_collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual, clear_art_automatic);
      }
      break;
  }

  if (*song == app_->current_albumcover_loader()->last_song()) {
    app_->current_albumcover_loader()->LoadAlbumCover(*song);
  }

}

QUrl AlbumCoverChoiceController::SaveCoverToFileAutomatic(const Song *song, const AlbumCoverImageResult &result, const bool force_overwrite) {

  return SaveCoverToFileAutomatic(song->source(),
                                  song->effective_albumartist(),
                                  song->effective_album(),
                                  song->album_id(),
                                  song->url().adjusted(QUrl::RemoveFilename).path(),
                                  result,
                                  force_overwrite);

}

QUrl AlbumCoverChoiceController::SaveCoverToFileAutomatic(const Song::Source source,
                                                          const QString &artist,
                                                          const QString &album,
                                                          const QString &album_id,
                                                          const QString &album_dir,
                                                          const AlbumCoverImageResult &result,
                                                          const bool force_overwrite) {

  QString filepath = CoverUtils::CoverFilePath(cover_options_, source, artist, album, album_id, album_dir, result.cover_url, "jpg");
  if (filepath.isEmpty()) return QUrl();

  QFile file(filepath);
  // Don't overwrite when saving in album dir if the filename is set to pattern unless "force_overwrite" is set.
  if (source == Song::Source::Collection && !cover_options_.cover_overwrite && !force_overwrite && get_save_album_cover_type() == CoverOptions::CoverType::Album && cover_options_.cover_filename == CoverOptions::CoverFilename::Pattern && file.exists()) {
    while (file.exists()) {
      QFileInfo fileinfo(file.fileName());
      file.setFileName(fileinfo.path() + "/0" + fileinfo.fileName());
    }
    filepath = file.fileName();
  }

  QUrl cover_url;
  if (result.is_jpeg()) {
    if (file.open(QIODevice::WriteOnly)) {
      if (file.write(result.image_data) > 0) {
        cover_url = QUrl::fromLocalFile(filepath);
      }
      else {
        qLog(Error) << "Failed to write cover to file" << file.fileName() << file.errorString();
        emit Error(tr("Failed to write cover to file %1: %2").arg(file.fileName(), file.errorString()));
      }
      file.close();
    }
    else {
      qLog(Error) << "Failed to open cover file" << file.fileName() << "for writing:" << file.errorString();
      emit Error(tr("Failed to open cover file %1 for writing: %2").arg(file.fileName(), file.errorString()));
    }
  }
  else {
    if (result.image.save(filepath, "JPG")) cover_url = QUrl::fromLocalFile(filepath);
  }

  return cover_url;

}

void AlbumCoverChoiceController::SaveCoverEmbeddedAutomatic(const Song &song, const AlbumCoverImageResult &result) {

  if (song.source() == Song::Source::Collection) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QFuture<SongList> future = QtConcurrent::run(&CollectionBackend::GetAlbumSongs, app_->collection_backend(), song.effective_albumartist(), song.effective_album(), CollectionFilterOptions());
#else
    QFuture<SongList> future = QtConcurrent::run(app_->collection_backend(), &CollectionBackend::GetAlbumSongs, song.effective_albumartist(), song.effective_album(), CollectionFilterOptions());
#endif
    QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>();
    QObject::connect(watcher, &QFutureWatcher<SongList>::finished, this, [this, watcher, song, result]() {
      SongList songs = watcher->result();
      watcher->deleteLater();
      QList<QUrl> urls;
      urls.reserve(songs.count());
      for (const Song &s : songs) urls << s.url();
      if (result.is_jpeg()) {
        quint64 id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.image_data);
        QMutexLocker l(&mutex_cover_save_tasks_);
        cover_save_tasks_.insert(id, song);
      }
      else {
        quint64 id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, result.image);
        QMutexLocker l(&mutex_cover_save_tasks_);
        cover_save_tasks_.insert(id, song);
      }
    });
    watcher->setFuture(future);
  }
  else {
    if (result.is_jpeg()) {
      app_->album_cover_loader()->SaveEmbeddedCoverAsync(song.url().toLocalFile(), result.image_data);
    }
    else {
      app_->album_cover_loader()->SaveEmbeddedCoverAsync(song.url().toLocalFile(), result.image);
    }
  }

}

void AlbumCoverChoiceController::SaveCoverEmbeddedAutomatic(const Song &song, const QUrl &cover_url) {

  SaveCoverEmbeddedAutomatic(song, cover_url.toLocalFile());

}

void AlbumCoverChoiceController::SaveCoverEmbeddedAutomatic(const Song &song, const QString &cover_filename) {

  if (song.source() == Song::Source::Collection) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QFuture<SongList> future = QtConcurrent::run(&CollectionBackend::GetAlbumSongs, app_->collection_backend(), song.effective_albumartist(), song.effective_album(), CollectionFilterOptions());
#else
    QFuture<SongList> future = QtConcurrent::run(app_->collection_backend(), &CollectionBackend::GetAlbumSongs, song.effective_albumartist(), song.effective_album(), CollectionFilterOptions());
#endif
    QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>();
    QObject::connect(watcher, &QFutureWatcher<SongList>::finished, this, [this, watcher, song, cover_filename]() {
      SongList songs = watcher->result();
      watcher->deleteLater();
      QList<QUrl> urls;
      urls.reserve(songs.count());
      for (const Song &s : songs) urls << s.url();
      quint64 id = app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, cover_filename);
      QMutexLocker l(&mutex_cover_save_tasks_);
      cover_save_tasks_.insert(id, song);
    });
    watcher->setFuture(future);
  }
  else {
    app_->album_cover_loader()->SaveEmbeddedCoverAsync(song.url().toLocalFile(), cover_filename);
  }

}

void AlbumCoverChoiceController::SaveCoverEmbeddedAutomatic(const QList<QUrl> &urls, const QImage &image) {

  app_->album_cover_loader()->SaveEmbeddedCoverAsync(urls, image);

}

bool AlbumCoverChoiceController::IsKnownImageExtension(const QString &suffix) {

  if (!sImageExtensions) {
    sImageExtensions = new QSet<QString>();
   (*sImageExtensions) << "png" << "jpg" << "jpeg" << "bmp" << "gif" << "xpm" << "pbm" << "pgm" << "ppm" << "xbm";
  }

  return sImageExtensions->contains(suffix);

}

bool AlbumCoverChoiceController::CanAcceptDrag(const QDragEnterEvent *e) {

  for (const QUrl &url : e->mimeData()->urls()) {
    const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
    if (IsKnownImageExtension(suffix)) return true;
  }
  return e->mimeData()->hasImage();

}

QUrl AlbumCoverChoiceController::SaveCover(Song *song, const QDropEvent *e) {

  for (const QUrl &url : e->mimeData()->urls()) {

    const QString filename = url.toLocalFile();
    const QString suffix = QFileInfo(filename).suffix().toLower();

    if (IsKnownImageExtension(suffix)) {
      if (get_save_album_cover_type() == CoverOptions::CoverType::Embedded && song->save_embedded_cover_supported()) {
        SaveCoverEmbeddedAutomatic(*song, filename);
        return QUrl::fromLocalFile(Song::kEmbeddedCover);
      }
      else {
        SaveArtManualToSong(song, url);
      }
      return url;
    }
  }

  if (e->mimeData()->hasImage()) {
    QImage image = qvariant_cast<QImage>(e->mimeData()->imageData());
    if (!image.isNull()) {
      return SaveCoverAutomatic(song, AlbumCoverImageResult(image));
    }
  }

  return QUrl();

}

QUrl AlbumCoverChoiceController::SaveCoverAutomatic(Song *song, const AlbumCoverImageResult &result) {

  QUrl cover_url;
  switch(get_save_album_cover_type()) {
    case CoverOptions::CoverType::Embedded:{
      if (song->save_embedded_cover_supported()) {
        SaveCoverEmbeddedAutomatic(*song, result);
        cover_url = QUrl::fromLocalFile(Song::kEmbeddedCover);
        break;
      }
    }
    [[fallthrough]];
    case CoverOptions::CoverType::Cache:
    case CoverOptions::CoverType::Album:{
      cover_url = SaveCoverToFileAutomatic(song, result);
      if (!cover_url.isEmpty()) SaveArtManualToSong(song, cover_url);
      break;
    }
  }

  return cover_url;

}

void AlbumCoverChoiceController::SaveEmbeddedCoverAsyncFinished(quint64 id, const bool success, const bool cleared) {

  if (!cover_save_tasks_.contains(id)) return;

  Song song = cover_save_tasks_.take(id);
  if (success) {
    if (cleared) SaveArtAutomaticToSong(&song, QUrl());
    else SaveArtAutomaticToSong(&song, QUrl::fromLocalFile(Song::kEmbeddedCover));
  }

}
