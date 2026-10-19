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

#include <algorithm>
#include <optional>

#include <QtGlobal>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSizePolicy>
#include <QString>

#include "includes/shared_ptr.h"
#include "constants/timeconstants.h"
#include "core/song.h"
#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricsfetcher.h"
#include "lyrics/lyricssearchresult.h"
#include "widgets/busyindicator.h"
#include "widgets/stretchheaderview.h"
#include "lyricssearchdialog.h"
#include "ui_lyricssearchdialog.h"

using namespace Qt::Literals::StringLiterals;

namespace {
enum Column {
  Column_Provider,
  Column_Artist,
  Column_Album,
  Column_Title,
};
}  // namespace

LyricsSearchDialog::LyricsSearchDialog(const SharedPtr<LyricsProviders> lyrics_providers, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_LyricsSearchDialog),
      lyrics_fetcher_(new LyricsFetcher(lyrics_providers, this)),
      id_(0),
      duration_(-1),
      search_type_(LyricsSearchType::Lyrics) {

  ui_->setupUi(this);

  // Keep the space for the busy indicator when it's hidden, so the other widgets don't move when searching, and don't let it grow taller than the search row.
  QSizePolicy busy_size_policy = ui_->busy->sizePolicy();
  busy_size_policy.setVerticalPolicy(QSizePolicy::Fixed);
  busy_size_policy.setRetainSizeWhenHidden(true);
  ui_->busy->setSizePolicy(busy_size_policy);
  ui_->busy->hide();

  // The columns are stretched to the width of the results.
  StretchHeaderView *header = new StretchHeaderView(Qt::Horizontal, this);
  ui_->results->setHeader(header);
  header->SetStretchEnabled(true);
  header->SetColumnWidth(Column_Provider, 0.2);
  header->SetColumnWidth(Column_Artist, 0.3);
  header->SetColumnWidth(Column_Album, 0.25);
  header->SetColumnWidth(Column_Title, 0.25);

  QObject::connect(lyrics_fetcher_, &LyricsFetcher::SearchFinished, this, &LyricsSearchDialog::SearchFinished);
  QObject::connect(ui_->search, &QPushButton::clicked, this, &LyricsSearchDialog::Search);
  QObject::connect(ui_->artist, &QLineEdit::returnPressed, this, &LyricsSearchDialog::Search);
  QObject::connect(ui_->album, &QLineEdit::returnPressed, this, &LyricsSearchDialog::Search);
  QObject::connect(ui_->title, &QLineEdit::returnPressed, this, &LyricsSearchDialog::Search);
  QObject::connect(ui_->results, &QTreeWidget::currentItemChanged, this, &LyricsSearchDialog::CurrentResultChanged);
  QObject::connect(ui_->results, &QTreeWidget::itemDoubleClicked, this, &LyricsSearchDialog::accept);

}

LyricsSearchDialog::~LyricsSearchDialog() {
  delete ui_;
}

std::optional<LyricsSearchResult> LyricsSearchDialog::Exec(const Song &song, const LyricsSearchType search_type) {

  search_type_ = search_type;
  setWindowTitle(search_type == LyricsSearchType::SyncedLyrics ? tr("Search for synchronized lyrics") : tr("Search for lyrics"));

  albumartist_ = song.effective_albumartist();
  duration_ = song.length_nanosec() / kNsecPerSec;
  ui_->artist->setText(song.artist());
  ui_->album->setText(song.album());
  ui_->title->setText(song.title());

  Search();

  const int result = exec();

  // Stop searching when the dialog is closed while searching.
  lyrics_fetcher_->Clear();
  id_ = 0;
  SetSearching(false);

  if (result != QDialog::Accepted) return std::nullopt;

  const QTreeWidgetItem *item = ui_->results->currentItem();
  if (!item) return std::nullopt;

  const qsizetype index = item->data(Column_Provider, Qt::UserRole).toLongLong();
  if (index < 0 || index >= results_.size()) return std::nullopt;

  return results_.at(index);

}

void LyricsSearchDialog::Search() {

  lyrics_fetcher_->Clear();
  results_.clear();
  ui_->results->clear();
  ui_->lyrics->clear();

  id_ = lyrics_fetcher_->SearchForLyrics(albumartist_, ui_->artist->text(), ui_->album->text(), ui_->title->text(), duration_, search_type_ == LyricsSearchType::SyncedLyrics);
  SetSearching(true);

}

void LyricsSearchDialog::SetSearching(const bool searching) {

  ui_->search->setEnabled(!searching);
  ui_->busy->setVisible(searching);
  ui_->status->setText(searching ? tr("Searching...") : QString());
  ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(!searching && ui_->results->currentItem() != nullptr);

}

void LyricsSearchDialog::SearchFinished(const quint64 id, const LyricsSearchResults &results) {

  if (id != id_) return;
  id_ = 0;

  // The best lyrics first, only the lyrics with synchronized lyrics when searching for synchronized lyrics.
  results_.clear();
  for (const LyricsSearchResult &result : results) {
    if ((LyricsSearchTypeHasLyrics(search_type_) && !result.lyrics.isEmpty()) || (LyricsSearchTypeHasSyncedLyrics(search_type_) && !result.synced_lyrics.isEmpty())) {
      results_ << result;
    }
  }
  std::stable_sort(results_.begin(), results_.end(), [](const LyricsSearchResult &a, const LyricsSearchResult &b) { return a.score > b.score; });

  for (qsizetype i = 0; i < results_.size(); ++i) {
    const LyricsSearchResult &result = results_.at(i);
    QTreeWidgetItem *item = new QTreeWidgetItem(ui_->results);
    item->setText(Column_Provider, result.provider);
    item->setText(Column_Artist, result.artist);
    item->setText(Column_Album, result.album);
    item->setText(Column_Title, result.title);
    item->setData(Column_Provider, Qt::UserRole, static_cast<qlonglong>(i));
  }

  if (!results_.isEmpty()) {
    ui_->results->setCurrentItem(ui_->results->topLevelItem(0));
  }

  SetSearching(false);

  if (results_.isEmpty()) {
    ui_->status->setText(search_type_ == LyricsSearchType::SyncedLyrics ? tr("No synchronized lyrics found.") : tr("No lyrics found."));
  }

}

void LyricsSearchDialog::CurrentResultChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) {

  Q_UNUSED(previous)

  ui_->lyrics->clear();
  ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(current != nullptr && id_ == 0);
  if (!current) return;

  const qsizetype index = current->data(Column_Provider, Qt::UserRole).toLongLong();
  if (index >= 0 && index < results_.size()) {
    const LyricsSearchResult &result = results_.at(index);
    switch (search_type_) {
      case LyricsSearchType::Lyrics:
        ui_->lyrics->setPlainText(result.lyrics);
        break;
      case LyricsSearchType::SyncedLyrics:
        ui_->lyrics->setPlainText(result.synced_lyrics);
        break;
      case LyricsSearchType::Both:{
        // Both are used, so show both.
        QString text = result.lyrics;
        if (!result.synced_lyrics.isEmpty()) {
          if (!text.isEmpty()) text += u"\n\n"_s;
          text += u"--- "_s + tr("Synchronized lyrics") + u" ---\n"_s + result.synced_lyrics;
        }
        ui_->lyrics->setPlainText(text);
        break;
      }
    }
  }

}
