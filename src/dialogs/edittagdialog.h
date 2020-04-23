/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef EDITTAGDIALOG_H
#define EDITTAGDIALOG_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QDialog>
#include <QFuture>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QImage>

#include "core/song.h"
#include "core/tagreaderclient.h"
#include "playlist/playlistitem.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"

class QWidget;
class QMenu;
class QLabel;
class QAbstractButton;
class QPushButton;
class QEvent;
class QShowEvent;
class QHideEvent;

class Application;
class AlbumCoverChoiceController;
class TrackSelectionDialog;
class Ui_EditTagDialog;
#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
class TagFetcher;
#endif

class EditTagDialog : public QDialog {
  Q_OBJECT

 public:
  explicit EditTagDialog(Application *app, QWidget *parent = nullptr);
  ~EditTagDialog();

  static const char *kHintText;
  static const char *kSettingsGroup;

  void SetSongs(const SongList &songs, const PlaylistItemList &items = PlaylistItemList());

  PlaylistItemList playlist_items() const { return playlist_items_; }

  void accept();

 signals:
  void Error(const QString &message);

 protected:
  bool eventFilter(QObject *o, QEvent *e);
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

 private:
  struct Data {
    Data(const Song &song = Song()) : original_(song), current_(song) {}

    static QVariant value(const Song &song, const QString &id);
    QVariant original_value(const QString &id) const {
      return value(original_, id);
    }
    QVariant current_value(const QString &id) const {
      return value(current_, id);
    }

    void set_value(const QString &id, const QVariant &value);

    Song original_;
    Song current_;
  };

 private slots:
  void SetSongsFinished(QFuture<QList<EditTagDialog::Data>> future);
  void AcceptFinished();

  void SelectionChanged();
  void FieldValueEdited();
  void ResetField();
  void ButtonClicked(QAbstractButton *button);
  void ResetPlayCounts();
  void FetchTag();
  void FetchTagSongChosen(const Song &original_song, const Song &new_metadata);

  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result);

  void LoadCoverFromFile();
  void SaveCoverToFile();
  void LoadCoverFromURL();
  void SearchForCover();
  void UnsetCover();
  void ShowCover();

  void PreviousSong();
  void NextSong();

  void SongSaveComplete(TagReaderReply *reply, const QString &filename, const Song &song);

 private:
  struct FieldData {
    FieldData(QLabel *label = nullptr, QWidget *editor = nullptr, const QString &id = QString())
        : label_(label), editor_(editor), id_(id) {}

    QLabel *label_;
    QWidget *editor_;
    QString id_;
  };

  Song *GetFirstSelected();
  void UpdateCoverOf(const Song &selected, const QModelIndexList &sel, const QUrl &cover_url);

  bool DoesValueVary(const QModelIndexList &sel, const QString &id) const;
  bool IsValueModified(const QModelIndexList &sel, const QString &id) const;

  void InitFieldValue(const FieldData &field, const QModelIndexList &sel);
  void UpdateFieldValue(const FieldData &field, const QModelIndexList &sel);
  void UpdateModifiedField(const FieldData &field, const QModelIndexList &sel);
  void ResetFieldValue(const FieldData &field, const QModelIndexList &sel);

  void UpdateSummaryTab(const Song &song);
  void UpdateStatisticsTab(const Song &song);

  void UpdateUI(const QModelIndexList &sel);

  bool SetLoading(const QString &message);
  void SetSongListVisibility(bool visible);

  // Called by QtConcurrentRun
  QList<Data> LoadData(const SongList &songs) const;
  void SaveData(const QList<Data> &tag_data);

 private:
  Ui_EditTagDialog *ui_;

  Application *app_;
  AlbumCoverChoiceController *album_cover_choice_controller_;

  bool loading_;

  PlaylistItemList playlist_items_;
  QList<Data> data_;
  QList<FieldData> fields_;

  bool ignore_edits_;

#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
  TagFetcher *tag_fetcher_;
#endif

  AlbumCoverLoaderOptions cover_options_;
  quint64 cover_art_id_;
  bool cover_art_is_set_;

  // A copy of the original, unscaled album cover.
  QImage original_;

  QMenu *cover_menu_;

  QPushButton *previous_button_;
  QPushButton *next_button_;

  TrackSelectionDialog *results_dialog_;

  int pending_;
};

#endif  // EDITTAGDIALOG_H
