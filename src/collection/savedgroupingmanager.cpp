/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2015, Nick Lanham <nick@afternight.org>
 * Copyright 2019-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QDialog>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QIODevice>
#include <QDataStream>
#include <QKeySequence>
#include <QPushButton>
#include <QSettings>

#include "core/logging.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/collectionsettings.h"
#include "collectionmodel.h"
#include "savedgroupingmanager.h"
#include "ui_savedgroupingmanager.h"

using namespace Qt::Literals::StringLiterals;

const char *SavedGroupingManager::kSavedGroupingsSettingsGroup = "SavedGroupings";

SavedGroupingManager::SavedGroupingManager(const QString &saved_groupings_settings_group, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_SavedGroupingManager),
      model_(new QStandardItemModel(0, 4, this)),
      saved_groupings_settings_group_(saved_groupings_settings_group) {

  ui_->setupUi(this);

  model_->setHorizontalHeaderItem(0, new QStandardItem(tr("Name")));
  model_->setHorizontalHeaderItem(1, new QStandardItem(tr("First level")));
  model_->setHorizontalHeaderItem(2, new QStandardItem(tr("Second Level")));
  model_->setHorizontalHeaderItem(3, new QStandardItem(tr("Third Level")));
  ui_->list->setModel(model_);
  ui_->remove->setIcon(IconLoader::Load(u"edit-delete"_s));
  ui_->remove->setEnabled(false);

  ui_->remove->setShortcut(QKeySequence::Delete);
  QObject::connect(ui_->list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SavedGroupingManager::UpdateButtonState);

  QObject::connect(ui_->remove, &QPushButton::clicked, this, &SavedGroupingManager::Remove);

}

SavedGroupingManager::~SavedGroupingManager() {
  delete ui_;
}

QString SavedGroupingManager::GetSavedGroupingsSettingsGroup(const QString &settings_group) {

  if (settings_group.isEmpty() || settings_group == QLatin1String(CollectionSettings::kSettingsGroup)) {
    return QLatin1String(kSavedGroupingsSettingsGroup);
  }

  return QLatin1String(kSavedGroupingsSettingsGroup) + QLatin1Char('_') + settings_group;

}

QString SavedGroupingManager::GroupByToString(const CollectionModel::GroupBy g) {

  switch (g) {
    case CollectionModel::GroupBy::None:
    case CollectionModel::GroupBy::GroupByCount:{
      return tr("None");
    }
    case CollectionModel::GroupBy::AlbumArtist:{
      return tr("Album artist");
    }
    case CollectionModel::GroupBy::Artist:{
      return tr("Artist");
    }
    case CollectionModel::GroupBy::Album:{
      return tr("Album");
    }
    case CollectionModel::GroupBy::AlbumDisc:{
      return tr("Album - Disc");
    }
    case CollectionModel::GroupBy::YearAlbum:{
      return tr("Year - Album");
    }
    case CollectionModel::GroupBy::YearAlbumDisc:{
      return tr("Year - Album - Disc");
    }
    case CollectionModel::GroupBy::OriginalYearAlbum:{
      return tr("Original year - Album");
    }
    case CollectionModel::GroupBy::OriginalYearAlbumDisc:{
      return tr("Original year - Album - Disc");
    }
    case CollectionModel::GroupBy::Disc:{
      return tr("Disc");
    }
    case CollectionModel::GroupBy::Year:{
      return tr("Year");
    }
    case CollectionModel::GroupBy::OriginalYear:{
      return tr("Original year");
    }
    case CollectionModel::GroupBy::Genre:{
      return tr("Genre");
    }
    case CollectionModel::GroupBy::Composer:{
      return tr("Composer");
    }
    case CollectionModel::GroupBy::Performer:{
      return tr("Performer");
    }
    case CollectionModel::GroupBy::Grouping:{
      return tr("Grouping");
    }
    case CollectionModel::GroupBy::FileType:{
      return tr("File type");
    }
    case CollectionModel::GroupBy::Format:{
      return tr("Format");
    }
    case CollectionModel::GroupBy::Samplerate:{
      return tr("Sample rate");
    }
    case CollectionModel::GroupBy::Bitdepth:{
      return tr("Bit depth");
    }
    case CollectionModel::GroupBy::Bitrate:{
      return tr("Bitrate");
    }
  }

  return tr("Unknown");

}

void SavedGroupingManager::UpdateModel() {

  model_->setRowCount(0);  // don't use clear, it deletes headers
  Settings s;
  s.beginGroup(saved_groupings_settings_group_);
  int version = s.value("version").toInt();
  if (version == 1) {
    QStringList saved = s.childKeys();
    for (int i = 0; i < saved.size(); ++i) {
      const QString &name = saved.at(i);
      if (name == "version"_L1) continue;
      QByteArray bytes = s.value(name).toByteArray();
      QDataStream ds(&bytes, QIODevice::ReadOnly);
      CollectionModel::Grouping g;
      ds >> g;

      QList<QStandardItem*> list;

      QStandardItem *item = new QStandardItem();
      item->setText(QUrl::fromPercentEncoding(name.toUtf8()));
      item->setData(name);

      list << item
           << new QStandardItem(GroupByToString(g.first))
           << new QStandardItem(GroupByToString(g.second))
           << new QStandardItem(GroupByToString(g.third));

      model_->appendRow(list);
    }
  }
  else {
    QStringList saved = s.childKeys();
    for (int i = 0; i < saved.size(); ++i) {
      const QString &name = saved.at(i);
      if (name == "version"_L1) continue;
      s.remove(name);
    }
  }
  s.endGroup();

}

void SavedGroupingManager::Remove() {

  if (ui_->list->selectionModel()->hasSelection()) {
    Settings s;
    s.beginGroup(saved_groupings_settings_group_);
    const QModelIndexList indexes = ui_->list->selectionModel()->selectedRows();
    for (const QModelIndex &idx : indexes) {
      if (idx.isValid()) {
        qLog(Debug) << "Remove saved grouping: " << model_->item(idx.row(), 0)->text();
        s.remove(model_->item(idx.row(), 0)->data().toString());
      }
    }
    s.endGroup();
  }
  UpdateModel();

  Q_EMIT UpdateGroupByActions();

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
