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

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QString>

#include "core/song.h"
#include "widgets/lineedit.h"
#include "lyricssearchtype.h"
#include "lyricstab.h"
#include "ui_lyricstab.h"

using namespace Qt::Literals::StringLiterals;

LyricsTab::LyricsTab(const bool synced, QWidget *parent)
    : QWidget(parent),
      synced_(synced),
      ui_(new Ui_LyricsTab) {

  ui_->setupUi(this);

  // The object name of the editor is the field ID used by the editors.
  ui_->lyrics->setObjectName(field_id());
  ui_->label_lyrics->hide();

  // Where to save the lyrics, items are enabled per song depending on what the song supports.
  ui_->storage->addItem(tr("Database"), static_cast<int>(Song::StorageType::Database));
  ui_->storage->addItem(tr("Tag"), static_cast<int>(Song::StorageType::Tag));
  if (synced_) {
    ui_->storage->addItem(tr("LRC file"), static_cast<int>(Song::StorageType::File));
    ui_->label_storage->setText(tr("Save synced lyrics to"));
    ui_->storage->setAccessibleName(tr("Save synced lyrics to"));
    ui_->storage->setAccessibleDescription(tr("Where the synchronized lyrics are saved: the collection database only, the tags of the file or an LRC file next to it"));
    ui_->storage->setToolTip(tr("Save the synchronized lyrics to the collection database only, to the tags of the file or to an LRC file next to it"));
  }
  else {
    ui_->storage->setToolTip(tr("Save the lyrics to the collection database only or to the tags of the file"));
  }

  // The search controls are the same in both tabs, the editors keep them in sync.
  ui_->search_type->addItem(tr("Lyrics"), static_cast<int>(LyricsSearchType::Lyrics));
  ui_->search_type->addItem(tr("Synced lyrics"), static_cast<int>(LyricsSearchType::SyncedLyrics));
  ui_->search_type->addItem(tr("Both lyrics and synced lyrics"), static_cast<int>(LyricsSearchType::Both));
  QObject::connect(ui_->search_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LyricsTab::SearchTypeChanged);

  QObject::connect(ui_->fetch, &QPushButton::clicked, this, &LyricsTab::FetchLyrics);
  QObject::connect(ui_->search, &QPushButton::clicked, this, &LyricsTab::SearchLyrics);
  // The editors block the signals while setting the current index, so only changes by the user are emitted.
  QObject::connect(ui_->storage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LyricsTab::StorageChanged);

}

LyricsTab::~LyricsTab() {
  delete ui_;
}

QString LyricsTab::field_id() const {
  return synced_ ? u"synced_lyrics"_s : u"lyrics"_s;
}

TextEdit *LyricsTab::lyrics() const { return ui_->lyrics; }
QLabel *LyricsTab::label() const { return ui_->label_lyrics; }
QComboBox *LyricsTab::storage() const { return ui_->storage; }
QComboBox *LyricsTab::search_type() const { return ui_->search_type; }
QPushButton *LyricsTab::fetch_button() const { return ui_->fetch; }
QPushButton *LyricsTab::search_button() const { return ui_->search; }
QLabel *LyricsTab::status() const { return ui_->status; }
