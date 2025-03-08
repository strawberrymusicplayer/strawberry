/*
 * Strawberry Music Player
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

#ifndef STREAMINGSONGSVIEW_H
#define STREAMINGSONGSVIEW_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QContextMenuEvent;

class StreamingService;
class StreamingCollectionView;

#include "ui_streamingcollectionviewcontainer.h"

class StreamingSongsView : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingSongsView(const SharedPtr<StreamingService> service, const QString &settings_group, QWidget *parent = nullptr);
  ~StreamingSongsView() override;

  void ReloadSettings();

  StreamingCollectionView *view() const { return ui_->view; }

  bool SearchFieldHasFocus() const { return ui_->filter_widget->SearchFieldHasFocus(); }
  void FocusSearchField() { ui_->filter_widget->FocusSearchField(); }

 private Q_SLOTS:
  void Configure();
  void GetSongs();
  void AbortGetSongs();
  void SongsFinished(const SongMap &songs, const QString &error);

 Q_SIGNALS:
  void ShowErrorDialog(const QString &error);
  void OpenSettingsDialog(const Song::Source source);

 private:
  const SharedPtr<StreamingService> service_;
  QString settings_group_;
  Ui_StreamingCollectionViewContainer *ui_;
};

#endif  // STREAMINGSONGSVIEW_H
