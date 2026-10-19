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

#include "config.h"

#include <QWidget>
#include <QList>
#include <QString>
#include <QSplitter>

#include "includes/shared_ptr.h"
#include "songeditor.h"
#include "lyricseditor.h"
#include "ui_lyricseditor.h"

namespace {
constexpr char kSettingsGroup[] = "LyricsEditor";
}

LyricsEditor::LyricsEditor(const SharedPtr<TagReaderClient> tagreader_client,
                           const SharedPtr<CollectionBackend> collection_backend,
                           const SharedPtr<LyricsProviders> lyrics_providers,
                           QWidget *parent)
    : SongEditor(tagreader_client, collection_backend, lyrics_providers, parent),
      ui_(new Ui_LyricsEditor) {

  ui_->setupUi(this);
  ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);

  // The lyrics tabs are added, and the fields in the tabs are found.
  InitSongEditor(ui_->song_list, ui_->tab_widget, ui_->button_box, ui_->loading_label);

}

LyricsEditor::~LyricsEditor() {
  delete ui_;
}

QString LyricsEditor::SettingsGroup() const {

  return QLatin1String(kSettingsGroup);

}

void LyricsEditor::Save() {

  // Only the lyrics are saved, the other fields of the songs are saved as they were loaded.
  for (int i = 0; i < data_.count(); ++i) {
    bool lyrics_changed = false;
    bool synced_lyrics_changed = false;
    PrepareLyricsForSave(&data_[i], &lyrics_changed, &synced_lyrics_changed);
    SaveLyricsData(i, lyrics_changed, synced_lyrics_changed);
  }

  if (save_tag_pending_ <= 0) SaveDataFinished();

}
