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

#ifndef ARTISTBIOVIEW_H
#define ARTISTBIOVIEW_H

#include <QObject>
#include <QWidget>
#include <QList>

#include "core/song.h"
#include "widgets/collapsibleinfopane.h"
#include "widgets/widgetfadehelper.h"
#include "widgets/collapsibleinfopane.h"
#include "playlist/playlistitem.h"
#include "smartplaylists/playlistgenerator_fwd.h"
#include "artistbiofetcher.h"

class QNetworkAccessManager;
class QTimeLine;
class QVBoxLayout;
class QScrollArea;
class QShowEvent;

class PrettyImageView;
class CollapsibleInfoPane;
class WidgetFadeHelper;

class ArtistBioView : public QWidget {
  Q_OBJECT

 public:
  explicit ArtistBioView(QWidget *parent = nullptr);
  ~ArtistBioView() override;

  static const char *kSettingsGroup;

 public slots:
  void SongChanged(const Song& metadata);
  void SongFinished();
  virtual void ReloadSettings();

 signals:
  void ShowSettingsDialog();

 protected:
  void showEvent(QShowEvent *e) override;

  void Update(const Song &metadata);
  void AddWidget(QWidget *widget);
  void AddSection(CollapsibleInfoPane *section);
  void Clear();
  void CollapseSections();

  bool NeedsUpdate(const Song& old_metadata, const Song &new_metadata) const;

 protected slots:
  void ResultReady(const int id, const ArtistBioFetcher::Result &result);
  void InfoResultReady(const int id, const CollapsibleInfoPane::Data &data);

 protected:
  QNetworkAccessManager *network_;
  ArtistBioFetcher *fetcher_;
  int current_request_id_;

 private:
  void MaybeUpdate(const Song &metadata);
  void ConnectWidget(QWidget *widget);

 private slots:
  void SectionToggled(const bool value);

 private:
  QVBoxLayout *container_;
  QList<QWidget*> widgets_;

  QWidget *section_container_;
  QList<CollapsibleInfoPane*> sections_;

  WidgetFadeHelper *fader_;

  Song queued_metadata_;
  Song old_metadata_;
  bool dirty_;
};

#endif  // ARTISTBIOVIEW_H
