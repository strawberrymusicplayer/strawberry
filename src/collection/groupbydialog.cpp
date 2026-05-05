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

#include <QDialog>
#include <QList>
#include <QHash>
#include <QWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

#include "collectionmodel.h"
#include "groupbydialog.h"
#include "ui_groupbydialog.h"

using std::make_unique;

class GroupByDialogPrivate {
 public:
  QList<CollectionModel::GroupBy> index_to_group_by_;
  QHash<CollectionModel::GroupBy, int> group_by_to_index_;
};

GroupByDialog::GroupByDialog(QWidget *parent) : QDialog(parent), ui_(make_unique<Ui_GroupByDialog>()), p_(make_unique<GroupByDialogPrivate>()) {

  ui_->setupUi(this);
  Reset();

  p_->index_to_group_by_ = {
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
    CollectionModel::GroupBy::Composer,
    CollectionModel::GroupBy::Performer,
    CollectionModel::GroupBy::Grouping,
    CollectionModel::GroupBy::FileType,
    CollectionModel::GroupBy::Samplerate,
    CollectionModel::GroupBy::Bitdepth,
    CollectionModel::GroupBy::Bitrate,
  };
  for (int i = 0; i < p_->index_to_group_by_.size(); ++i) {
    p_->group_by_to_index_.insert(p_->index_to_group_by_[i], i);
  }

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
      p_->index_to_group_by_[ui_->combobox_first->currentIndex()],
      p_->index_to_group_by_[ui_->combobox_second->currentIndex()],
      p_->index_to_group_by_[ui_->combobox_third->currentIndex()]),
    ui_->checkbox_separate_albums_by_grouping->isChecked()
   );
  QDialog::accept();

}

void GroupByDialog::CollectionGroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping) {

  ui_->combobox_first->setCurrentIndex(p_->group_by_to_index_.value(g[0]));
  ui_->combobox_second->setCurrentIndex(p_->group_by_to_index_.value(g[1]));
  ui_->combobox_third->setCurrentIndex(p_->group_by_to_index_.value(g[2]));
  ui_->checkbox_separate_albums_by_grouping->setChecked(separate_albums_by_grouping);

}
