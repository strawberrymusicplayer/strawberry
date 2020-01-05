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

#include "config.h"

#include <QtGlobal>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QWidget>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QSet>
#include <QMimeData>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QRegExp>
#include <QUrl>
#include <QImage>
#include <QImageWriter>
#include <QPixmap>
#include <QIcon>
#include <QRect>
#include <QAction>
#include <QFileDialog>
#include <QLabel>
#include <QtEvents>

#include "core/utilities.h"
#include "core/song.h"
#include "core/iconloader.h"
#include "core/application.h"

#include "collection/collectionbackend.h"
#include "settings/collectionsettingspage.h"
#include "organise/organiseformat.h"
#include "internet/internetservices.h"
#include "internet/internetservice.h"
#include "albumcoverchoicecontroller.h"
#include "albumcoverfetcher.h"
#include "albumcoverloader.h"
#include "albumcoversearcher.h"
#include "coverfromurldialog.h"
#include "currentalbumcoverloader.h"

const char *AlbumCoverChoiceController::kLoadImageFileFilter = QT_TR_NOOP("Images (*.png *.jpg *.jpeg *.bmp *.gif *.xpm *.pbm *.pgm *.ppm *.xbm)");
const char *AlbumCoverChoiceController::kSaveImageFileFilter = QT_TR_NOOP("Images (*.png *.jpg *.jpeg *.bmp *.xpm *.pbm *.ppm *.xbm)");
const char *AlbumCoverChoiceController::kAllFilesFilter = QT_TR_NOOP("All files (*)");

QSet<QString> *AlbumCoverChoiceController::sImageExtensions = nullptr;

AlbumCoverChoiceController::AlbumCoverChoiceController(QWidget *parent) :
    QWidget(parent),
    app_(nullptr),
    cover_searcher_(nullptr),
    cover_fetcher_(nullptr),
    save_file_dialog_(nullptr),
    cover_from_url_dialog_(nullptr),
    cover_album_dir_(false),
    cover_filename_(CollectionSettingsPage::SaveCover_Hash),
    cover_overwrite_(false),
    cover_lowercase_(true),
    cover_replace_spaces_(true)
    {

  cover_from_file_ = new QAction(IconLoader::Load("document-open"), tr("Load cover from disk..."), this);
  cover_to_file_ = new QAction(IconLoader::Load("document-save"), tr("Save cover to disk..."), this);
  cover_from_url_ = new QAction(IconLoader::Load("download"), tr("Load cover from URL..."), this);
  search_for_cover_ = new QAction(IconLoader::Load("search"), tr("Search for album covers..."), this);
  unset_cover_ = new QAction(IconLoader::Load("list-remove"), tr("Unset cover"), this);
  show_cover_ = new QAction(IconLoader::Load("zoom-in"), tr("Show fullsize..."), this);

  search_cover_auto_ = new QAction(tr("Search automatically"), this);
  search_cover_auto_->setCheckable(true);
  search_cover_auto_->setChecked(false);

  separator_ = new QAction(this);
  separator_->setSeparator(true);

  ReloadSettings();

}

AlbumCoverChoiceController::~AlbumCoverChoiceController() {}

void AlbumCoverChoiceController::Init(Application *app) {

  app_ = app;

  cover_fetcher_ = new AlbumCoverFetcher(app_->cover_providers(), this);
  cover_searcher_ = new AlbumCoverSearcher(QIcon(":/pictures/cdcase.png"), app, this);
  cover_searcher_->Init(cover_fetcher_);

  connect(cover_fetcher_, SIGNAL(AlbumCoverFetched(const quint64, const QUrl&, const QImage&, CoverSearchStatistics)), this, SLOT(AlbumCoverFetched(const quint64, const QUrl&, const QImage&, CoverSearchStatistics)));

}

void AlbumCoverChoiceController::ReloadSettings() {

  QSettings s;
  s.beginGroup(CollectionSettingsPage::kSettingsGroup);
  cover_album_dir_ = s.value("cover_album_dir", false).toBool();
  cover_filename_ = CollectionSettingsPage::SaveCover(s.value("cover_filename", CollectionSettingsPage::SaveCover_Hash).toInt());
  cover_pattern_ = s.value("cover_pattern", "%albumartist-%album").toString();
  cover_overwrite_ = s.value("cover_overwrite", false).toBool();
  cover_lowercase_ = s.value("cover_lowercase", false).toBool();
  cover_replace_spaces_ = s.value("cover_replace_spaces", false).toBool();
  s.endGroup();

}

QList<QAction*> AlbumCoverChoiceController::GetAllActions() {
  return QList<QAction*>() << cover_from_file_ << cover_to_file_ << separator_ << cover_from_url_ << search_for_cover_ << unset_cover_ << separator_ << show_cover_;
}

QUrl AlbumCoverChoiceController::LoadCoverFromFile(Song *song) {

  QString cover_file = QFileDialog::getOpenFileName(this, tr("Load cover from disk"), GetInitialPathForFileDialog(*song, QString()), tr(kLoadImageFileFilter) + ";;" + tr(kAllFilesFilter));

  if (cover_file.isNull()) return QUrl();

  // Can we load the image?
  QImage image(cover_file);

  if (image.isNull()) {
    return QUrl();
  }
  else {
    QUrl cover_url(QUrl::fromLocalFile(cover_file));
    SaveCoverToSong(song, cover_url);
    return cover_url;
  }

}

void AlbumCoverChoiceController::SaveCoverToFileManual(const Song &song, const QImage &image) {

  QString initial_file_name = "/";

  if (!song.effective_albumartist().isEmpty()) {
    initial_file_name = initial_file_name + song.effective_albumartist();
  }
  initial_file_name = initial_file_name + "-" + (song.effective_album().isEmpty() ? tr("unknown") : song.effective_album()) + ".jpg";
  initial_file_name = initial_file_name.toLower();
  initial_file_name.replace(QRegExp("\\s"), "-");
  initial_file_name.remove(OrganiseFormat::kValidFatCharacters);

  QString save_filename = QFileDialog::getSaveFileName(this, tr("Save album cover"), GetInitialPathForFileDialog(song, initial_file_name), tr(kSaveImageFileFilter) + ";;" + tr(kAllFilesFilter));

  if (save_filename.isNull()) return;

  QString extension = save_filename.right(4);
  if (!extension.startsWith('.') || !QImageWriter::supportedImageFormats().contains(extension.right(3).toUtf8())) {
    save_filename.append(".jpg");
  }

  image.save(save_filename);

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

  if (!cover_from_url_dialog_) { cover_from_url_dialog_ = new CoverFromURLDialog(this); }

  QImage image = cover_from_url_dialog_->Exec();

  if (image.isNull()) {
    return QUrl();
  }
  else {
    QUrl cover_url = SaveCoverToFileAutomatic(song, QUrl(), image, true);
    if (cover_url.isEmpty()) return QUrl();
    SaveCoverToSong(song, cover_url);
    return cover_url;
  }

}

QUrl AlbumCoverChoiceController::SearchForCover(Song *song) {

  QString album = song->effective_album();
  album.remove(Song::kAlbumRemoveDisc);
  album.remove(Song::kAlbumRemoveMisc);

  // Get something sensible to stick in the search box
  QImage image = cover_searcher_->Exec(song->effective_albumartist(), album);

  if (image.isNull()) {
    return QUrl();
  }
  else {
    QUrl cover_url = SaveCoverToFileAutomatic(song, QUrl(), image, true);
    if (cover_url.isEmpty()) return QUrl();
    SaveCoverToSong(song, cover_url);
    return cover_url;
  }

}

QUrl AlbumCoverChoiceController::UnsetCover(Song *song) {

  QUrl cover_url(QUrl::fromLocalFile(Song::kManuallyUnsetCover));
  SaveCoverToSong(song, cover_url);

  return cover_url;

}

void AlbumCoverChoiceController::ShowCover(const Song &song) {

  QPixmap pixmap = AlbumCoverLoader::TryLoadPixmap(song.art_automatic(), song.art_manual(), song.url());
  if (pixmap.isNull()) return;
  ShowCover(song, pixmap);

}

void AlbumCoverChoiceController::ShowCover(const Song &song, const QImage &image) {

  if (song.art_manual().isLocalFile() || song.art_automatic().isLocalFile()) {
    QPixmap pixmap = AlbumCoverLoader::TryLoadPixmap(song.art_automatic(), song.art_manual(), song.url());
    if (!pixmap.isNull()) ShowCover(song, pixmap);
  }
  else if (!image.isNull()) ShowCover(song, QPixmap::fromImage(image));

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
  title_text += " (" + QString::number(label->pixmap()->width()) + "x" + QString::number(label->pixmap()->height()) + "px)";

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
    const int new_width = (double)desktop_width * 0.95;
    if (new_width < label->pixmap()->width()) {
      label->setPixmap(label->pixmap()->scaledToWidth(new_width, Qt::SmoothTransformation));
    }
  }
  else {
    const int new_height = (double)desktop_height * 0.85;
    if (new_height < label->pixmap()->height()) {
      label->setPixmap(label->pixmap()->scaledToHeight(new_height, Qt::SmoothTransformation));
    }
  }

  dialog->setWindowTitle(title_text);
  dialog->setFixedSize(label->pixmap()->size());
  dialog->show();

}

void AlbumCoverChoiceController::SearchCoverAutomatically(const Song &song) {

  qint64 id = cover_fetcher_->FetchAlbumCover(song.effective_albumartist(), song.effective_album(), true);

  cover_fetching_tasks_[id] = song;

}

void AlbumCoverChoiceController::AlbumCoverFetched(const quint64 id, const QUrl &cover_url, const QImage &image, const CoverSearchStatistics &statistics) {

  Q_UNUSED(statistics);

  Song song;
  if (cover_fetching_tasks_.contains(id)) {
    song = cover_fetching_tasks_.take(id);
  }

  if (!image.isNull()) {
    QUrl new_cover_url = SaveCoverToFileAutomatic(&song, cover_url, image, false);
    if (!new_cover_url.isEmpty()) SaveCoverToSong(&song, new_cover_url);
  }

  emit AutomaticCoverSearchDone();

}

void AlbumCoverChoiceController::SaveCoverToSong(Song *song, const QUrl &cover_url) {

  if (!song->is_valid()) return;

  song->set_art_manual(cover_url);

  if (song->id() != -1) {  // Update the backends.
    switch (song->source()) {
      case Song::Source_Collection:
        app_->collection_backend()->UpdateManualAlbumArtAsync(song->artist(), song->albumartist(), song->album(), cover_url);
        break;
      case Song::Source_LocalFile:
      case Song::Source_CDDA:
      case Song::Source_Device:
      case Song::Source_Stream:
      case Song::Source_Unknown:
        break;
      case Song::Source_Tidal:
      case Song::Source_Qobuz:
      case Song::Source_Subsonic:
        InternetService *service = app_->internet_services()->ServiceBySource(song->source());
        if (!service) break;
        if (service->artists_collection_backend())
          service->artists_collection_backend()->UpdateManualAlbumArtAsync(song->artist(), song->albumartist(), song->album(), cover_url);
        if (service->albums_collection_backend())
          service->albums_collection_backend()->UpdateManualAlbumArtAsync(song->artist(), song->albumartist(), song->album(), cover_url);
        if (service->songs_collection_backend())
          service->songs_collection_backend()->UpdateManualAlbumArtAsync(song->artist(), song->albumartist(), song->album(), cover_url);
        break;
    }

  }

  if (song->url() == app_->current_albumcover_loader()->last_song().url()) {
    app_->current_albumcover_loader()->LoadAlbumCover(*song);
  }

}

QUrl AlbumCoverChoiceController::SaveCoverToFileAutomatic(const Song *song, const QUrl &cover_url, const QImage &image, const bool overwrite) {

  return SaveCoverToFileAutomatic(song->source(), song->effective_albumartist(), song->effective_album(), song->album_id(), song->url().adjusted(QUrl::RemoveFilename).path(), cover_url, image, overwrite);

}

QUrl AlbumCoverChoiceController::SaveCoverToFileAutomatic(const Song::Source source, const QString &artist, const QString &album, const QString &album_id, const QString &album_dir, const QUrl &cover_url, const QImage &image, const bool overwrite) {

  QString filepath = app_->album_cover_loader()->CoverFilePath(source, artist, album, album_id, album_dir, cover_url);
  if (filepath.isEmpty()) return QUrl();

  QUrl new_cover_url(QUrl::fromLocalFile(filepath));

  // Don't overwrite when saving in album dir if the filename is set to pattern unless the "overwrite" is set.
  if (source == Song::Source_Collection && QFile::exists(filepath) && !cover_overwrite_ && !overwrite && cover_album_dir_ && cover_filename_ == CollectionSettingsPage::SaveCover_Pattern) {
    return new_cover_url;
  }

  if (!image.save(filepath, "JPG") && !QFile::exists(filepath)) return QUrl();

  return new_cover_url;

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
      SaveCoverToSong(song, url);
      return url;
    }
  }

  if (e->mimeData()->hasImage()) {
    QImage image = qvariant_cast<QImage>(e->mimeData()->imageData());
    if (!image.isNull()) {
      QUrl cover_url = SaveCoverToFileAutomatic(song, QUrl(), image, true);
      if (cover_url.isEmpty()) return QUrl();
      SaveCoverToSong(song, cover_url);
      return cover_url;
    }
  }

  return QUrl();

}
