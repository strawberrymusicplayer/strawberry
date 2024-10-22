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

#ifndef TRACKSELECTIONDIALOG_H
#define TRACKSELECTIONDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QList>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QWidget;
class QTreeWidget;
class QPushButton;
class Ui_TrackSelectionDialog;
class TagReaderClient;

class TrackSelectionDialog : public QDialog {
  Q_OBJECT

 public:
  explicit TrackSelectionDialog(const SharedPtr<TagReaderClient> tagreader_client, QWidget *parent = nullptr);
  ~TrackSelectionDialog() override;

  void set_save_on_close(bool save_on_close) { save_on_close_ = save_on_close; }

  void Init(const SongList &songs);

 public Q_SLOTS:
  void FetchTagProgress(const Song &original_song, const QString &progress);
  void FetchTagFinished(const Song &original_song, const SongList &songs_guessed);

  // QDialog
  void accept() override;

 Q_SIGNALS:
  void Error(const QString &error);
  void SongChosen(const Song &original_song, const Song &new_metadata);

 private Q_SLOTS:
  void UpdateStack();

  void NextSong();
  void PreviousSong();

  void ResultSelected();
  void AcceptFinished();

 private:
  Ui_TrackSelectionDialog *ui_;

  struct Data {
    Data() : pending_(true), selected_result_(0) {}

    Song original_song_;
    bool pending_;
    QString progress_string_;
    SongList results_;
    int selected_result_;
  };

  void AddDivider(const QString &text, QTreeWidget *parent) const;
  static void AddSong(const Song &song, int result_index, QTreeWidget *parent);

  void SetLoading(const QString &message);
  void SaveData(const QList<Data> &data) const;

 private:
  const SharedPtr<TagReaderClient> tagreader_client_;

  QList<Data> data_;

  QPushButton *previous_button_;
  QPushButton *next_button_;

  bool save_on_close_;
};

#endif  // TRACKSELECTIONDIALOG_H
