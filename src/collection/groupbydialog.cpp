/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <functional>

#include <QDialog>
#include <QWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

#include "collectionmodel.h"
#include "groupbydialog.h"
#include "ui_groupbydialog.h"

// boost::multi_index still relies on these being in the global namespace.
using std::placeholders::_1;
using std::placeholders::_2;

#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <boost/operators.hpp>

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
  typedef multi_index_container<
      Mapping,
      indexed_by<
          ordered_unique<tag<tag_index>, member<Mapping, int, &Mapping::combo_box_index> >,
          ordered_unique<tag<tag_group_by>, member<Mapping, CollectionModel::GroupBy, &Mapping::group_by> > > > MappingContainer;

 public:
  MappingContainer mapping_;
};

GroupByDialog::GroupByDialog(QWidget *parent) : QDialog(parent), ui_(new Ui_GroupByDialog), p_(new GroupByDialogPrivate) {

  ui_->setupUi(this);
  Reset();

  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_None, 0));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Artist, 1));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_AlbumArtist, 2));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Album, 3));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_AlbumDisc, 4));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Disc, 5));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Format, 6));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Genre, 7));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Year, 8));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_YearAlbum, 9));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_YearAlbumDisc, 10));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_OriginalYear, 11));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_OriginalYearAlbum, 12));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Composer, 13));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Performer, 14));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Grouping, 15));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_FileType, 16));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Samplerate, 17));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Bitdepth, 18));
  p_->mapping_.insert(Mapping(CollectionModel::GroupBy_Bitrate, 19));

  connect(ui_->buttonbox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), SLOT(Reset()));

  resize(sizeHint());

}

GroupByDialog::~GroupByDialog() = default;

void GroupByDialog::Reset() {
  ui_->combobox_first->setCurrentIndex(2);   // Album Artist
  ui_->combobox_second->setCurrentIndex(3);  // Album
  ui_->combobox_third->setCurrentIndex(0);   // None
}

void GroupByDialog::accept() {
  emit Accepted(CollectionModel::Grouping(
      p_->mapping_.get<tag_index>().find(ui_->combobox_first->currentIndex())->group_by,
      p_->mapping_.get<tag_index>().find(ui_->combobox_second->currentIndex())->group_by,
      p_->mapping_.get<tag_index>().find(ui_->combobox_third->currentIndex())->group_by)
   );
  QDialog::accept();
}

void GroupByDialog::CollectionGroupingChanged(const CollectionModel::Grouping &g) {
  ui_->combobox_first->setCurrentIndex(p_->mapping_.get<tag_group_by>().find(g[0])->combo_box_index);
  ui_->combobox_second->setCurrentIndex(p_->mapping_.get<tag_group_by>().find(g[1])->combo_box_index);
  ui_->combobox_third->setCurrentIndex(p_->mapping_.get<tag_group_by>().find(g[2])->combo_box_index);
}

