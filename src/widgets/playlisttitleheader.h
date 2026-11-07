/*
 * Strawberry Music Player
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

#ifndef PLAYLISTTITLEHEADER_H
#define PLAYLISTTITLEHEADER_H

#include "config.h"

#include <QWidget>
#include <QString>

class QLineEdit;
class QPushButton;
class QStackedWidget;

// Header of the playlist editor page: the playlist name, which can be tapped to rename the playlist inline, and a Back button.
class PlaylistTitleHeader : public QWidget {
  Q_OBJECT

 public:
  explicit PlaylistTitleHeader(QWidget *parent = nullptr);

  int playlist_id() const { return playlist_id_; }
  QString playlist_name() const { return playlist_name_; }
  bool is_renaming() const { return editing_playlist_id_ >= 0; }

  QPushButton *name_button() const { return name_button_; }
  QLineEdit *name_editor() const { return name_editor_; }
  QPushButton *back_button() const { return back_button_; }

 public Q_SLOTS:
  // Shows the playlist, or no name with an id below 0; switching to another playlist cancels an ongoing rename.
  void SetPlaylist(const int id, const QString &name);
  void StartRename();
  void CancelRename();

 Q_SIGNALS:
  void BackClicked();
  void RenameRequested(const int id, const QString &new_name);

 private Q_SLOTS:
  void SaveRename();
  void EditingFinished();

 private:
  QStackedWidget *title_;
  QPushButton *name_button_;
  QLineEdit *name_editor_;
  QPushButton *back_button_;
  int playlist_id_;
  QString playlist_name_;
  int editing_playlist_id_;
};

#endif  // PLAYLISTTITLEHEADER_H
