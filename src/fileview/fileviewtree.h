/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FILEVIEWTREE_H
#define FILEVIEWTREE_H

#include <QObject>
#include <QTreeView>
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

class FileViewTree : public QTreeView {
  Q_OBJECT

 public:
  explicit FileViewTree(QWidget *parent = nullptr);

  void mousePressEvent(QMouseEvent *e) override;

 Q_SIGNALS:
  void AddToPlaylist(QMimeData *data);
  void CopyToCollection(const QList<QUrl> &urls);
  void MoveToCollection(const QList<QUrl> &urls);
  void CopyToDevice(const QList<QUrl> &urls);
  void Delete(const QStringList &filenames);
  void EditTags(const QList<QUrl> &urls);

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

#endif  // FILEVIEWTREE_H
