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
#include <QWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

#include "collectionmodel.h"
#include "groupbydialog.h"
#include "ui_groupbydialog.h"

#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <boost/operators.hpp>

using std::make_unique;

using boost::multi_index_container;
using boost::multi_index::indexed_by;
using boost::multi_index::ordered_unique;
using boost::multi_index::tag;
using boost::multi_index::member;

namespace {

struct Mapping {
  Mapping(CollectionModel::GroupBy g, int i) : group_by(g), combo_box_index(i) {}

  CollectionModel::GroupBy group_by;
  int combo_box_index;
};

struct tag_index {};
struct tag_group_by {};

}  // namespace

class GroupByDialogPrivate {
 private:
  using MappingContainer = multi_index_container<Mapping, indexed_by<ordered_unique<tag<tag_index>, member<Mapping, int, &Mapping::combo_box_index>>, ordered_unique<tag<tag_group_by>, member<Mapping, CollectionModel::GroupBy, &Mapping::group_by>>>>;

 public:
  MappingContainer mapping_;
};

GroupByDialog::GroupByDialog(QWidget *parent) : QDialog(parent), ui_(make_unique<Ui_GroupByDialog>()), p_(make_unique<GroupByDialogPrivate>()) {

  ui_->setupUi(this);
  Reset();

  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::None, 0));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Artist, 1));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::AlbumArtist, 2));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Album, 3));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::AlbumDisc, 4));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Disc, 5));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Format, 6));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Genre, 7));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Year, 8));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::YearAlbum, 9));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::YearAlbumDisc, 10));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::OriginalYear, 11));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::OriginalYearAlbum, 12));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::OriginalYearAlbumDisc, 13));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Composer, 14));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Performer, 15));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Grouping, 16));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::FileType, 17));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Samplerate, 18));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Bitdepth, 19));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy::Bitrate, 20));

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

  const auto &by_index = p_->mapping_.get<tag_index>();
  const auto it1 = by_index.find(ui_->combobox_first->currentIndex());
  const auto it2 = by_index.find(ui_->combobox_second->currentIndex());
  const auto it3 = by_index.find(ui_->combobox_third->currentIndex());
  Q_EMIT Accepted(CollectionModel::Grouping(
      it1 != by_index.end() ? it1->group_by : CollectionModel::GroupBy::None,
      it2 != by_index.end() ? it2->group_by : CollectionModel::GroupBy::None,
      it3 != by_index.end() ? it3->group_by : CollectionModel::GroupBy::None),
    ui_->checkbox_separate_albums_by_grouping->isChecked()
   );
  QDialog::accept();

}

void GroupByDialog::CollectionGroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping) {

  const auto &by_group = p_->mapping_.get<tag_group_by>();
  const auto it1 = by_group.find(g[0]);
  const auto it2 = by_group.find(g[1]);
  const auto it3 = by_group.find(g[2]);
  ui_->combobox_first->setCurrentIndex(it1 != by_group.end() ? it1->combo_box_index : 0);
  ui_->combobox_second->setCurrentIndex(it2 != by_group.end() ? it2->combo_box_index : 0);
  ui_->combobox_third->setCurrentIndex(it3 != by_group.end() ? it3->combo_box_index : 0);
  ui_->checkbox_separate_albums_by_grouping->setChecked(separate_albums_by_grouping);

}
