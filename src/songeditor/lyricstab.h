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

#ifndef LYRICSTAB_H
#define LYRICSTAB_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

class QLabel;
class QComboBox;
class QPushButton;
class TextEdit;
class Ui_LyricsTab;

// The tab for editing the lyrics or the synchronized lyrics of songs, used by the tag editor and the lyrics editor.
// The editors handle the songs, this is only the widgets.
class LyricsTab : public QWidget {
  Q_OBJECT

 public:
  explicit LyricsTab(const bool synced, QWidget *parent = nullptr);
  ~LyricsTab() override;

  bool synced() const { return synced_; }
  // The field ID of the lyrics editor, it's also its object name.
  QString field_id() const;

  TextEdit *lyrics() const;
  QLabel *label() const;
  QComboBox *storage() const;
  // What to search for, it's the same in the lyrics and synchronized lyrics tabs.
  QComboBox *search_type() const;
  QPushButton *fetch_button() const;
  QPushButton *search_button() const;
  QLabel *status() const;

 Q_SIGNALS:
  // Completing the lyrics automatically, searching for lyrics, the storage or what to search for was changed by the user.
  void FetchLyrics();
  void SearchLyrics();
  void StorageChanged(const int index);
  void SearchTypeChanged(const int index);

 private:
  const bool synced_;
  Ui_LyricsTab *ui_;
};

#endif  // LYRICSTAB_H
