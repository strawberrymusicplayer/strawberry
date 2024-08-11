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

#ifndef FILEVIEWLIST_H
#define FILEVIEWLIST_H

#include <QObject>
#include <QListView>
#include <QItemSelectionModel>
#include <QList>
#include <QUrl>
#include <QString>
#include <QStringList>

class QWidget;
class QMimeData;
class QMenu;
class QMouseEvent;
class QContextMenuEvent;

class MimeData;

class FileViewList : public QListView {
  Q_OBJECT

 public:
  explicit FileViewList(QWidget *parent = nullptr);

  void mousePressEvent(QMouseEvent *e) override;

 Q_SIGNALS:
  void AddToPlaylist(QMimeData *data);
  void CopyToCollection(const QList<QUrl> &urls);
  void MoveToCollection(const QList<QUrl> &urls);
  void CopyToDevice(const QList<QUrl> &urls);
  void Delete(const QStringList &filenames);
  void EditTags(const QList<QUrl> &urls);
  void Back();
  void Forward();

 protected:
  void contextMenuEvent(QContextMenuEvent *e) override;

 private:
  QStringList FilenamesFromSelection() const;
  QList<QUrl> UrlListFromSelection() const;
  MimeData *MimeDataFromSelection() const;

 private Q_SLOTS:
  void LoadSlot();
  void AddToPlaylistSlot();
  void OpenInNewPlaylistSlot();
  void CopyToCollectionSlot();
  void MoveToCollectionSlot();
  void CopyToDeviceSlot();
  void DeleteSlot();
  void EditTagsSlot();
  void ShowInBrowser();

 private:
  QMenu *menu_;
  QItemSelection menu_selection_;
};

#endif  // FILEVIEWLIST_H
