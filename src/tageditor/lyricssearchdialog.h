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

#ifndef LYRICSSEARCHDIALOG_H
#define LYRICSSEARCHDIALOG_H

#include "config.h"

#include <optional>

#include <QtGlobal>
#include <QObject>
#include <QDialog>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "lyrics/lyricssearchresult.h"

class QWidget;
class QTreeWidgetItem;
class Ui_LyricsSearchDialog;
class LyricsProviders;
class LyricsFetcher;

// Searches all lyrics providers for a song, and lets the user choose which lyrics to use.
class LyricsSearchDialog : public QDialog {
  Q_OBJECT

 public:
  explicit LyricsSearchDialog(const SharedPtr<LyricsProviders> lyrics_providers, QWidget *parent = nullptr);
  ~LyricsSearchDialog() override;

  // Searches for lyrics for the song and shows the dialog, returns the chosen lyrics, or nothing when the dialog was cancelled.
  // With synced, only the lyrics with synchronized lyrics are shown, and the synchronized lyrics are shown.
  std::optional<LyricsSearchResult> Exec(const Song &song, const bool synced);

 private Q_SLOTS:
  void Search();
  void SearchFinished(const quint64 id, const LyricsSearchResults &results);
  void CurrentResultChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

 private:
  void SetSearching(const bool searching);

  Ui_LyricsSearchDialog *ui_;
  LyricsFetcher *lyrics_fetcher_;
  quint64 id_;
  // The search uses the album artist and the length of the song, they can't be edited in the dialog.
  QString albumartist_;
  qint64 duration_;
  bool synced_;
  LyricsSearchResults results_;
};

#endif  // LYRICSSEARCHDIALOG_H
