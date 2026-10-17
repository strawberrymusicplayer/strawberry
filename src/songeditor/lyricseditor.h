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

#ifndef LYRICSEDITOR_H
#define LYRICSEDITOR_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"
#include "songeditor.h"

class QWidget;

class TagReaderClient;
class CollectionBackend;
class LyricsProviders;
class Ui_LyricsEditor;

// Edits only the lyrics and synchronized lyrics of songs, nothing else is loaded or saved.
class LyricsEditor : public SongEditor {
  Q_OBJECT

 public:
  explicit LyricsEditor(const SharedPtr<TagReaderClient> tagreader_client,
                        const SharedPtr<CollectionBackend> collection_backend,
                        const SharedPtr<LyricsProviders> lyrics_providers,
                        QWidget *parent = nullptr);

  ~LyricsEditor() override;

 protected:
  // SongEditor
  QString SettingsGroup() const override;
  void Save() override;

 private:
  Ui_LyricsEditor *ui_;
};

#endif  // LYRICSEDITOR_H
