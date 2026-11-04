/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTACTIONHANDLER_H
#define PLAYLISTACTIONHANDLER_H

#include <functional>

#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QString>

class QAbstractItemView;
class QDialog;
class QMimeData;
class LongPressHandler;
class MimeData;

class PlaylistActionHandler : public QObject {
  Q_OBJECT

 public:
  using Eligible = std::function<bool(const QModelIndex &)>;
  using CreateMimeData = std::function<MimeData *(const QModelIndex &)>;

  explicit PlaylistActionHandler(QAbstractItemView *view, const QString &section, Eligible eligible, CreateMimeData create_mimedata);

 Q_SIGNALS:
  void AddToPlaylist(QMimeData *data);

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 private:
  void Cancel();
  void ShowActions();

  QAbstractItemView *view_;
  QString section_;
  Eligible eligible_;
  CreateMimeData create_mimedata_;
  LongPressHandler *long_press_;
  QPersistentModelIndex index_;
  QPointer<QDialog> dialog_;
};

#endif  // PLAYLISTACTIONHANDLER_H
