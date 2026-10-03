/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef STREAMINGLIBRARYVIEW_H
#define STREAMINGLIBRARYVIEW_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

class StreamingService;
class StreamingCollectionView;
class StreamingCollectionViewContainer;

class StreamingLibraryView : public QWidget {
  Q_OBJECT

 public:
  explicit StreamingLibraryView(const SharedPtr<StreamingService> service, const QString &settings_group, QWidget *parent = nullptr);

  void ReloadSettings();

  StreamingCollectionView *view() const;

  bool SearchFieldHasFocus() const;
  void FocusSearchField();

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
  StreamingCollectionViewContainer *container_;
};

#endif  // STREAMINGLIBRARYVIEW_H
