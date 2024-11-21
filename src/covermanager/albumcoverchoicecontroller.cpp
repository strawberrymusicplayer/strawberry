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

#include "config.h"

#include <utility>
#include <memory>

#include <QtGlobal>
#include <QGuiApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QScreen>
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

#include "constants/filenameconstants.h"
#include "constants/filefilterconstants.h"
#include "constants/coverssettings.h"
#include "utilities/strutils.h"
#include "utilities/mimeutils.h"
#include "utilities/coveroptions.h"
#include "utilities/coverutils.h"
#include "utilities/screenutils.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionfilteroptions.h"
#include "collection/collectionbackend.h"
#include "streaming/streamingservices.h"
#include "streaming/streamingservice.h"
#include "albumcoverchoicecontroller.h"
#include "albumcoverfetcher.h"
#include "albumcoverloader.h"
#include "albumcoversearcher.h"
#include "albumcoverimageresult.h"
#include "coverfromurldialog.h"
#include "currentalbumcoverloader.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

QSet<QString> *AlbumCoverChoiceController::sImageExtensions = nullptr;

AlbumCoverChoiceController::AlbumCoverChoiceController(QWidget *parent)
    : QWidget(parent),
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

  cover_from_file_ = new QAction(IconLoader::Load(u"document-open"_s), tr("Load cover from disk..."), this);
  cover_to_file_ = new QAction(IconLoader::Load(u"document-save"_s), tr("Save cover to disk..."), this);
  cover_from_url_ = new QAction(IconLoader::Load(u"download"_s), tr("Load cover from URL..."), this);
  search_for_cover_ = new QAction(IconLoader::Load(u"search"_s), tr("Search for album covers..."), this);
  unset_cover_ = new QAction(IconLoader::Load(u"list-remove"_s), tr("Unset cover"), this);
  delete_cover_ = new QAction(IconLoader::Load(u"list-remove"_s), tr("Delete cover"), this);
  clear_cover_ = new QAction(IconLoader::Load(u"list-remove"_s), tr("Clear cover"), this);
  separator1_ = new QAction(this);
  separator1_->setSeparator(true);
  show_cover_ = new QAction(IconLoader::Load(u"zoom-in"_s), tr("Show fullsize..."), this);

  search_cover_auto_ = new QAction(tr("Search automatically"), this);
  search_cover_auto_->setCheckable(true);
  search_cover_auto_->setChecked(false);

  separator2_ = new QAction(this);
  separator2_->setSeparator(true);

  ReloadSettings();

}

AlbumCoverChoiceController::~AlbumCoverChoiceController() = default;

void AlbumCoverChoiceController::Init(const SharedPtr<NetworkAccessManager> network,
                                      const SharedPtr<TagReaderClient> tagreader_client,
                                      const SharedPtr<CollectionBackend> collection_backend,
                                      const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                      const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                      const SharedPtr<CoverProviders> cover_providers,
                                      const SharedPtr<StreamingServices> streaming_services) {

  network_ = network;
  tagreader_client_ = tagreader_client;
  collection_backend_ = collection_backend;
  current_albumcover_loader_ = current_albumcover_loader;
  streaming_services_ = streaming_services;

  cover_fetcher_ = new AlbumCoverFetcher(cover_providers, network, this);
  cover_searcher_ = new AlbumCoverSearcher(QIcon(u":/pictures/cdcase.png"_s), albumcover_loader, this);
  cover_searcher_->Init(cover_fetcher_);

  QObject::connect(cover_fetcher_, &AlbumCoverFetcher::AlbumCoverFetched, this, &AlbumCoverChoiceController::AlbumCoverFetched);

}

void AlbumCoverChoiceController::ReloadSettings() {

  Settings s;
  s.beginGroup(CoversSettings::kSettingsGroup);
  cover_options_.cover_type = static_cast<CoverOptions::CoverType>(s.value(CoversSettings::kSaveType, static_cast<int>(CoverOptions::CoverType::Cache)).toInt());
  cover_options_.cover_filename = static_cast<CoverOptions::CoverFilename>(s.value(CoversSettings::kSaveFilename, static_cast<int>(CoverOptions::CoverFilename::Pattern)).toInt());
  cover_options_.cover_pattern = s.value(CoversSettings::kSavePattern, u"%albumartist-%album"_s).toString();
  cover_options_.cover_overwrite = s.value(CoversSettings::kSaveOverwrite, false).toBool();
  cover_options_.cover_lowercase = s.value(CoversSettings::kSaveLowercase, false).toBool();
  cover_options_.cover_replace_spaces = s.value(CoversSettings::kSaveReplaceSpaces, false).toBool();
  s.endGroup();

  cover_types_ = AlbumCoverLoaderOptions::LoadTypes();

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

  if (!song->url().isValid() || !song->url().isLocalFile()) {
    return AlbumCoverImageResult();
  }

  QString cover_file = QFileDialog::getOpenFileName(this, tr("Load cover from disk"), GetInitialPathForFileDialog(*song, QString()), tr(kLoadImageFileFilter) + u";;"_s + tr(kAllFilesFilterSpec));
  if (cover_file.isEmpty()) return AlbumCoverImageResult();

  QFile file(cover_file);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Failed to open cover file" << cover_file << "for reading:" << file.errorString();
    Q_EMIT Error(tr("Failed to open cover file %1 for reading: %2").arg(cover_file, file.errorString()));
    return AlbumCoverImageResult();
  }
  AlbumCoverImageResult result;
  result.image_data = file.readAll();
  file.close();
  if (result.image_data.isEmpty()) {
    qLog(Error) << "Cover file" << cover_file << "is empty.";
    Q_EMIT Error(tr("Cover file %1 is empty.").arg(cover_file));
    return AlbumCoverImageResult();
  }

  if (result.image.loadFromData(result.image_data)) {
    result.cover_url = QUrl::fromLocalFile(cover_file);
    result.mime_type = Utilities::MimeTypeFromData(result.image_data);
  }

  return result;

}

QUrl AlbumCoverChoiceController::LoadCoverFromFile(Song *song) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return QUrl();

  QString cover_file = QFileDialog::getOpenFileName(this, tr("Load cover from disk"), GetInitialPathForFileDialog(*song, QString()), tr(kLoadImageFileFilter) + u";;"_s + tr(kAllFilesFilterSpec));
  if (cover_file.isEmpty() || QImage(cover_file).isNull()) return QUrl();

  switch (get_save_album_cover_type()) {
    case CoverOptions::CoverType::Embedded:
      if (song->save_embedded_cover_supported()) {
        SaveCoverEmbeddedToCollectionSongs(*song, cover_file);
        return QUrl();
      }
      [[fallthrough]];
    case CoverOptions::CoverType::Cache:
    case CoverOptions::CoverType::Album:{
      const QUrl cover_url = QUrl::fromLocalFile(cover_file);
      SaveArtManualToSong(song, cover_url);
      return cover_url;
    }
  }

  return QUrl();

}

void AlbumCoverChoiceController::SaveCoverToFileManual(const Song &song, const AlbumCoverImageResult &result) {

  QString initial_file_name = "/"_L1;

  if (!song.effective_albumartist().isEmpty()) {
    initial_file_name = initial_file_name + song.effective_albumartist();
  }
  initial_file_name = initial_file_name + QLatin1Char('-') + (song.effective_album().isEmpty() ? tr("unknown") : song.effective_album()) + ".jpg"_L1;
  initial_file_name = initial_file_name.toLower();
  static const QRegularExpression regex_whitespaces(u"\\s"_s);
  initial_file_name.replace(regex_whitespaces, u"-"_s);
  static const QRegularExpression regex_invalid_fat_characters(QLatin1String(kInvalidFatCharactersRegex), QRegularExpression::CaseInsensitiveOption);
  initial_file_name.remove(regex_invalid_fat_characters);

  QString save_filename = QFileDialog::getSaveFileName(this, tr("Save album cover"), GetInitialPathForFileDialog(song, initial_file_name), tr(kSaveImageFileFilter) + u";;"_s + tr(kAllFilesFilterSpec));

  if (save_filename.isEmpty()) return;

  QFileInfo fileinfo(save_filename);
  if (fileinfo.suffix().isEmpty()) {
    save_filename.append(".jpg"_L1);
    fileinfo.setFile(save_filename);
  }

  if (!QImageWriter::supportedImageFormats().contains(fileinfo.completeSuffix().toUtf8().toLower())) {
    save_filename = Utilities::PathWithoutFilenameExtension(save_filename) + ".jpg"_L1;
    fileinfo.setFile(save_filename);
  }

  if (result.is_jpeg() && fileinfo.completeSuffix().compare("jpg"_L1, Qt::CaseInsensitive) == 0) {
    QFile file(save_filename);
    if (!file.open(QIODevice::WriteOnly)) {
      qLog(Error) << "Failed to open cover file" << save_filename << "for writing:" << file.errorString();
      Q_EMIT Error(tr("Failed to open cover file %1 for writing: %2").arg(save_filename, file.errorString()));
      file.close();
      return;
    }
    if (file.write(result.image_data) <= 0) {
      qLog(Error) << "Failed writing cover to file" << save_filename << file.errorString();
      Q_EMIT Error(tr("Failed writing cover to file %1: %2").arg(save_filename, file.errorString()));
      file.close();
      return;
    }
    file.close();
  }
  else {
    if (!result.image.save(save_filename)) {
      qLog(Error) << "Failed writing cover to file" << save_filename;
      Q_EMIT Error(tr("Failed writing cover to file %1.").arg(save_filename));
    }
  }

}

QString AlbumCoverChoiceController::GetInitialPathForFileDialog(const Song &song, const QString &filename) {

  // Art automatic is first to show user which cover the album may be using now;
  // The song is using it if there's no manual path but we cannot use manual path here because it can contain cached paths
  if (song.art_automatic_is_valid()) {
     return song.art_automatic().toLocalFile();
  }

  // If no automatic art, start in the song's folder
  if (!song.url().isEmpty() && song.url().isValid() && song.url().isLocalFile() && song.url().toLocalFile().contains(u'/')) {
    return song.url().toLocalFile().section(u'/', 0, -2) + filename;
  }

  return QDir::home().absolutePath() + filename;

}

void AlbumCoverChoiceController::LoadCoverFromURL(Song *song) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return;

  const AlbumCoverImageResult result = LoadImageFromURL();
  if (!result.image.isNull()) {
    SaveCoverAutomatic(song, result);
  }

}

AlbumCoverImageResult AlbumCoverChoiceController::LoadImageFromURL() {

  if (!cover_from_url_dialog_) { cover_from_url_dialog_ = new CoverFromURLDialog(network_, this); }

  return cover_from_url_dialog_->Exec();

}

void AlbumCoverChoiceController::SearchForCover(Song *song) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return;

  // Get something sensible to stick in the search box
  AlbumCoverImageResult result = SearchForImage(song);
  if (result.is_valid()) {
    SaveCoverAutomatic(song, result);
  }

}

AlbumCoverImageResult AlbumCoverChoiceController::SearchForImage(Song *song) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return AlbumCoverImageResult();

  QString album = song->effective_album();
  album = Song::AlbumRemoveDiscMisc(album);

  // Get something sensible to stick in the search box
  return cover_searcher_->Exec(song->effective_albumartist(), album);

}

void AlbumCoverChoiceController::UnsetCover(Song *song) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return;

  UnsetAlbumCoverForSong(song);

}

void AlbumCoverChoiceController::ClearCover(Song *song) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return;

  ClearAlbumCoverForSong(song);

}

bool AlbumCoverChoiceController::DeleteCover(Song *song, const bool unset) {

  if (!song->url().isValid() || !song->url().isLocalFile() || song->effective_albumartist().isEmpty() || song->album().isEmpty()) return false;

  if (song->art_embedded() && song->save_embedded_cover_supported()) {
    SaveCoverEmbeddedToCollectionSongs(*song, AlbumCoverImageResult());
  }

  bool success = true;

  if (song->art_automatic().isValid() && song->art_automatic().isLocalFile()) {
    const QString art_automatic = song->art_automatic().toLocalFile();
    QFile file(art_automatic);
    if (file.exists()) {
      if (file.remove()) {
        song->clear_art_automatic();
      }
      else {
        success = false;
        qLog(Error) << "Failed to delete cover file" << art_automatic << file.errorString();
        Q_EMIT Error(tr("Failed to delete cover file %1: %2").arg(art_automatic, file.errorString()));
      }
    }
    else song->clear_art_automatic();
  }
  else song->clear_art_automatic();

  if (song->art_manual().isValid() && song->art_manual().isLocalFile()) {
    const QString art_manual = song->art_manual().toLocalFile();
    QFile file(art_manual);
    if (file.exists()) {
      if (file.remove()) {
        song->clear_art_manual();
      }
      else {
        success = false;
        qLog(Error) << "Failed to delete cover file" << art_manual << file.errorString();
        Q_EMIT Error(tr("Failed to delete cover file %1: %2").arg(art_manual, file.errorString()));
      }
    }
    else song->clear_art_manual();
  }
  else song->clear_art_manual();

  if (success) {
    if (unset) UnsetCover(song);
    else ClearCover(song);
  }

  return success;

}

void AlbumCoverChoiceController::ShowCover(const Song &song, const QImage &image) {

  if (!image.isNull()) {
    QPixmap pixmap = QPixmap::fromImage(image);
    if (!pixmap.isNull()) {
      pixmap.setDevicePixelRatio(devicePixelRatioF());
      ShowCover(song, pixmap);
      return;
    }
  }

  for (const AlbumCoverLoaderOptions::Type type : std::as_const(cover_types_)) {
    switch (type) {
      case AlbumCoverLoaderOptions::Type::Unset:{
        if (song.art_unset()) {
          return;
        }
        break;
      }
      case AlbumCoverLoaderOptions::Type::Manual:{
        QPixmap pixmap;
        if (song.art_manual_is_valid() && song.art_manual().isLocalFile() && pixmap.load(song.art_manual().toLocalFile())) {
          pixmap.setDevicePixelRatio(devicePixelRatioF());
          ShowCover(song, pixmap);
          return;
        }
        break;
      }
      case AlbumCoverLoaderOptions::Type::Embedded:{
        if (song.art_embedded() && !song.url().isEmpty() && song.url().isValid() && song.url().isLocalFile()) {
          QImage image_embedded_cover;
          const TagReaderResult result = tagreader_client_->LoadCoverImageBlocking(song.url().toLocalFile(), image_embedded_cover);
          if (result.success() && !image_embedded_cover.isNull()) {
            QPixmap pixmap = QPixmap::fromImage(image_embedded_cover);
            if (!pixmap.isNull()) {
              pixmap.setDevicePixelRatio(devicePixelRatioF());
              ShowCover(song, pixmap);
              return;
            }
          }
        }
        break;
      }
      case AlbumCoverLoaderOptions::Type::Automatic:{
        QPixmap pixmap;
        if (song.art_automatic_is_valid() && song.art_automatic().isLocalFile() && pixmap.load(song.art_automatic().toLocalFile())) {
          pixmap.setDevicePixelRatio(devicePixelRatioF());
          ShowCover(song, pixmap);
          return;
        }
        break;
      }
    }
  }

}

void AlbumCoverChoiceController::ShowCover(const Song &song, const QPixmap &pixmap) {

  QDialog *dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose, true);

  // Use Artist - Album as the window title
  QString title_text(song.effective_albumartist());
  if (!song.effective_album().isEmpty()) title_text += " - "_L1 + song.effective_album();

  QLabel *label = new QLabel(dialog);
  label->setPixmap(pixmap);

  // Add (WxHpx) to the title before possibly resizing
  title_text += QLatin1String(" (") + QString::number(pixmap.width()) + QLatin1Char('x') + QString::number(pixmap.height()) + "px)"_L1;

  // If the cover is larger than the screen, resize the window 85% seems to be enough to account for title bar and taskbar etc.
  QScreen *screen = Utilities::GetScreen(this);
  QRect screenGeometry = screen->availableGeometry();
  int desktop_height = screenGeometry.height();
  int desktop_width = screenGeometry.width();

  // Resize differently if monitor is in portrait mode
  if (desktop_width < desktop_height) {
    const int new_width = static_cast<int>(static_cast<double>(desktop_width) * 0.95);
    if (new_width < pixmap.width()) {
      label->setPixmap(pixmap.scaledToWidth(static_cast<int>(new_width * pixmap.devicePixelRatioF()), Qt::SmoothTransformation));
    }
  }
  else {
    const int new_height = static_cast<int>(static_cast<double>(desktop_height) * 0.85);
    if (new_height < pixmap.height()) {
      label->setPixmap(pixmap.scaledToHeight(static_cast<int>(new_height * pixmap.devicePixelRatioF()), Qt::SmoothTransformation));
    }
  }

  dialog->setWindowTitle(title_text);
  dialog->setFixedSize(label->pixmap(Qt::ReturnByValue).size() / pixmap.devicePixelRatioF());
  dialog->show();

}

quint64 AlbumCoverChoiceController::SearchCoverAutomatically(const Song &song) {

  quint64 id = cover_fetcher_->FetchAlbumCover(song.effective_albumartist(), song.album(), song.title(), true);

  cover_fetching_tasks_.insert(id, song);

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

  Q_EMIT AutomaticCoverSearchDone();

}

void AlbumCoverChoiceController::SaveArtEmbeddedToSong(Song *song, const bool art_embedded) {

  if (!song->is_valid()) return;

  song->set_art_embedded(art_embedded);
  song->set_art_unset(false);

  if (song->source() == Song::Source::Collection) {
    collection_backend_->UpdateEmbeddedAlbumArtAsync(song->effective_albumartist(), song->album(), art_embedded);
  }

  if (*song == current_albumcover_loader_->last_song()) {
    current_albumcover_loader_->LoadAlbumCover(*song);
  }

}

void AlbumCoverChoiceController::SaveArtManualToSong(Song *song, const QUrl &art_manual) {

  if (!song->is_valid()) return;

  song->set_art_manual(art_manual);
  song->set_art_unset(false);

  // Update the backends.
  switch (song->source()) {
    case Song::Source::Collection:
      collection_backend_->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual);
      break;
    case Song::Source::LocalFile:
    case Song::Source::CDDA:
    case Song::Source::Device:
    case Song::Source::Stream:
    case Song::Source::RadioParadise:
    case Song::Source::SomaFM:
    case Song::Source::Unknown:
      break;
    case Song::Source::Subsonic:
    case Song::Source::Tidal:
    case Song::Source::Spotify:
    case Song::Source::Qobuz:
      StreamingServicePtr service = streaming_services_->ServiceBySource(song->source());
      if (!service) break;
      if (service->artists_collection_backend()) {
        service->artists_collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual);
      }
      if (service->albums_collection_backend()) {
        service->albums_collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual);
      }
      if (service->songs_collection_backend()) {
        service->songs_collection_backend()->UpdateManualAlbumArtAsync(song->effective_albumartist(), song->album(), art_manual);
      }
      break;
  }

  if (*song == current_albumcover_loader_->last_song()) {
    current_albumcover_loader_->LoadAlbumCover(*song);
  }

}

void AlbumCoverChoiceController::ClearAlbumCoverForSong(Song *song) {

  if (!song->is_valid()) return;

  song->set_art_unset(false);
  song->set_art_embedded(false);
  song->clear_art_automatic();
  song->clear_art_manual();

  if (song->source() == Song::Source::Collection) {
    collection_backend_->ClearAlbumArtAsync(song->effective_albumartist(), song->album(), false);
  }

  if (*song == current_albumcover_loader_->last_song()) {
    current_albumcover_loader_->LoadAlbumCover(*song);
  }

}

void AlbumCoverChoiceController::UnsetAlbumCoverForSong(Song *song) {

  if (!song->is_valid()) return;

  song->set_art_unset(true);
  song->set_art_embedded(false);
  song->clear_art_manual();
  song->clear_art_automatic();

  if (song->source() == Song::Source::Collection) {
    collection_backend_->UnsetAlbumArtAsync(song->effective_albumartist(), song->album());
  }

  if (*song == current_albumcover_loader_->last_song()) {
    current_albumcover_loader_->LoadAlbumCover(*song);
  }

}

QUrl AlbumCoverChoiceController::SaveCoverToFileAutomatic(const Song *song, const AlbumCoverImageResult &result, const bool force_overwrite) {

  return SaveCoverToFileAutomatic(song->source(),
                                  song->effective_albumartist(),
                                  song->effective_album(),
                                  song->album_id(),
                                  QFileInfo(song->url().toLocalFile()).path(),
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

  QString filepath = CoverUtils::CoverFilePath(cover_options_, source, artist, album, album_id, album_dir, result.cover_url, u"jpg"_s);
  if (filepath.isEmpty()) return QUrl();

  QFile file(filepath);
  // Don't overwrite when saving in album dir if the filename is set to pattern unless "force_overwrite" is set.
  if (source == Song::Source::Collection && !cover_options_.cover_overwrite && !force_overwrite && get_save_album_cover_type() == CoverOptions::CoverType::Album && cover_options_.cover_filename == CoverOptions::CoverFilename::Pattern && file.exists()) {
    while (file.exists()) {
      QFileInfo fileinfo(file.fileName());
      file.setFileName(fileinfo.path() + "/0"_L1 + fileinfo.fileName());
    }
    filepath = file.fileName();
  }

  if (!result.image_data.isEmpty() && result.is_jpeg()) {
    if (file.open(QIODevice::WriteOnly)) {
      if (file.write(result.image_data) > 0) {
        file.close();
        return QUrl::fromLocalFile(filepath);
      }
      else {
        qLog(Error) << "Failed to write cover to file" << file.fileName() << file.errorString();
        Q_EMIT Error(tr("Failed to write cover to file %1: %2").arg(file.fileName(), file.errorString()));
      }
      file.close();
    }
    else {
      qLog(Error) << "Failed to open cover file" << file.fileName() << "for writing:" << file.errorString();
      Q_EMIT Error(tr("Failed to open cover file %1 for writing: %2").arg(file.fileName(), file.errorString()));
    }
  }
  else {
    if (result.image.save(filepath, "JPG")) {
      return QUrl::fromLocalFile(filepath);
    }
  }

  return QUrl();

}

void AlbumCoverChoiceController::SaveCoverEmbeddedToCollectionSongs(const Song &song, const AlbumCoverImageResult &result) {

  SaveCoverEmbeddedToCollectionSongs(song, QString(), result.image_data, result.mime_type);

}

void AlbumCoverChoiceController::SaveCoverEmbeddedToCollectionSongs(const Song &song, const QString &cover_filename, const QByteArray &image_data, const QString &mime_type) {

  if (song.source() == Song::Source::Collection) {
    SaveCoverEmbeddedToCollectionSongs(song.effective_albumartist(), song.effective_album(), cover_filename, image_data, mime_type);
  }
  else {
    SaveCoverEmbeddedToSong(song, cover_filename, image_data, mime_type);
  }

}

void AlbumCoverChoiceController::SaveCoverEmbeddedToCollectionSongs(const QString &effective_albumartist, const QString &effective_album, const QString &cover_filename, const QByteArray &image_data, const QString &mime_type) {

  QFuture<SongList> future = QtConcurrent::run(&CollectionBackend::GetAlbumSongs, collection_backend_, effective_albumartist, effective_album, CollectionFilterOptions());
  QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>();
  QObject::connect(watcher, &QFutureWatcher<SongList>::finished, this, [this, watcher, cover_filename, image_data, mime_type]() {
    const SongList collection_songs = watcher->result();
    watcher->deleteLater();
    for (const Song &collection_song : collection_songs) {
      SaveCoverEmbeddedToSong(collection_song, cover_filename, image_data, mime_type);
    }
  });
  watcher->setFuture(future);

}

void AlbumCoverChoiceController::SaveCoverEmbeddedToSong(const Song &song, const QString &cover_filename, const QByteArray &image_data, const QString &mime_type) {

  QMutexLocker l(&mutex_cover_save_tasks_);
  cover_save_tasks_.append(song);
  const bool art_embedded = !image_data.isNull();
  TagReaderReplyPtr reply = tagreader_client_->SaveCoverAsync(song.url().toLocalFile(), SaveTagCoverData(cover_filename, image_data, mime_type));
  SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
  *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, song, art_embedded, connection]() {
    SaveEmbeddedCoverFinished(reply, song, art_embedded);
    QObject::disconnect(*connection);
  });

}

bool AlbumCoverChoiceController::IsKnownImageExtension(const QString &suffix) {

  if (!sImageExtensions) {
    sImageExtensions = new QSet<QString>();
   (*sImageExtensions) << u"png"_s << u"jpg"_s << u"jpeg"_s << u"bmp"_s << u"gif"_s << u"xpm"_s << u"pbm"_s << u"pgm"_s << u"ppm"_s << u"xbm"_s;
  }

  return sImageExtensions->contains(suffix);

}

bool AlbumCoverChoiceController::CanAcceptDrag(const QDragEnterEvent *e) {

  const QList<QUrl> urls = e->mimeData()->urls();
  for (const QUrl &url : urls) {
    const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
    if (IsKnownImageExtension(suffix)) return true;
  }
  return e->mimeData()->hasImage();

}

void AlbumCoverChoiceController::SaveCover(Song *song, const QDropEvent *e) {

  const QList<QUrl> urls = e->mimeData()->urls();
  for (const QUrl &url : urls) {

    const QString filename = url.toLocalFile();
    const QString suffix = QFileInfo(filename).suffix().toLower();

    if (IsKnownImageExtension(suffix)) {
      if (get_save_album_cover_type() == CoverOptions::CoverType::Embedded && song->save_embedded_cover_supported()) {
        SaveCoverEmbeddedToCollectionSongs(*song, filename);
      }
      else {
        SaveArtManualToSong(song, url);
      }
      return;
    }
  }

  if (e->mimeData()->hasImage()) {
    QImage image = qvariant_cast<QImage>(e->mimeData()->imageData());
    if (!image.isNull()) {
      SaveCoverAutomatic(song, AlbumCoverImageResult(image));
    }
  }

}

QUrl AlbumCoverChoiceController::SaveCoverAutomatic(Song *song, const AlbumCoverImageResult &result) {

  QUrl cover_url;
  switch(get_save_album_cover_type()) {
    case CoverOptions::CoverType::Embedded:{
      if (song->save_embedded_cover_supported()) {
        SaveCoverEmbeddedToCollectionSongs(*song, result);
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

void AlbumCoverChoiceController::SaveEmbeddedCoverFinished(TagReaderReplyPtr reply, Song song, const bool art_embedded) {

  if (!cover_save_tasks_.contains(song)) return;
  cover_save_tasks_.removeAll(song);

  if (reply->success()) {
    SaveArtEmbeddedToSong(&song, art_embedded);
  }
  else {
    Q_EMIT Error(tr("Could not save cover to file %1.").arg(song.url().toLocalFile()));
  }

}
