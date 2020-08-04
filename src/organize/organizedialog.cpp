/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>
#include <functional>
#include <algorithm>

#include <QtGlobal>
#include <QtConcurrent>
#include <QAbstractItemModel>
#include <QDialog>
#include <QScreen>
#include <QWindow>
#include <QHash>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
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
#include <QFlags>
#include <QShowEvent>
#include <QCloseEvent>
#include <QSettings>

#include "core/closure.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "core/tagreaderclient.h"
#include "core/utilities.h"
#include "widgets/freespacebar.h"
#include "widgets/linetextedit.h"
#include "collection/collectionbackend.h"
#include "organize.h"
#include "organizeformat.h"
#include "organizedialog.h"
#include "organizeerrordialog.h"
#include "ui_organizedialog.h"

const char *OrganizeDialog::kDefaultFormat = "%albumartist/%album{ (Disc %disc)}/{%track - }{%albumartist - }%album{ (Disc %disc)} - %title.%extension";

const char *OrganizeDialog::kSettingsGroup = "OrganizeDialog";

OrganizeDialog::OrganizeDialog(TaskManager *task_manager, CollectionBackend *backend, QWidget *parentwindow, QWidget *parent)
    : QDialog(parent),
      parentwindow_(parentwindow),
      ui_(new Ui_OrganizeDialog),
      task_manager_(task_manager),
      backend_(backend),
      total_size_(0) {

  ui_->setupUi(this);

  setWindowFlags(windowFlags()|Qt::WindowMaximizeButtonHint);

  QPushButton *button_save = ui_->button_box->addButton("Save settings", QDialogButtonBox::ApplyRole);
  connect(button_save, SIGNAL(clicked()), SLOT(SaveSettings()));
  button_save->setIcon(IconLoader::Load("document-save"));
  ui_->button_box->button(QDialogButtonBox::RestoreDefaults)->setIcon(IconLoader::Load("edit-undo"));
  connect(ui_->button_box->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked()), SLOT(RestoreDefaults()));

  ui_->aftercopying->setItemIcon(1, IconLoader::Load("edit-delete"));

  // Valid tags
  QMap<QString, QString> tags;
  tags[tr("Title")] = "title";
  tags[tr("Album")] = "album";
  tags[tr("Artist")] = "artist";
  tags[tr("Artist's initial")] = "artistinitial";
  tags[tr("Album artist")] = "albumartist";
  tags[tr("Composer")] = "composer";
  tags[tr("Performer")] = "performer";
  tags[tr("Grouping")] = "grouping";
  tags[tr("Track")] = "track";
  tags[tr("Disc")] = "disc";
  tags[tr("Year")] = "year";
  tags[tr("Original year")] = "originalyear";
  tags[tr("Genre")] = "genre";
  tags[tr("Comment")] = "comment";
  tags[tr("Length")] = "length";
  tags[tr("Bitrate", "Refers to bitrate in file organize dialog.")] = "bitrate";
  tags[tr("Sample rate")] = "samplerate";
  tags[tr("Bit depth")] = "bitdepth";
  tags[tr("File extension")] = "extension";

  // Naming scheme input field
  new OrganizeFormat::SyntaxHighlighter(ui_->naming);

  connect(ui_->destination, SIGNAL(currentIndexChanged(int)), SLOT(UpdatePreviews()));
  connect(ui_->naming, SIGNAL(textChanged()), SLOT(UpdatePreviews()));
  connect(ui_->remove_problematic, SIGNAL(toggled(bool)), SLOT(UpdatePreviews()));
  connect(ui_->remove_non_fat, SIGNAL(toggled(bool)), SLOT(UpdatePreviews()));
  connect(ui_->remove_non_ascii, SIGNAL(toggled(bool)), SLOT(UpdatePreviews()));
  connect(ui_->allow_ascii_ext, SIGNAL(toggled(bool)), SLOT(UpdatePreviews()));
  connect(ui_->replace_spaces, SIGNAL(toggled(bool)), SLOT(UpdatePreviews()));
  connect(ui_->remove_non_ascii, SIGNAL(toggled(bool)), SLOT(AllowExtASCII(bool)));

  // Get the titles of the tags to put in the insert menu
  QStringList tag_titles = tags.keys();
  std::stable_sort(tag_titles.begin(), tag_titles.end());

  // Build the insert menu
  QMenu *tag_menu = new QMenu(this);
  for (const QString &title : tag_titles) {
    QAction *action = tag_menu->addAction(title);
    QString tag = tags[title];
    connect(action, &QAction::triggered, [this, tag]() { InsertTag(tag); } );
  }

  ui_->insert->setMenu(tag_menu);

}

OrganizeDialog::~OrganizeDialog() {
  delete ui_;
}

void OrganizeDialog::SetDestinationModel(QAbstractItemModel *model, bool devices) {

  ui_->destination->setModel(model);

  ui_->eject_after->setVisible(devices);

}

void OrganizeDialog::showEvent(QShowEvent*) {

  LoadGeometry();
  LoadSettings();

}

void OrganizeDialog::closeEvent(QCloseEvent*) {

  SaveGeometry();

}

void OrganizeDialog::accept() {

  SaveGeometry();
  SaveSettings();

  const QModelIndex destination = ui_->destination->model()->index(ui_->destination->currentIndex(), 0);
  std::shared_ptr<MusicStorage> storage = destination.data(MusicStorage::Role_StorageForceConnect).value<std::shared_ptr<MusicStorage>>();

  if (!storage) return;

  // It deletes itself when it's finished.
  const bool copy = ui_->aftercopying->currentIndex() == 0;
  Organize *organize = new Organize(task_manager_, storage, format_, copy, ui_->overwrite->isChecked(), ui_->mark_as_listened->isChecked(), ui_->albumcover->isChecked(), new_songs_info_, ui_->eject_after->isChecked(), playlist_);
  connect(organize, SIGNAL(Finished(QStringList, QStringList)), SLOT(OrganizeFinished(QStringList, QStringList)));
  connect(organize, SIGNAL(FileCopied(int)), this, SIGNAL(FileCopied(int)));
  if (backend_)
    connect(organize, SIGNAL(SongPathChanged(Song, QFileInfo)), backend_, SLOT(SongPathChanged(Song, QFileInfo)));

  organize->Start();

  QDialog::accept();

}

void OrganizeDialog::reject() {

  SaveGeometry();
  QDialog::reject();

}

void OrganizeDialog::LoadGeometry() {

  if (parentwindow_) {

    QSettings s;
    s.beginGroup(kSettingsGroup);
    if (s.contains("geometry")) {
      restoreGeometry(s.value("geometry").toByteArray());
    }
    s.endGroup();

  // Center the window on the same screen as the parentwindow.
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QScreen *screen = parentwindow_->screen();
#else
    QScreen *screen = (parentwindow_->window() && parentwindow_->window()->windowHandle() ? parentwindow_->window()->windowHandle()->screen() : nullptr);
#endif
    if (screen) {
      const QRect sr = screen->availableGeometry();
      const QRect wr({}, size().boundedTo(sr.size()));
      resize(wr.size());
      move(sr.center() - wr.center());
    }
  }

}

void OrganizeDialog::SaveGeometry() {

  if (parentwindow_) {
    QSettings s;
    s.beginGroup(kSettingsGroup);
    s.setValue("geometry", saveGeometry());
    s.endGroup();
  }

}

void OrganizeDialog::RestoreDefaults() {

  ui_->naming->setPlainText(kDefaultFormat);
  ui_->remove_problematic->setChecked(true);
  ui_->remove_non_fat->setChecked(false);
  ui_->remove_non_ascii->setChecked(false);
  ui_->allow_ascii_ext->setChecked(false);
  ui_->replace_spaces->setChecked(true);
  ui_->overwrite->setChecked(false);
  ui_->mark_as_listened->setChecked(false);
  ui_->albumcover->setChecked(true);
  ui_->eject_after->setChecked(false);

  SaveSettings();

}

void OrganizeDialog::LoadSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  ui_->naming->setPlainText(s.value("format", kDefaultFormat).toString());
  ui_->remove_problematic->setChecked(s.value("remove_problematic", true).toBool());
  ui_->remove_non_fat->setChecked(s.value("remove_non_fat", false).toBool());
  ui_->remove_non_ascii->setChecked(s.value("remove_non_ascii", false).toBool());
  ui_->allow_ascii_ext->setChecked(s.value("allow_ascii_ext", false).toBool());
  ui_->replace_spaces->setChecked(s.value("replace_spaces", true).toBool());
  ui_->overwrite->setChecked(s.value("overwrite", false).toBool());
  ui_->albumcover->setChecked(s.value("albumcover", true).toBool());
  ui_->mark_as_listened->setChecked(s.value("mark_as_listened", false).toBool());
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

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("format", ui_->naming->toPlainText());
  s.setValue("remove_problematic", ui_->remove_problematic->isChecked());
  s.setValue("remove_non_fat", ui_->remove_non_fat->isChecked());
  s.setValue("remove_non_ascii", ui_->remove_non_ascii->isChecked());
  s.setValue("allow_ascii_ext", ui_->allow_ascii_ext->isChecked());
  s.setValue("replace_spaces", ui_->replace_spaces->isChecked());
  s.setValue("overwrite", ui_->overwrite->isChecked());
  s.setValue("mark_as_listened", ui_->overwrite->isChecked());
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

  return songs_.count();

}

bool OrganizeDialog::SetUrls(const QList<QUrl> &urls) {

  QStringList filenames;

  // Only add file:// URLs
  for (const QUrl &url : urls) {
    if (url.scheme() == "file") {
      filenames << url.toLocalFile();
    }
  }

  return SetFilenames(filenames);

}

bool OrganizeDialog::SetFilenames(const QStringList &filenames) {

  songs_future_ = QtConcurrent::run(std::bind(&OrganizeDialog::LoadSongsBlocking, this, filenames));
  NewClosure(songs_future_, [=]() { SetSongs(songs_future_.result()); });

  SetLoadingSongs(true);
  return true;

}

void OrganizeDialog::SetLoadingSongs(bool loading) {

  if (loading) {
    ui_->preview_stack->setCurrentWidget(ui_->loading_page);
    ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
  }
  else {
    ui_->preview_stack->setCurrentWidget(ui_->preview_page);
    // The Ok button is enabled by UpdatePreviews
  }

}

SongList OrganizeDialog::LoadSongsBlocking(const QStringList &filenames) {

  SongList songs;
  Song song;

  QStringList filenames_copy = filenames;
  while (!filenames_copy.isEmpty()) {
    const QString filename = filenames_copy.takeFirst();

    // If it's a directory, add all the files inside.
    if (QFileInfo(filename).isDir()) {
      const QDir dir(filename);
      for (const QString &entry : dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable)) {
        filenames_copy << dir.filePath(entry);
      }
      continue;
    }

    TagReaderClient::Instance()->ReadFileBlocking(filename, &song);
    if (song.is_valid()) songs << song;
  }

  return songs;

}

void OrganizeDialog::SetCopy(bool copy) {
    ui_->aftercopying->setCurrentIndex(copy ? 0 : 1);
}

void OrganizeDialog::SetPlaylist(const QString &playlist)
{
    playlist_ = playlist;
}

void OrganizeDialog::InsertTag(const QString &tag) {
  ui_->naming->insertPlainText("%" + tag);
}

Organize::NewSongInfoList OrganizeDialog::ComputeNewSongsFilenames(const SongList &songs, const OrganizeFormat &format) {

  // Check if we will have multiple files with the same name.
  // If so, they will erase each other if the overwrite flag is set.
  // Better to rename them: e.g. foo.bar -> foo(2).bar
  QHash<QString, int> filenames;
  Organize::NewSongInfoList new_songs_info;

  for (const Song &song : songs) {
    QString new_filename = format.GetFilenameForSong(song);
    if (filenames.contains(new_filename)) {
      QString song_number = QString::number(++filenames[new_filename]);
      new_filename = Utilities::PathWithoutFilenameExtension(new_filename) + "(" + song_number + ")." + QFileInfo(new_filename).suffix();
    }
    filenames.insert(new_filename, 1);
    new_songs_info << Organize::NewSongInfo(song, new_filename);
  }
  return new_songs_info;

}

void OrganizeDialog::UpdatePreviews() {

  if (songs_future_.isRunning()) {
    return;
  }

  const QModelIndex destination = ui_->destination->model()->index(ui_->destination->currentIndex(), 0);
  std::shared_ptr<MusicStorage> storage;
  bool has_local_destination = false;

  if (destination.isValid()) {
    storage = destination.data(MusicStorage::Role_Storage).value<std::shared_ptr<MusicStorage>>();
    if (storage) {
      has_local_destination = !storage->LocalPath().isEmpty();
    }
  }

  // Update the free space bar
  quint64 capacity = destination.data(MusicStorage::Role_Capacity).toLongLong();
  quint64 free = destination.data(MusicStorage::Role_FreeSpace).toLongLong();

  if (!capacity) {
    ui_->free_space->hide();
  }
  else {
    ui_->free_space->show();
    ui_->free_space->set_free_bytes(free);
    ui_->free_space->set_total_bytes(capacity);
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

  ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(ok);
  if (!format_valid) return;

  new_songs_info_ = ComputeNewSongsFilenames(songs_, format_);

  // Update the previews
  ui_->preview->clear();
  ui_->groupbox_preview->setVisible(has_local_destination);
  ui_->groupbox_naming->setVisible(has_local_destination);
  if (has_local_destination) {
    for (const Organize::NewSongInfo &song_info : new_songs_info_) {
      QString filename = storage->LocalPath() + "/" + song_info.new_filename_;
      ui_->preview->addItem(QDir::toNativeSeparators(filename));
    }
  }

}

QSize OrganizeDialog::sizeHint() const { return QSize(650, 0); }

void OrganizeDialog::OrganizeFinished(const QStringList files_with_errors, const QStringList log) {
  if (files_with_errors.isEmpty()) return;

  error_dialog_.reset(new OrganizeErrorDialog);
  error_dialog_->Show(OrganizeErrorDialog::Type_Copy, files_with_errors, log);
}

void OrganizeDialog::AllowExtASCII(bool checked) {
  ui_->allow_ascii_ext->setEnabled(checked);
}
