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

#include <memory>

#include <QApplication>
#include <QWidget>
#include <QObject>
#include <QDataStream>
#include <QIODevice>
#include <QAction>
#include <QActionGroup>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QInputDialog>
#include <QList>
#include <QTimer>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QtEvents>

#include "core/iconloader.h"
#include "core/song.h"
#include "core/logging.h"
#include "collectionmodel.h"
#include "collectionfilter.h"
#include "collectionquery.h"
#include "savedgroupingmanager.h"
#include "collectionfilterwidget.h"
#include "groupbydialog.h"
#include "ui_collectionfilterwidget.h"
#include "widgets/qsearchfield.h"
#include "settings/appearancesettingspage.h"

CollectionFilterWidget::CollectionFilterWidget(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_CollectionFilterWidget),
      model_(nullptr),
      filter_(nullptr),
      group_by_dialog_(new GroupByDialog),
      filter_delay_(new QTimer(this)),
      filter_applies_to_model_(true),
      delay_behaviour_(DelayedOnLargeLibraries) {

  ui_->setupUi(this);

  QString available_fields = Song::kSearchColumns.join(", ");

  ui_->search_field->setToolTip(
  QString("<html><head/><body><p>") +
  tr("Prefix a word with a field name to limit the search to that field, e.g.:") +
  QString(" ") +
  QString("<span style=\"font-weight:600;\">") +
  tr("artist") +
  QString(":") +
  QString("</span><span style=\"font-style:italic;\">Strawbs</span>") +
  QString(" ") +
  tr("searches the collection for all artists that contain the word") +
  QString(" Strawbs.") +
  QString("</p><p><span style=\"font-weight:600;\">") +
  tr("Available fields") +
  QString(": ") +
  "</span><span style=\"font-style:italic;\">" +
  available_fields +
  QString("</span>.") +
  QString("</p></body></html>")
  );

  QObject::connect(ui_->search_field, &QSearchField::returnPressed, this, &CollectionFilterWidget::ReturnPressed);
  QObject::connect(filter_delay_, &QTimer::timeout, this, &CollectionFilterWidget::FilterDelayTimeout);

  filter_delay_->setInterval(kFilterDelay);
  filter_delay_->setSingleShot(true);

  // Icons
  ui_->options->setIcon(IconLoader::Load("configure"));

  // Filter by age
  QActionGroup *filter_age_group = new QActionGroup(this);
  filter_age_group->addAction(ui_->filter_age_all);
  filter_age_group->addAction(ui_->filter_age_today);
  filter_age_group->addAction(ui_->filter_age_week);
  filter_age_group->addAction(ui_->filter_age_month);
  filter_age_group->addAction(ui_->filter_age_three_months);
  filter_age_group->addAction(ui_->filter_age_year);

  filter_age_menu_ = new QMenu(tr("Show"), this);
  filter_age_menu_->addActions(filter_age_group->actions());

  filter_ages_[ui_->filter_age_all] = -1;
  filter_ages_[ui_->filter_age_today] = 60 * 60 * 24;
  filter_ages_[ui_->filter_age_week] = 60 * 60 * 24 * 7;
  filter_ages_[ui_->filter_age_month] = 60 * 60 * 24 * 30;
  filter_ages_[ui_->filter_age_three_months] = 60 * 60 * 24 * 30 * 3;
  filter_ages_[ui_->filter_age_year] = 60 * 60 * 24 * 365;

  // "Group by ..."
  group_by_group_ = CreateGroupByActions(this);

  group_by_menu_ = new QMenu(tr("Group by"), this);
  group_by_menu_->addActions(group_by_group_->actions());

  QObject::connect(group_by_group_, &QActionGroup::triggered, this, &CollectionFilterWidget::GroupByClicked);
  QObject::connect(ui_->save_grouping, &QAction::triggered, this, &CollectionFilterWidget::SaveGroupBy);
  QObject::connect(ui_->manage_groupings, &QAction::triggered, this, &CollectionFilterWidget::ShowGroupingManager);

  // Collection config menu
  collection_menu_ = new QMenu(tr("Display options"), this);
  collection_menu_->setIcon(ui_->options->icon());
  collection_menu_->addMenu(filter_age_menu_);
  collection_menu_->addMenu(group_by_menu_);
  collection_menu_->addAction(ui_->save_grouping);
  collection_menu_->addAction(ui_->manage_groupings);
  collection_menu_->addSeparator();
  ui_->options->setMenu(collection_menu_);

  QObject::connect(ui_->search_field, &QSearchField::textChanged, this, &CollectionFilterWidget::FilterTextChanged);

  ReloadSettings();

}

CollectionFilterWidget::~CollectionFilterWidget() { delete ui_; }

void CollectionFilterWidget::Init(CollectionModel *model, CollectionFilter *filter) {

  if (model_) {
    QObject::disconnect(model_, nullptr, this, nullptr);
    QObject::disconnect(model_, nullptr, group_by_dialog_.get(), nullptr);
    QObject::disconnect(group_by_dialog_.get(), nullptr, model_, nullptr);
    QList<QAction*> filter_ages = filter_ages_.keys();
    for (QAction *action : filter_ages) {
      QObject::disconnect(action, &QAction::triggered, model_, nullptr);
    }
  }

  model_ = model;
  filter_ = filter;

  // Connect signals
  QObject::connect(model_, &CollectionModel::GroupingChanged, group_by_dialog_.get(), &GroupByDialog::CollectionGroupingChanged);
  QObject::connect(model_, &CollectionModel::GroupingChanged, this, &CollectionFilterWidget::GroupingChanged);
  QObject::connect(group_by_dialog_.get(), &GroupByDialog::Accepted, model_, &CollectionModel::SetGroupBy);

  QList<QAction*> filter_ages = filter_ages_.keys();
  for (QAction *action : filter_ages) {
    int age = filter_ages_[action];
    QObject::connect(action, &QAction::triggered, [this, age]() { model_->SetFilterAge(age); } );
  }

  // Load settings
  if (!settings_group_.isEmpty()) {
    QSettings s;
    s.beginGroup(settings_group_);
    int version = 0;
    if (s.contains(group_by_version())) version = s.value(group_by_version(), 0).toInt();
    if (version == 1) {
      model_->SetGroupBy(CollectionModel::Grouping(
          CollectionModel::GroupBy(s.value(group_by(1), int(CollectionModel::GroupBy_AlbumArtist)).toInt()),
          CollectionModel::GroupBy(s.value(group_by(2), int(CollectionModel::GroupBy_AlbumDisc)).toInt()),
          CollectionModel::GroupBy(s.value(group_by(3), int(CollectionModel::GroupBy_None)).toInt())));
    }
    else {
      model_->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy_AlbumArtist, CollectionModel::GroupBy_AlbumDisc, CollectionModel::GroupBy_None));
    }
    s.endGroup();
  }

}

void CollectionFilterWidget::setFilter(CollectionFilter *filter) {
  filter_ = filter;
}

void CollectionFilterWidget::ReloadSettings() {

  QSettings s;
  s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  int iconsize = s.value(AppearanceSettingsPage::kIconSizeConfigureButtons, 20).toInt();
  s.endGroup();
  ui_->options->setIconSize(QSize(iconsize, iconsize));
  ui_->search_field->setIconSize(iconsize);

}

QString CollectionFilterWidget::group_by() {

  if (settings_prefix_.isEmpty()) {
    return QString("group_by");
  }
  else {
    return QString("%1_group_by").arg(settings_prefix_);
  }

}

QString CollectionFilterWidget::group_by_version() {

  if (settings_prefix_.isEmpty()) {
    return QString("group_by_version");
  }
  else {
    return QString("%1_group_by_version").arg(settings_prefix_);
  }

}

QString CollectionFilterWidget::group_by(const int number) { return group_by() + QString::number(number); }

void CollectionFilterWidget::UpdateGroupByActions() {

  if (group_by_group_) {
    QObject::disconnect(group_by_group_, nullptr, this, nullptr);
    delete group_by_group_;
  }

  group_by_group_ = CreateGroupByActions(this);
  group_by_menu_->clear();
  group_by_menu_->addActions(group_by_group_->actions());
  QObject::connect(group_by_group_, &QActionGroup::triggered, this, &CollectionFilterWidget::GroupByClicked);
  if (model_) {
    CheckCurrentGrouping(model_->GetGroupBy());
  }

}


QActionGroup *CollectionFilterWidget::CreateGroupByActions(QObject *parent) {

  QActionGroup *ret = new QActionGroup(parent);

  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_AlbumArtist, CollectionModel::GroupBy_Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_AlbumArtist, CollectionModel::GroupBy_AlbumDisc)));
  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Year - Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_AlbumArtist, CollectionModel::GroupBy_YearAlbum)));
  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Year - Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_AlbumArtist, CollectionModel::GroupBy_YearAlbumDisc)));

  ret->addAction(CreateGroupByAction(tr("Group by Artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Artist, CollectionModel::GroupBy_Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist/Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Artist, CollectionModel::GroupBy_AlbumDisc)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist/Year - Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Artist, CollectionModel::GroupBy_YearAlbum)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist/Year - Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Artist, CollectionModel::GroupBy_YearAlbumDisc)));

  ret->addAction(CreateGroupByAction(tr("Group by Genre/Album artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Genre, CollectionModel::GroupBy_AlbumArtist, CollectionModel::GroupBy_Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Genre/Artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Genre, CollectionModel::GroupBy_Artist, CollectionModel::GroupBy_Album)));

  ret->addAction(CreateGroupByAction(tr("Group by Album Artist"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_AlbumArtist)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Artist)));

  ret->addAction(CreateGroupByAction(tr("Group by Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Genre/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy_Genre, CollectionModel::GroupBy_Album)));

  QAction *sep1 = new QAction(parent);
  sep1->setSeparator(true);
  ret->addAction(sep1);

  // read saved groupings
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
      ret->addAction(CreateGroupByAction(saved.at(i), parent, g));
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

  QAction *sep2 = new QAction(parent);
  sep2->setSeparator(true);
  ret->addAction(sep2);

  ret->addAction(CreateGroupByAction(tr("Advanced grouping..."), parent, CollectionModel::Grouping()));

  return ret;

}

QAction *CollectionFilterWidget::CreateGroupByAction(const QString &text, QObject *parent, const CollectionModel::Grouping grouping) {

  QAction *ret = new QAction(text, parent);
  ret->setCheckable(true);

  if (grouping.first != CollectionModel::GroupBy_None) {
    ret->setProperty("group_by", QVariant::fromValue(grouping));
  }

  return ret;

}

void CollectionFilterWidget::SaveGroupBy() {

  QString text = QInputDialog::getText(this, tr("Grouping Name"), tr("Grouping name:"));
  if (!text.isEmpty() && model_) {
    model_->SaveGrouping(text);
    UpdateGroupByActions();
  }

}

void CollectionFilterWidget::ShowGroupingManager() {

  if (!groupings_manager_) {
    groupings_manager_ = std::make_unique<SavedGroupingManager>();
  }
  groupings_manager_->SetFilter(this);
  groupings_manager_->UpdateModel();
  groupings_manager_->show();

}


void CollectionFilterWidget::FocusOnFilter(QKeyEvent *event) {

  ui_->search_field->setFocus();
  QApplication::sendEvent(ui_->search_field, event);

}

void CollectionFilterWidget::GroupByClicked(QAction *action) {

  if (action->property("group_by").isNull()) {
    group_by_dialog_->show();
    return;
  }

  CollectionModel::Grouping g = action->property("group_by").value<CollectionModel::Grouping>();
  model_->SetGroupBy(g);

}

void CollectionFilterWidget::GroupingChanged(const CollectionModel::Grouping g) {

  if (!settings_group_.isEmpty()) {
    // Save the settings
    QSettings s;
    s.beginGroup(settings_group_);
    s.setValue(group_by_version(), 1);
    s.setValue(group_by(1), int(g[0]));
    s.setValue(group_by(2), int(g[1]));
    s.setValue(group_by(3), int(g[2]));
    s.endGroup();
  }

  // Now make sure the correct action is checked
  CheckCurrentGrouping(g);

}

void CollectionFilterWidget::CheckCurrentGrouping(const CollectionModel::Grouping g) {

  for (QAction *action : group_by_group_->actions()) {
    if (action->property("group_by").isNull()) continue;

    if (g == action->property("group_by").value<CollectionModel::Grouping>()) {
      action->setChecked(true);
      return;
    }
  }

  // Check the advanced action
  QList<QAction*> actions = group_by_group_->actions();
  QAction *action = actions.last();
  action->setChecked(true);

}

void CollectionFilterWidget::SetFilterHint(const QString &hint) {
  ui_->search_field->setPlaceholderText(hint);
}

void CollectionFilterWidget::SetQueryMode(QueryOptions::QueryMode query_mode) {

  ui_->search_field->clear();
  ui_->search_field->setEnabled(query_mode == QueryOptions::QueryMode_All);

  model_->SetFilterQueryMode(query_mode);

}

void CollectionFilterWidget::ShowInCollection(const QString &search) {
  ui_->search_field->setText(search);
}

void CollectionFilterWidget::SetAgeFilterEnabled(bool enabled) {
  filter_age_menu_->setEnabled(enabled);
}

void CollectionFilterWidget::SetGroupByEnabled(bool enabled) {
  group_by_menu_->setEnabled(enabled);
}

void CollectionFilterWidget::AddMenuAction(QAction *action) {
  collection_menu_->addAction(action);
}

void CollectionFilterWidget::keyReleaseEvent(QKeyEvent *e) {

  switch (e->key()) {
    case Qt::Key_Up:
      emit UpPressed();
      e->accept();
      break;

    case Qt::Key_Down:
      emit DownPressed();
      e->accept();
      break;

    case Qt::Key_Escape:
      ui_->search_field->clear();
      e->accept();
      break;
  }

  QWidget::keyReleaseEvent(e);

}

void CollectionFilterWidget::FilterTextChanged(const QString &text) {

  const bool delay = (delay_behaviour_ == AlwaysDelayed) || (delay_behaviour_ == DelayedOnLargeLibraries && !text.isEmpty() && text.length() < 3 && model_->total_song_count() >= 100000);

  if (delay) {
    filter_delay_->start();
  }
  else {
    filter_delay_->stop();
    FilterDelayTimeout();
  }

}

void CollectionFilterWidget::FilterDelayTimeout() {

  if (filter_applies_to_model_) {
    filter_->setFilterFixedString(ui_->search_field->text());
  }

}
