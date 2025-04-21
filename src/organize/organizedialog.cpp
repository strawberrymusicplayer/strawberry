/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>
#include <functional>
#include <memory>

#include <QtGlobal>
#include <QGuiApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QAbstractItemModel>
#include <QDialog>
#include <QScreen>
#include <QHash>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QAction>
#include <QMenu>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QShowEvent>
#include <QCloseEvent>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "core/settings.h"
#include "utilities/strutils.h"
#include "utilities/screenutils.h"
#include "widgets/freespacebar.h"
#include "widgets/linetextedit.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionbackend.h"
#include "organize.h"
#include "organizeformat.h"
#include "organizesyntaxhighlighter.h"
#include "organizedialog.h"
#include "organizeerrordialog.h"
#include "ui_organizedialog.h"
#include "transcoder/transcoder.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "OrganizeDialog";
constexpr char kDefaultFormat[] = "%albumartist/%album{ (Disc %disc)}/{%track - }{%albumartist - }%album{ (Disc %disc)} - %title.%extension";
}

OrganizeDialog::OrganizeDialog(const SharedPtr<TaskManager> task_manager,
                               const SharedPtr<TagReaderClient> tagreader_client,
                               const SharedPtr<CollectionBackend> collection_backend,
                               QWidget *parentwindow,
                               QWidget *parent)
    : QDialog(parent),
      parentwindow_(parentwindow),
      ui_(new Ui_OrganizeDialog),
      task_manager_(task_manager),
      tagreader_client_(tagreader_client),
      collection_backend_(collection_backend),
      total_size_(0),
      devices_(false) {

  ui_->setupUi(this);

  setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

  QPushButton *button_save = ui_->button_box->addButton(u"Save settings"_s, QDialogButtonBox::ApplyRole);
  QObject::connect(button_save, &QPushButton::clicked, this, &OrganizeDialog::SaveSettings);
  button_save->setIcon(IconLoader::Load(u"document-save"_s));
  ui_->button_box->button(QDialogButtonBox::RestoreDefaults)->setIcon(IconLoader::Load(u"edit-undo"_s));
  QObject::connect(ui_->button_box->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &OrganizeDialog::RestoreDefaults);

  ui_->aftercopying->setItemIcon(1, IconLoader::Load(u"edit-delete"_s));

  // Valid tags
  QMap<QString, QString> tags;
  tags[tr("Title")] = u"title"_s;
  tags[tr("Album")] = u"album"_s;
  tags[tr("Artist")] = u"artist"_s;
  tags[tr("Artist's initial")] = u"artistinitial"_s;
  tags[tr("Album artist")] = u"albumartist"_s;
  tags[tr("Composer")] = u"composer"_s;
  tags[tr("Performer")] = u"performer"_s;
  tags[tr("Grouping")] = u"grouping"_s;
  tags[tr("Track")] = u"track"_s;
  tags[tr("Disc")] = u"disc"_s;
  tags[tr("Year")] = u"year"_s;
  tags[tr("Original year")] = u"originalyear"_s;
  tags[tr("Genre")] = u"genre"_s;
  tags[tr("Comment")] = u"comment"_s;
  tags[tr("Length")] = u"length"_s;
  tags[tr("Bitrate", "Refers to bitrate in file organize dialog.")] = u"bitrate"_s;
  tags[tr("Sample rate")] = u"samplerate"_s;
  tags[tr("Bit depth")] = u"bitdepth"_s;
  tags[tr("File extension")] = u"extension"_s;

  // Naming scheme input field
  new OrganizeSyntaxHighlighter(ui_->naming);

  QObject::connect(ui_->destination, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->naming, &LineTextEdit::textChanged, this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->remove_problematic, &QCheckBox::toggled, this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->remove_non_fat, &QCheckBox::toggled, this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->remove_non_ascii, &QCheckBox::toggled, this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->allow_ascii_ext, &QCheckBox::toggled, this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->replace_spaces, &QCheckBox::toggled, this, &OrganizeDialog::UpdatePreviews);
  QObject::connect(ui_->remove_non_ascii, &QCheckBox::toggled, this, &OrganizeDialog::AllowExtASCII);

  // Get the titles of the tags to put in the insert menu
  QStringList tag_titles = tags.keys();
  std::stable_sort(tag_titles.begin(), tag_titles.end());

  // Build the insert menu
  QMenu *tag_menu = new QMenu(this);
  for (const QString &title : std::as_const(tag_titles)) {
    QAction *action = tag_menu->addAction(title);
    QString tag = tags[title];
    QObject::connect(action, &QAction::triggered, this, [this, tag]() { InsertTag(tag); });
  }

  ui_->insert->setMenu(tag_menu);

}

OrganizeDialog::~OrganizeDialog() {
  delete ui_;
}

void OrganizeDialog::SetDestinationModel(QAbstractItemModel *model, const bool devices) {

  ui_->destination->setModel(model);

  ui_->eject_after->setVisible(devices);

  devices_ = devices;

}

void OrganizeDialog::showEvent(QShowEvent *e) {

  Q_UNUSED(e)

  LoadGeometry();
  LoadSettings();

}

void OrganizeDialog::closeEvent(QCloseEvent *e) {

  Q_UNUSED(e)

  if (!devices_) SaveGeometry();

}

void OrganizeDialog::accept() {

  SaveGeometry();

  const QModelIndex destination = ui_->destination->model()->index(ui_->destination->currentIndex(), 0);
  SharedPtr<MusicStorage> storage = destination.data(MusicStorage::Role_StorageForceConnect).value<SharedPtr<MusicStorage>>();

  if (!storage) return;

  // It deletes itself when it's finished.
  const bool copy = ui_->aftercopying->currentIndex() == 0;
  Organize *organize = new Organize(task_manager_, tagreader_client_, storage, format_, copy, ui_->overwrite->isChecked(), ui_->albumcover->isChecked(), new_songs_info_, ui_->eject_after->isChecked(), playlist_);
  QObject::connect(organize, &Organize::Finished, this, &OrganizeDialog::OrganizeFinished);
  QObject::connect(organize, &Organize::FileCopied, this, &OrganizeDialog::FileCopied);
  if (collection_backend_) {
    QObject::connect(organize, &Organize::SongPathChanged, &*collection_backend_, &CollectionBackend::SongPathChanged);
  }

  organize->Start();

  QDialog::accept();

}

void OrganizeDialog::reject() {

  SaveGeometry();
  QDialog::reject();

}

void OrganizeDialog::LoadGeometry() {

  if (devices_) {
    AdjustSize();
  }
  else {
    Settings s;
    s.beginGroup(kSettingsGroup);
    if (s.contains("geometry")) {
      restoreGeometry(s.value("geometry").toByteArray());
    }
    s.endGroup();
  }

  if (parentwindow_) {
    // Center the window on the same screen as the parentwindow.
    Utilities::CenterWidgetOnScreen(Utilities::GetScreen(parentwindow_), this);
  }

}

void OrganizeDialog::SaveGeometry() {

  if (parentwindow_) {
    Settings s;
    s.beginGroup(kSettingsGroup);
    s.setValue("geometry", saveGeometry());
    s.endGroup();
  }

}

void OrganizeDialog::AdjustSize() {

  QScreen *screen = Utilities::GetScreen(this);
  int max_width = 0;
  int max_height = 0;
  if (screen) {
    max_width = static_cast<int>(static_cast<float>(screen->geometry().size().width()) / static_cast<float>(0.5));
    max_height = static_cast<int>(static_cast<float>(screen->geometry().size().height()) / static_cast<float>(1.5));
  }

  int min_width = 0;
  int min_height = 0;
  if (ui_->preview->isVisible()) {
    int h = ui_->layout_copying->sizeHint().height() +
            ui_->button_box->sizeHint().height() +
            ui_->eject_after->sizeHint().height() +
            ui_->free_space->sizeHint().height() +
            ui_->groupbox_naming->sizeHint().height();
    if (ui_->preview->count() > 0) h += ui_->preview->sizeHintForRow(0) * ui_->preview->count();
    else h += ui_->loading_page->sizeHint().height();
    min_width = std::min(ui_->preview->sizeHintForColumn(0), max_width);
    min_height = std::min(h, max_height);
  }

  setMinimumSize(min_width, min_height);
  adjustSize();

}

void OrganizeDialog::RestoreDefaults() {

  ui_->naming->setPlainText(QLatin1String(kDefaultFormat));
  ui_->remove_problematic->setChecked(true);
  ui_->remove_non_fat->setChecked(false);
  ui_->remove_non_ascii->setChecked(false);
  ui_->allow_ascii_ext->setChecked(false);
  ui_->replace_spaces->setChecked(true);
  ui_->overwrite->setChecked(false);
  ui_->albumcover->setChecked(true);
  ui_->eject_after->setChecked(false);

}

void OrganizeDialog::LoadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->naming->setPlainText(s.value("format", QLatin1String(kDefaultFormat)).toString());
  ui_->remove_problematic->setChecked(s.value("remove_problematic", true).toBool());
  ui_->remove_non_fat->setChecked(s.value("remove_non_fat", false).toBool());
  ui_->remove_non_ascii->setChecked(s.value("remove_non_ascii", false).toBool());
  ui_->allow_ascii_ext->setChecked(s.value("allow_ascii_ext", false).toBool());
  ui_->replace_spaces->setChecked(s.value("replace_spaces", true).toBool());
  ui_->overwrite->setChecked(s.value("overwrite", false).toBool());
  ui_->albumcover->setChecked(s.value("albumcover", true).toBool());
  ui_->eject_after->setChecked(s.value("eject_after", false).toBool());

  QString destination = s.value("destination").toString();
  int index = ui_->destination->findText(destination);
  if (index != -1 && !destination.isEmpty()) {
    ui_->destination->setCurrentIndex(index);
  }

  s.endGroup();

  AllowExtASCII(ui_->remove_non_ascii->isChecked());

}

void OrganizeDialog::SaveSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("format", ui_->naming->toPlainText());
  s.setValue("remove_problematic", ui_->remove_problematic->isChecked());
  s.setValue("remove_non_fat", ui_->remove_non_fat->isChecked());
  s.setValue("remove_non_ascii", ui_->remove_non_ascii->isChecked());
  s.setValue("allow_ascii_ext", ui_->allow_ascii_ext->isChecked());
  s.setValue("replace_spaces", ui_->replace_spaces->isChecked());
  s.setValue("overwrite", ui_->overwrite->isChecked());
  s.setValue("albumcover", ui_->albumcover->isChecked());
  s.setValue("destination", ui_->destination->currentText());
  s.setValue("eject_after", ui_->eject_after->isChecked());
  s.endGroup();

}

bool OrganizeDialog::SetSongs(const SongList &songs) {

  total_size_ = 0;
  songs_.clear();

  for (const Song &song : songs) {
    if (!song.url().isLocalFile()) {
      continue;
    }

    if (song.filesize() > 0) total_size_ += song.filesize();

    songs_ << song;
  }

  ui_->free_space->set_additional_bytes(total_size_);
  UpdatePreviews();
  SetLoadingSongs(false);

  if (songs_future_.isRunning()) {
    songs_future_.cancel();
  }
  songs_future_ = QFuture<SongList>();

  return !songs_.isEmpty();

}

bool OrganizeDialog::SetUrls(const QList<QUrl> &urls) {

  QStringList filenames;
  for (const QUrl &url : urls) {
    if (url.isLocalFile()) {
      filenames << url.toLocalFile();
    }
  }

  return SetFilenames(filenames);

}

bool OrganizeDialog::SetFilenames(const QStringList &filenames) {

  songs_future_ = QtConcurrent::run(&OrganizeDialog::LoadSongsBlocking, this, filenames);
  QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>();
  QObject::connect(watcher, &QFutureWatcher<SongList>::finished, this, [this, watcher]() {
    SetSongs(watcher->result());
    watcher->deleteLater();
  });
  watcher->setFuture(songs_future_);

  SetLoadingSongs(true);
  return true;

}

void OrganizeDialog::SetLoadingSongs(const bool loading) {

  if (loading) {
    ui_->preview_stack->setCurrentWidget(ui_->loading_page);
    ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
  }
  else {
    ui_->preview_stack->setCurrentWidget(ui_->preview_page);
    // The Ok button is enabled by UpdatePreviews
  }

}

SongList OrganizeDialog::LoadSongsBlocking(const QStringList &filenames) const {

  SongList songs;

  QStringList filenames_copy = filenames;
  while (!filenames_copy.isEmpty()) {
    const QString filename = filenames_copy.takeFirst();

    // If it's a directory, add all the files inside.
    if (QFileInfo(filename).isDir()) {
      const QDir dir(filename);
      const QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
      for (const QString &entry : entries) {
        filenames_copy << dir.filePath(entry);
      }
      continue;
    }

    Song song;
    const TagReaderResult result = tagreader_client_->ReadFileBlocking(filename, &song);
    if (result.success() && song.is_valid()) {
      songs << song;
    }
    else {
      qLog(Error) << "Could not read file" << filename << result.error_string();
    }
  }

  return songs;

}

void OrganizeDialog::SetCopy(const bool copy) {
  ui_->aftercopying->setCurrentIndex(copy ? 0 : 1);
}

void OrganizeDialog::SetPlaylist(const QString &playlist) {
  playlist_ = playlist;
}

void OrganizeDialog::InsertTag(const QString &tag) {
  ui_->naming->insertPlainText(QLatin1Char('%') + tag);
}

Organize::NewSongInfoList OrganizeDialog::ComputeNewSongsFilenames(const SongList &songs, const OrganizeFormat &format, const QString &extension) {

  // Check if we will have multiple files with the same name.
  // If so, they will erase each other if the overwrite flag is set.
  // Better to rename them: e.g. foo.bar -> foo(2).bar
  QHash<QString, int> filenames;
  Organize::NewSongInfoList new_songs_info;
  new_songs_info.reserve(songs.count());
  for (const Song &song : songs) {
    OrganizeFormat::GetFilenameForSongResult result = format.GetFilenameForSong(song, extension);
    if (result.filename.isEmpty()) {
      return Organize::NewSongInfoList();
    }
    if (result.unique_filename) {
      if (filenames.contains(result.filename)) {
        QString song_number = QString::number(++filenames[result.filename]);
        result.filename = Utilities::PathWithoutFilenameExtension(result.filename) + u"("_s + song_number + u")."_s + QFileInfo(result.filename).suffix();
      }
      else {
        filenames.insert(result.filename, 1);
      }
    }
    new_songs_info << Organize::NewSongInfo(song, result.filename, result.unique_filename);
  }

  return new_songs_info;

}

void OrganizeDialog::UpdatePreviews() {

  if (songs_future_.isRunning()) {
    return;
  }

  const QModelIndex destination = ui_->destination->model()->index(ui_->destination->currentIndex(), 0);
  SharedPtr<MusicStorage> storage;
  bool has_local_destination = false;

  if (destination.isValid()) {
    storage = destination.data(MusicStorage::Role_Storage).value<SharedPtr<MusicStorage>>();
    if (storage) {
      has_local_destination = !storage->LocalPath().isEmpty();
    }
  }

  // Update the free space bar
  quint64 capacity = destination.data(MusicStorage::Role_Capacity).toULongLong();
  quint64 free = destination.data(MusicStorage::Role_FreeSpace).toULongLong();

  if (capacity > 0) {
    ui_->free_space->show();
    ui_->free_space->set_free_bytes(free);
    ui_->free_space->set_total_bytes(capacity);
  }
  else {
    ui_->free_space->hide();
  }

  // Update the format object
  format_.set_format(ui_->naming->toPlainText());
  format_.set_remove_problematic(ui_->remove_problematic->isChecked());
  format_.set_remove_non_fat(ui_->remove_non_fat->isChecked());
  format_.set_remove_non_ascii(ui_->remove_non_ascii->isChecked());
  format_.set_allow_ascii_ext(ui_->allow_ascii_ext->isChecked());
  format_.set_replace_spaces(ui_->replace_spaces->isChecked());

  const bool format_valid = !has_local_destination || format_.IsValid();

  // Are we going to enable the ok button?
  bool ok = format_valid && !songs_.isEmpty();
  if (capacity != 0 && total_size_ > free) ok = false;

  if (ok) {
    QString extension;
    if (storage && storage->GetTranscodeMode() == MusicStorage::TranscodeMode::Transcode_Always) {
      const Song::FileType format = storage->GetTranscodeFormat();
      TranscoderPreset preset = Transcoder::PresetForFileType(format);
      extension = preset.extension_;
    }
    new_songs_info_ = ComputeNewSongsFilenames(songs_, format_, extension);
    if (new_songs_info_.isEmpty()) {
      ok = false;
    }
  }
  else {
    new_songs_info_.clear();
  }

  // Update the previews
  ui_->preview->clear();
  ui_->groupbox_preview->setVisible(has_local_destination);
  ui_->groupbox_naming->setVisible(has_local_destination);
  if (has_local_destination) {
    for (const Organize::NewSongInfo &song_info : std::as_const(new_songs_info_)) {
      QString filename = storage->LocalPath() + QLatin1Char('/') + song_info.new_filename_;
      QListWidgetItem *item = new QListWidgetItem(song_info.unique_filename_ ? IconLoader::Load(u"dialog-ok-apply"_s) : IconLoader::Load(u"dialog-warning"_s), QDir::toNativeSeparators(filename), ui_->preview);
      ui_->preview->addItem(item);
      if (!song_info.unique_filename_) {
        ok = false;
      }
    }
  }

  if (devices_) {
    AdjustSize();
  }

  ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(ok);

}

void OrganizeDialog::OrganizeFinished(const QStringList &files_with_errors, const QStringList &log) {

  if (files_with_errors.isEmpty()) return;

  error_dialog_ = make_unique<OrganizeErrorDialog>();
  error_dialog_->Show(OrganizeErrorDialog::OperationType::Copy, files_with_errors, log);

}

void OrganizeDialog::AllowExtASCII(const bool checked) {
  ui_->allow_ascii_ext->setEnabled(checked);
}
