/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>
#include <type_traits>

#include <QDialog>
#include <QWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHash>
#include <QPushButton>

#include "core/logging.h"
#include "collectionmodel.h"
#include "groupbydialog.h"
#include "ui_groupbydialog.h"

using std::make_unique;

inline size_t qHash(CollectionModel::GroupBy g, size_t seed = 0) noexcept {
  return ::qHash(static_cast<std::underlying_type_t<CollectionModel::GroupBy>>(g), seed);
}

class GroupByDialogPrivate {
 public:
  // Bidirectional mapping between combo-box index and CollectionModel::GroupBy.
  // Both maps are populated together at construction and never mutated, so a pair of QHashes is enough.
  QHash<int, CollectionModel::GroupBy> by_index;
  QHash<CollectionModel::GroupBy, int> by_group;

  // Bijection guard: hard-fail on duplicates so a programming error in the constructor's hardcoded list is caught immediately rather than producing a silently-wrong dialog (where lookups would later fall back to None / -1).
  // qFatal aborts unconditionally, so the maps can't end up partially populated.
  void Insert(CollectionModel::GroupBy g, int i) {
    if (by_index.contains(i) || by_group.contains(g)) {
      qFatal("GroupByDialog: duplicate mapping (GroupBy=%d, index=%d) - check the constructor's hardcoded list for a clash", static_cast<int>(g), i);
    }
    by_index.insert(i, g);
    by_group.insert(g, i);
  }

  // Looks up the GroupBy for a combo-box index.
  // If the index is not registered (which would indicate the combo box has more entries than the constructor populated, i.e. a programming error),
  // logs a warning and falls back to GroupBy::None rather than silently returning a default-constructed enum value.
  CollectionModel::GroupBy GroupByForIndex(int index) const {
    const auto it = by_index.constFind(index);
    if (it == by_index.constEnd()) {
      qLog(Warning) << "GroupByDialog: combo-box index" << index << "is not registered; falling back to GroupBy::None";
      return CollectionModel::GroupBy::None;
    }
    return it.value();
  }

  // Looks up the combo-box index for a GroupBy.
  // Returns -1 ("no mapping") when the GroupBy isn't registered,
  // so callers can leave the combo box at its current selection instead of silently snapping to index 0.
  int IndexForGroupBy(CollectionModel::GroupBy g) const {
    const auto it = by_group.constFind(g);
    if (it == by_group.constEnd()) {
      qLog(Warning) << "GroupByDialog: GroupBy" << static_cast<int>(g) << "is not registered; combo box left unchanged";
      return -1;
    }
    return it.value();
  }
};

GroupByDialog::GroupByDialog(QWidget *parent) : QDialog(parent), ui_(make_unique<Ui_GroupByDialog>()), p_(make_unique<GroupByDialogPrivate>()) {

  ui_->setupUi(this);
  Reset();

  p_->Insert(CollectionModel::GroupBy::None, 0);
  p_->Insert(CollectionModel::GroupBy::Artist, 1);
  p_->Insert(CollectionModel::GroupBy::AlbumArtist, 2);
  p_->Insert(CollectionModel::GroupBy::Album, 3);
  p_->Insert(CollectionModel::GroupBy::AlbumDisc, 4);
  p_->Insert(CollectionModel::GroupBy::Disc, 5);
  p_->Insert(CollectionModel::GroupBy::Format, 6);
  p_->Insert(CollectionModel::GroupBy::Genre, 7);
  p_->Insert(CollectionModel::GroupBy::Year, 8);
  p_->Insert(CollectionModel::GroupBy::YearAlbum, 9);
  p_->Insert(CollectionModel::GroupBy::YearAlbumDisc, 10);
  p_->Insert(CollectionModel::GroupBy::OriginalYear, 11);
  p_->Insert(CollectionModel::GroupBy::OriginalYearAlbum, 12);
  p_->Insert(CollectionModel::GroupBy::Composer, 13);
  p_->Insert(CollectionModel::GroupBy::Performer, 14);
  p_->Insert(CollectionModel::GroupBy::Grouping, 15);
  p_->Insert(CollectionModel::GroupBy::FileType, 16);
  p_->Insert(CollectionModel::GroupBy::Samplerate, 17);
  p_->Insert(CollectionModel::GroupBy::Bitdepth, 18);
  p_->Insert(CollectionModel::GroupBy::Bitrate, 19);

  QObject::connect(ui_->buttonbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &GroupByDialog::Reset);

  resize(sizeHint());

}

GroupByDialog::~GroupByDialog() = default;

void GroupByDialog::Reset() {

  ui_->combobox_first->setCurrentIndex(2);   // Album Artist
  ui_->combobox_second->setCurrentIndex(4);  // Album Disc
  ui_->combobox_third->setCurrentIndex(0);   // None
  ui_->checkbox_separate_albums_by_grouping->setChecked(false);

}

void GroupByDialog::accept() {

  Q_EMIT Accepted(CollectionModel::Grouping(
      p_->GroupByForIndex(ui_->combobox_first->currentIndex()),
      p_->GroupByForIndex(ui_->combobox_second->currentIndex()),
      p_->GroupByForIndex(ui_->combobox_third->currentIndex())),
    ui_->checkbox_separate_albums_by_grouping->isChecked()
   );
  QDialog::accept();

}

void GroupByDialog::CollectionGroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping) {

  const int first = p_->IndexForGroupBy(g[0]);
  if (first >= 0) ui_->combobox_first->setCurrentIndex(first);
  const int second = p_->IndexForGroupBy(g[1]);
  if (second >= 0) ui_->combobox_second->setCurrentIndex(second);
  const int third = p_->IndexForGroupBy(g[2]);
  if (third >= 0) ui_->combobox_third->setCurrentIndex(third);
  ui_->checkbox_separate_albums_by_grouping->setChecked(separate_albums_by_grouping);

}
