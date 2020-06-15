/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef INTERNETSONGSVIEW_H
#define INTERNETSONGSVIEW_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

#include "core/song.h"
#include "settings/settingsdialog.h"
#include "ui_internetcollectionviewcontainer.h"

class QContextMenuEvent;

class Application;
class InternetService;
class InternetCollectionView;

class InternetSongsView : public QWidget {
  Q_OBJECT

 public:
  explicit InternetSongsView(Application *app, InternetService *service, const QString &settings_group, const SettingsDialog::Page settings_page, QWidget *parent = nullptr);
  ~InternetSongsView() override;

  void ReloadSettings();

  InternetCollectionView *view() const { return ui_->view; }

 private slots:
  void OpenSettingsDialog();
  void GetSongs();
  void AbortGetSongs();
  void SongsFinished(const SongList &songs, const QString &error);

 private:
  Application *app_;
  InternetService *service_;
  QString settings_group_;
  SettingsDialog::Page settings_page_;
  Ui_InternetCollectionViewContainer *ui_;

};

#endif  // INTERNETSONGSVIEW_H
