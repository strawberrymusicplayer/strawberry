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

#include <QDialog>
#include <QWidget>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QIODevice>
#include <QDataStream>
#include <QByteArray>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QKeySequence>
#include <QPushButton>
#include <QTreeView>
#include <QSettings>
#include <QtDebug>

#include "core/logging.h"
#include "core/iconloader.h"
#include "collectionfilterwidget.h"
#include "collectionmodel.h"
#include "savedgroupingmanager.h"
#include "ui_savedgroupingmanager.h"

SavedGroupingManager::SavedGroupingManager(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_SavedGroupingManager),
      model_(new QStandardItemModel(0, 4, this)) {

  ui_->setupUi(this);

  model_->setHorizontalHeaderItem(0, new QStandardItem(tr("Name")));
  model_->setHorizontalHeaderItem(1, new QStandardItem(tr("First level")));
  model_->setHorizontalHeaderItem(2, new QStandardItem(tr("Second Level")));
  model_->setHorizontalHeaderItem(3, new QStandardItem(tr("Third Level")));
  ui_->list->setModel(model_);
  ui_->remove->setIcon(IconLoader::Load("edit-delete"));
  ui_->remove->setEnabled(false);

  ui_->remove->setShortcut(QKeySequence::Delete);
  connect(ui_->list->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), SLOT(UpdateButtonState()));

  connect(ui_->remove, SIGNAL(clicked()), SLOT(Remove()));
}

SavedGroupingManager::~SavedGroupingManager() {
  delete ui_;
  delete model_;
}

QString SavedGroupingManager::GroupByToString(const CollectionModel::GroupBy &g) {

  switch (g) {
    case CollectionModel::GroupBy_None:
    case CollectionModel::GroupByCount: {
      return tr("None");
    }
    case CollectionModel::GroupBy_AlbumArtist: {
      return tr("Album artist");
    }
    case CollectionModel::GroupBy_Artist: {
      return tr("Artist");
    }
    case CollectionModel::GroupBy_Album: {
      return tr("Album");
    }
    case CollectionModel::GroupBy_AlbumDisc: {
      return tr("Album - Disc");
    }
    case CollectionModel::GroupBy_YearAlbum: {
      return tr("Year - Album");
    }
    case CollectionModel::GroupBy_YearAlbumDisc: {
      return tr("Year - Album - Disc");
    }
    case CollectionModel::GroupBy_OriginalYearAlbum: {
      return tr("Original year - Album");
    }
    case CollectionModel::GroupBy_OriginalYearAlbumDisc: {
      return tr("Original year - Album - Disc");
    }
    case CollectionModel::GroupBy_Disc: {
      return tr("Disc");
    }
    case CollectionModel::GroupBy_Year: {
      return tr("Year");
    }
    case CollectionModel::GroupBy_OriginalYear: {
      return tr("Original year");
    }
    case CollectionModel::GroupBy_Genre: {
      return tr("Genre");
    }
    case CollectionModel::GroupBy_Composer: {
      return tr("Composer");
    }
    case CollectionModel::GroupBy_Performer: {
      return tr("Performer");
    }
    case CollectionModel::GroupBy_Grouping: {
      return tr("Grouping");
    }
    case CollectionModel::GroupBy_FileType: {
      return tr("File type");
    }
    case CollectionModel::GroupBy_Format: {
      return tr("Format");
    }
    case CollectionModel::GroupBy_Samplerate: {
      return tr("Sample rate");
    }
    case CollectionModel::GroupBy_Bitdepth: {
      return tr("Bit depth");
    }
    case CollectionModel::GroupBy_Bitrate: {
      return tr("Bitrate");
    }
  }

  return tr("Unknown");

}

void SavedGroupingManager::UpdateModel() {

  model_->setRowCount(0);  // don't use clear, it deletes headers
  QSettings s;
  s.beginGroup(CollectionModel::kSavedGroupingsSettingsGroup);
  int version = s.value("version").toInt();
  if (version == 1) {
    QStringList saved = s.childKeys();
    for (int i = 0; i < saved.size(); ++i) {
      if (saved.at(i) == "version") continue;
      QByteArray bytes = s.value(saved.at(i)).toByteArray();
      QDataStream ds(&bytes, QIODevice::ReadOnly);
      CollectionModel::Grouping g;
      ds >> g;

      QList<QStandardItem*> list;
      list << new QStandardItem(saved.at(i))
           << new QStandardItem(GroupByToString(g.first))
           << new QStandardItem(GroupByToString(g.second))
           << new QStandardItem(GroupByToString(g.third));

      model_->appendRow(list);
    }
  }
  else {
    QStringList saved = s.childKeys();
    for (int i = 0; i < saved.size(); ++i) {
      if (saved.at(i) == "version") continue;
      s.remove(saved.at(i));
    }
  }
  s.endGroup();

}

void SavedGroupingManager::Remove() {

  if (ui_->list->selectionModel()->hasSelection()) {
    QSettings s;
    s.beginGroup(CollectionModel::kSavedGroupingsSettingsGroup);
    for (const QModelIndex &index : ui_->list->selectionModel()->selectedRows()) {
      if (index.isValid()) {
        qLog(Debug) << "Remove saved grouping: " << model_->item(index.row(), 0)->text();
        s.remove(model_->item(index.row(), 0)->text());
      }
    }
    s.endGroup();
  }
  UpdateModel();
  filter_->UpdateGroupByActions();

}

void SavedGroupingManager::UpdateButtonState() {

  if (ui_->list->selectionModel()->hasSelection()) {
    const QModelIndex current = ui_->list->selectionModel()->currentIndex();
    ui_->remove->setEnabled(current.isValid());
  }
  else {
    ui_->remove->setEnabled(false);
  }

}

