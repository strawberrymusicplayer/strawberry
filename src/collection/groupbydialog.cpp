/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <array>
#include <algorithm>
#include <iterator>

#include <QDialog>
#include <QWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

#include "collectionmodel.h"
#include "groupbydialog.h"
#include "ui_groupbydialog.h"

using std::make_unique;

namespace {

// The groupings in the order of the combo boxes.
constexpr std::array kGroupBys {
  CollectionModel::GroupBy::None,
  CollectionModel::GroupBy::Artist,
  CollectionModel::GroupBy::AlbumArtist,
  CollectionModel::GroupBy::Album,
  CollectionModel::GroupBy::AlbumDisc,
  CollectionModel::GroupBy::Disc,
  CollectionModel::GroupBy::Format,
  CollectionModel::GroupBy::Genre,
  CollectionModel::GroupBy::Year,
  CollectionModel::GroupBy::YearAlbum,
  CollectionModel::GroupBy::YearAlbumDisc,
  CollectionModel::GroupBy::OriginalYear,
  CollectionModel::GroupBy::OriginalYearAlbum,
  CollectionModel::GroupBy::OriginalYearAlbumDisc,
  CollectionModel::GroupBy::Composer,
  CollectionModel::GroupBy::Performer,
  CollectionModel::GroupBy::Grouping,
  CollectionModel::GroupBy::FileType,
  CollectionModel::GroupBy::Samplerate,
  CollectionModel::GroupBy::Bitdepth,
  CollectionModel::GroupBy::Bitrate
};

CollectionModel::GroupBy GroupByForComboBoxIndex(const int combo_box_index) {

  if (combo_box_index < 0 || combo_box_index >= static_cast<int>(kGroupBys.size())) {
    return CollectionModel::GroupBy::None;
  }

  return kGroupBys[static_cast<size_t>(combo_box_index)];

}

int ComboBoxIndexForGroupBy(const CollectionModel::GroupBy group_by) {

  const auto it = std::find(kGroupBys.cbegin(), kGroupBys.cend(), group_by);
  if (it == kGroupBys.cend()) return 0;

  return static_cast<int>(std::distance(kGroupBys.cbegin(), it));

}

}  // namespace

GroupByDialog::GroupByDialog(QWidget *parent) : QDialog(parent), ui_(make_unique<Ui_GroupByDialog>()) {

  ui_->setupUi(this);
  Reset();

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
      GroupByForComboBoxIndex(ui_->combobox_first->currentIndex()),
      GroupByForComboBoxIndex(ui_->combobox_second->currentIndex()),
      GroupByForComboBoxIndex(ui_->combobox_third->currentIndex())),
    ui_->checkbox_separate_albums_by_grouping->isChecked()
   );
  QDialog::accept();

}

void GroupByDialog::CollectionGroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping) {

  ui_->combobox_first->setCurrentIndex(ComboBoxIndexForGroupBy(g[0]));
  ui_->combobox_second->setCurrentIndex(ComboBoxIndexForGroupBy(g[1]));
  ui_->combobox_third->setCurrentIndex(ComboBoxIndexForGroupBy(g[2]));
  ui_->checkbox_separate_albums_by_grouping->setChecked(separate_albums_by_grouping);

}
