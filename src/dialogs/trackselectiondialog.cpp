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

#include <utility>

#include <QtConcurrentRun>
#include <QWidget>
#include <QDialog>
#include <QTreeWidgetItem>
#include <QAbstractItemModel>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QPalette>
#include <QFont>
#include <QKeySequence>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>

#include "core/iconloader.h"
#include "core/logging.h"
#include "tagreader/tagreaderclient.h"
#include "widgets/busyindicator.h"
#include "trackselectiondialog.h"
#include "ui_trackselectiondialog.h"

TrackSelectionDialog::TrackSelectionDialog(const SharedPtr<TagReaderClient> tagreader_client, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_TrackSelectionDialog),
      tagreader_client_(tagreader_client),
      save_on_close_(false) {

  // Setup dialog window
  ui_->setupUi(this);

  QObject::connect(ui_->song_list, &QListWidget::currentRowChanged, this, &TrackSelectionDialog::UpdateStack);
  QObject::connect(ui_->results, &QTreeWidget::currentItemChanged, this, &TrackSelectionDialog::ResultSelected);

  ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);
  SetLoading(QString());

  // Add the next/previous buttons
  previous_button_ = new QPushButton(IconLoader::Load(QStringLiteral("go-previous")), tr("Previous"), this);
  next_button_ = new QPushButton(IconLoader::Load(QStringLiteral("go-next")), tr("Next"), this);
  ui_->button_box->addButton(previous_button_, QDialogButtonBox::ResetRole);
  ui_->button_box->addButton(next_button_, QDialogButtonBox::ResetRole);

  QObject::connect(previous_button_, &QPushButton::clicked, this, &TrackSelectionDialog::PreviousSong);
  QObject::connect(next_button_, &QPushButton::clicked, this, &TrackSelectionDialog::NextSong);

  // Set some shortcuts for the buttons
  new QShortcut(QKeySequence::Back, previous_button_, SLOT(click()));
  new QShortcut(QKeySequence::Forward, next_button_, SLOT(click()));
  new QShortcut(QKeySequence::MoveToPreviousPage, previous_button_, SLOT(click()));
  new QShortcut(QKeySequence::MoveToNextPage, next_button_, SLOT(click()));

  // Resize columns
  ui_->results->setColumnWidth(0, 50);   // Track column
  ui_->results->setColumnWidth(1, 50);   // Year column
  ui_->results->setColumnWidth(2, 160);  // Title column
  ui_->results->setColumnWidth(3, 160);  // Artist column
  ui_->results->setColumnWidth(4, 160);  // Album column

}

TrackSelectionDialog::~TrackSelectionDialog() {
  delete ui_;
}

void TrackSelectionDialog::Init(const SongList &songs) {

  ui_->song_list->clear();
  ui_->stack->setCurrentWidget(ui_->loading_page);
  data_.clear();

  for (const Song &song : songs) {
    Data tag_data;
    tag_data.original_song_ = song;
    data_ << tag_data;

    QListWidgetItem *item = new QListWidgetItem(ui_->song_list);
    item->setText(QFileInfo(song.url().toLocalFile()).fileName());
    item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
  }

  const bool multiple = songs.count() > 1;
  ui_->song_list->setVisible(multiple);
  next_button_->setEnabled(multiple);
  previous_button_->setEnabled(multiple);

  ui_->song_list->setCurrentRow(0);

}

void TrackSelectionDialog::FetchTagProgress(const Song &original_song, const QString &progress) {

  // Find the item with this filename
  int row = -1;
  for (int i = 0; i < data_.count(); ++i) {
    if (data_[i].original_song_.url() == original_song.url()) {
      row = i;
      break;
    }
  }

  if (row == -1) return;

  data_[row].progress_string_ = progress;

  // If it's the current item, update the display
  if (ui_->song_list->currentIndex().row() == row) {
    UpdateStack();
  }

}

void TrackSelectionDialog::FetchTagFinished(const Song &original_song, const SongList &songs_guessed) {

  // Find the item with this filename
  int row = -1;
  for (int i = 0; i < data_.count(); ++i) {
    if (data_[i].original_song_.url() == original_song.url()) {
      row = i;
      break;
    }
  }

  if (row == -1) return;

  // Set the color back to black
  ui_->song_list->item(row)->setForeground(palette().text());

  // Add the results to the list
  data_[row].pending_ = false;
  data_[row].results_ = songs_guessed;

  // If it's the current item, update the display
  if (ui_->song_list->currentIndex().row() == row) {
    UpdateStack();
  }

}

void TrackSelectionDialog::UpdateStack() {

  const int row = ui_->song_list->currentRow();
  if (row < 0 || row >= data_.count()) return;

  const Data tag_data = data_.value(row);

  if (tag_data.pending_) {
    ui_->stack->setCurrentWidget(ui_->loading_page);
    ui_->progress->set_text(tag_data.progress_string_ + QStringLiteral("..."));
    return;
  }
  if (tag_data.results_.isEmpty()) {
    ui_->stack->setCurrentWidget(ui_->error_page);
    return;
  }
  ui_->stack->setCurrentWidget(ui_->results_page);

  // Clear tree widget
  ui_->results->clear();

  // Put the original tags at the top
  AddDivider(tr("Original tags"), ui_->results);
  AddSong(tag_data.original_song_, -1, ui_->results);

  // Fill tree view with songs
  AddDivider(tr("Suggested tags"), ui_->results);

  int song_index = 0;
  for (const Song &song : tag_data.results_) {
    AddSong(song, song_index++, ui_->results);
  }

  // Find the item that was selected last time
  for (int i = 0; i < ui_->results->model()->rowCount(); ++i) {
    const QModelIndex index = ui_->results->model()->index(i, 0);
    const QVariant id = index.data(Qt::UserRole);
    if (!id.isNull() && id.toInt() == tag_data.selected_result_) {
      ui_->results->setCurrentIndex(index);
      break;
    }
  }
}

void TrackSelectionDialog::AddDivider(const QString &text, QTreeWidget *parent) const {

  QTreeWidgetItem *item = new QTreeWidgetItem(parent);
  item->setFirstColumnSpanned(true);
  item->setText(0, text);
  item->setFlags(Qt::NoItemFlags);
  item->setForeground(0, palette().color(QPalette::Disabled, QPalette::Text));

  QFont bold_font(font());
  bold_font.setBold(true);
  item->setFont(0, bold_font);

}

void TrackSelectionDialog::AddSong(const Song &song, int result_index, QTreeWidget *parent) {

  QStringList values;
  values << ((song.track() > 0) ? QString::number(song.track()) : QString()) << ((song.year() > 0) ? QString::number(song.year()) : QString()) << song.title() << song.artist() << song.album();

  QTreeWidgetItem *item = new QTreeWidgetItem(parent, values);
  item->setData(0, Qt::UserRole, result_index);
  item->setData(0, Qt::TextAlignmentRole, Qt::AlignCenter);

}

void TrackSelectionDialog::ResultSelected() {

  if (!ui_->results->currentItem()) return;

  const int song_row = ui_->song_list->currentRow();
  if (song_row == -1) return;

  const int result_index = ui_->results->currentItem()->data(0, Qt::UserRole).toInt();
  data_[song_row].selected_result_ = result_index;

}

void TrackSelectionDialog::SetLoading(const QString &message) {

  const bool loading = !message.isEmpty();

  ui_->button_box->setEnabled(!loading);
  ui_->splitter->setEnabled(!loading);
  ui_->loading_label->setVisible(loading);
  ui_->loading_label->set_text(message);

}

void TrackSelectionDialog::SaveData(const QList<Data> &_data) const {

  for (int i = 0; i < _data.count(); ++i) {
    const Data &ref = _data[i];
    if (ref.pending_ || ref.results_.isEmpty() || ref.selected_result_ == -1) {
      continue;
    }

    const Song &new_metadata = ref.results_[ref.selected_result_];

    Song copy(ref.original_song_);
    copy.set_title(new_metadata.title());
    copy.set_artist(new_metadata.artist());
    copy.set_album(new_metadata.album());
    copy.set_track(new_metadata.track());
    copy.set_year(new_metadata.year());

    const TagReaderResult result = tagreader_client_->WriteFileBlocking(copy.url().toLocalFile(), copy, TagReaderClient::SaveOption::Tags, SaveTagCoverData());
    if (!result.success()) {
      qLog(Error) << "Failed to write new auto-tags to" << copy.url().toLocalFile() << result.error_string();
    }
  }

}

void TrackSelectionDialog::accept() {

  if (save_on_close_) {
    SetLoading(tr("Saving tracks") + QStringLiteral("..."));

    // Save tags in the background
    QFuture<void> future = QtConcurrent::run(&TrackSelectionDialog::SaveData, this, data_);
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, this, &TrackSelectionDialog::AcceptFinished);
    watcher->setFuture(future);

    return;
  }

  QDialog::accept();

  for (const Data &tag_data : std::as_const(data_)) {
    if (tag_data.pending_ || tag_data.results_.isEmpty() || tag_data.selected_result_ == -1) {
      continue;
    }

    const Song &new_metadata = tag_data.results_[tag_data.selected_result_];

    Q_EMIT SongChosen(tag_data.original_song_, new_metadata);
  }

}

void TrackSelectionDialog::AcceptFinished() {

  QFutureWatcher<void> *watcher = static_cast<QFutureWatcher<void>*>(sender());
  if (!watcher) return;
  watcher->deleteLater();

  SetLoading(QString());
  QDialog::accept();

}

void TrackSelectionDialog::NextSong() {
  int row = (ui_->song_list->currentRow() + 1) % ui_->song_list->count();
  ui_->song_list->setCurrentRow(row);
}

void TrackSelectionDialog::PreviousSong() {
  int row = (ui_->song_list->currentRow() - 1 + ui_->song_list->count()) % ui_->song_list->count();
  ui_->song_list->setCurrentRow(row);
}
