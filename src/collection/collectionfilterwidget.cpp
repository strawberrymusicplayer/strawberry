/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QUrl>
#include <QRegularExpression>
#include <QInputDialog>
#include <QList>
#include <QTimer>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QKeyEvent>

#include "core/iconloader.h"
#include "core/logging.h"
#include "core/settings.h"
#include "collectionfilteroptions.h"
#include "collectionmodel.h"
#include "collectionfilter.h"
#include "filterparser/filterparser.h"
#include "savedgroupingmanager.h"
#include "collectionfilterwidget.h"
#include "groupbydialog.h"
#include "ui_collectionfilterwidget.h"
#include "widgets/searchfield.h"
#include "constants/collectionsettings.h"
#include "constants/appearancesettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kFilterDelay = 500;  // msec
}

CollectionFilterWidget::CollectionFilterWidget(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_CollectionFilterWidget),
      model_(nullptr),
      filter_(nullptr),
      group_by_dialog_(new GroupByDialog(this)),
      groupings_manager_(nullptr),
      filter_age_menu_(nullptr),
      group_by_menu_(nullptr),
      collection_menu_(nullptr),
      group_by_group_(nullptr),
      timer_filter_delay_(new QTimer(this)),
      filter_applies_to_model_(true),
      delay_behaviour_(DelayBehaviour::DelayedOnLargeLibraries) {

  ui_->setupUi(this);

  ui_->search_field->setToolTip(FilterParser::ToolTip());

  QObject::connect(ui_->search_field, &SearchField::returnPressed, this, &CollectionFilterWidget::ReturnPressed);
  QObject::connect(timer_filter_delay_, &QTimer::timeout, this, &CollectionFilterWidget::FilterDelayTimeout);

  timer_filter_delay_->setInterval(kFilterDelay);
  timer_filter_delay_->setSingleShot(true);

  // Icons
  ui_->options->setIcon(IconLoader::Load(u"configure"_s));

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

  filter_max_ages_[ui_->filter_age_all] = -1;
  filter_max_ages_[ui_->filter_age_today] = 60 * 60 * 24;
  filter_max_ages_[ui_->filter_age_week] = 60 * 60 * 24 * 7;
  filter_max_ages_[ui_->filter_age_month] = 60 * 60 * 24 * 30;
  filter_max_ages_[ui_->filter_age_three_months] = 60 * 60 * 24 * 30 * 3;
  filter_max_ages_[ui_->filter_age_year] = 60 * 60 * 24 * 365;

  group_by_menu_ = new QMenu(tr("Group by"), this);

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

  QObject::connect(ui_->search_field, &SearchField::textChanged, this, &CollectionFilterWidget::FilterTextChanged);
  QObject::connect(ui_->options, &QToolButton::clicked, ui_->options, &QToolButton::showMenu);

  ReloadSettings();

}

CollectionFilterWidget::~CollectionFilterWidget() { delete ui_; }

void CollectionFilterWidget::Init(CollectionModel *model, CollectionFilter *filter) {

  if (model_) {
    QObject::disconnect(model_, nullptr, this, nullptr);
    QObject::disconnect(model_, nullptr, group_by_dialog_, nullptr);
    QObject::disconnect(group_by_dialog_, nullptr, model_, nullptr);
    const QList<QAction*> actions = filter_max_ages_.keys();
    for (QAction *action : actions) {
      QObject::disconnect(action, &QAction::triggered, model_, nullptr);
    }
  }

  model_ = model;
  filter_ = filter;

  // Connect signals
  QObject::connect(model_, &CollectionModel::GroupingChanged, group_by_dialog_, &GroupByDialog::CollectionGroupingChanged);
  QObject::connect(model_, &CollectionModel::GroupingChanged, this, &CollectionFilterWidget::GroupingChanged);
  QObject::connect(group_by_dialog_, &GroupByDialog::Accepted, model_, &CollectionModel::SetGroupBy);

  const QList<QAction*> actions = filter_max_ages_.keys();
  for (QAction *action : actions) {
    const int filter_max_age = filter_max_ages_.value(action);
    QObject::connect(action, &QAction::triggered, this, [this, filter_max_age]() { model_->SetFilterMaxAge(filter_max_age); } );
  }

  // Load settings
  if (!settings_group_.isEmpty()) {
    Settings s;
    s.beginGroup(settings_group_);
    int version = 0;
    if (s.contains(group_by_version())) version = s.value(group_by_version(), 0).toInt();
    if (version == 1) {
      model_->SetGroupBy(CollectionModel::Grouping(
        static_cast<CollectionModel::GroupBy>(s.value(group_by_key(1), static_cast<int>(CollectionModel::GroupBy::AlbumArtist)).toInt()),
        static_cast<CollectionModel::GroupBy>(s.value(group_by_key(2), static_cast<int>(CollectionModel::GroupBy::AlbumDisc)).toInt()),
        static_cast<CollectionModel::GroupBy>(s.value(group_by_key(3), static_cast<int>(CollectionModel::GroupBy::None)).toInt())),
        s.value(separate_albums_by_grouping_key(), false).toBool());
    }
    else {
      model_->SetGroupBy(CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::AlbumDisc, CollectionModel::GroupBy::None), false);
    }
    s.endGroup();
  }

}

void CollectionFilterWidget::SetSettingsGroup(const QString &settings_group) {

  settings_group_ = settings_group;
  saved_groupings_settings_group_ = SavedGroupingManager::GetSavedGroupingsSettingsGroup(settings_group);

  UpdateGroupByActions();

}

void CollectionFilterWidget::SetSettingsPrefix(const QString &prefix) {

  settings_prefix_ = prefix;

}

void CollectionFilterWidget::setFilter(CollectionFilter *filter) {
  filter_ = filter;
}

void CollectionFilterWidget::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  int iconsize = s.value(AppearanceSettings::kIconSizeConfigureButtons, 20).toInt();
  s.endGroup();
  ui_->options->setIconSize(QSize(iconsize, iconsize));
  ui_->search_field->setIconSize(iconsize);

}

QString CollectionFilterWidget::group_by_version() const {

  if (settings_prefix_.isEmpty()) {
    return u"group_by_version"_s;
  }

  return QStringLiteral("%1_group_by_version").arg(settings_prefix_);

}

QString CollectionFilterWidget::group_by_key() const {

  if (settings_prefix_.isEmpty()) {
    return u"group_by"_s;
  }

  return QStringLiteral("%1_group_by").arg(settings_prefix_);

}

QString CollectionFilterWidget::group_by_key(const int number) const { return group_by_key() + QString::number(number); }

QString CollectionFilterWidget::separate_albums_by_grouping_key() const {

  if (settings_prefix_.isEmpty()) {
    return u"separate_albums_by_grouping"_s;
  }

  return QStringLiteral("%1_separate_albums_by_grouping").arg(settings_prefix_);

}

void CollectionFilterWidget::UpdateGroupByActions() {

  if (group_by_group_) {
    QObject::disconnect(group_by_group_, nullptr, this, nullptr);
    delete group_by_group_;
  }

  group_by_group_ = CreateGroupByActions(saved_groupings_settings_group_, this);
  group_by_menu_->clear();
  group_by_menu_->addActions(group_by_group_->actions());
  QObject::connect(group_by_group_, &QActionGroup::triggered, this, &CollectionFilterWidget::GroupByClicked);
  if (model_) {
    CheckCurrentGrouping(model_->GetGroupBy());
  }

}

QActionGroup *CollectionFilterWidget::CreateGroupByActions(const QString &saved_groupings_settings_group, QObject *parent) {

  QActionGroup *ret = new QActionGroup(parent);

  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::AlbumDisc)));
  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Year - Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::YearAlbum)));
  ret->addAction(CreateGroupByAction(tr("Group by Album artist/Year - Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::YearAlbumDisc)));

  ret->addAction(CreateGroupByAction(tr("Group by Artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Artist, CollectionModel::GroupBy::Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist/Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Artist, CollectionModel::GroupBy::AlbumDisc)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist/Year - Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Artist, CollectionModel::GroupBy::YearAlbum)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist/Year - Album - Disc"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Artist, CollectionModel::GroupBy::YearAlbumDisc)));

  ret->addAction(CreateGroupByAction(tr("Group by Genre/Album artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Genre, CollectionModel::GroupBy::AlbumArtist, CollectionModel::GroupBy::Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Genre/Artist/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Genre, CollectionModel::GroupBy::Artist, CollectionModel::GroupBy::Album)));

  ret->addAction(CreateGroupByAction(tr("Group by Album Artist"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::AlbumArtist)));
  ret->addAction(CreateGroupByAction(tr("Group by Artist"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Artist)));

  ret->addAction(CreateGroupByAction(tr("Group by Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Album)));
  ret->addAction(CreateGroupByAction(tr("Group by Genre/Album"), parent, CollectionModel::Grouping(CollectionModel::GroupBy::Genre, CollectionModel::GroupBy::Album)));

  QAction *sep1 = new QAction(parent);
  sep1->setSeparator(true);
  ret->addAction(sep1);

  // Read saved groupings
  Settings s;
  s.beginGroup(saved_groupings_settings_group);
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
      ret->addAction(CreateGroupByAction(QUrl::fromPercentEncoding(name.toUtf8()), parent, g));
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

  QAction *sep2 = new QAction(parent);
  sep2->setSeparator(true);
  ret->addAction(sep2);

  ret->addAction(CreateGroupByAction(tr("Advanced grouping..."), parent, CollectionModel::Grouping()));

  return ret;

}

QAction *CollectionFilterWidget::CreateGroupByAction(const QString &text, QObject *parent, const CollectionModel::Grouping grouping) {

  QAction *ret = new QAction(text, parent);
  ret->setCheckable(true);

  if (grouping.first != CollectionModel::GroupBy::None) {
    ret->setProperty("group_by", QVariant::fromValue(grouping));
  }

  return ret;

}

void CollectionFilterWidget::SaveGroupBy() {

  if (!model_) return;

  const QString name = QInputDialog::getText(this, tr("Grouping Name"), tr("Grouping name:"));
  if (name.isEmpty()) return;

  qLog(Debug) << "Saving current grouping to" << name;

  Settings s;
  if (settings_group_.isEmpty() || settings_group_ == QLatin1String(CollectionSettings::kSettingsGroup)) {
    s.beginGroup(SavedGroupingManager::kSavedGroupingsSettingsGroup);
  }
  else {
    s.beginGroup(QLatin1String(SavedGroupingManager::kSavedGroupingsSettingsGroup) + QLatin1Char('_') + settings_group_);
  }
  QByteArray buffer;
  QDataStream datastream(&buffer, QIODevice::WriteOnly);
  datastream << model_->GetGroupBy();
  s.setValue("version", u"1"_s);
  s.setValue(QUrl::toPercentEncoding(name), buffer);
  s.endGroup();

  UpdateGroupByActions();

}

void CollectionFilterWidget::ShowGroupingManager() {

  if (!groupings_manager_) {
    groupings_manager_ = new SavedGroupingManager(saved_groupings_settings_group_, this);
    QObject::connect(groupings_manager_, &SavedGroupingManager::UpdateGroupByActions, this, &CollectionFilterWidget::UpdateGroupByActions);
  }

  groupings_manager_->UpdateModel();
  groupings_manager_->show();

}

bool CollectionFilterWidget::SearchFieldHasFocus() const {

  return ui_->search_field->hasFocus();

}

void CollectionFilterWidget::FocusSearchField() {

  ui_->search_field->setFocus();

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

void CollectionFilterWidget::GroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping) {

  if (!settings_group_.isEmpty()) {
    Settings s;
    s.beginGroup(settings_group_);
    s.setValue(group_by_version(), 1);
    s.setValue(group_by_key(1), static_cast<int>(g[0]));
    s.setValue(group_by_key(2), static_cast<int>(g[1]));
    s.setValue(group_by_key(3), static_cast<int>(g[2]));
    s.setValue(separate_albums_by_grouping_key(), separate_albums_by_grouping);
    s.endGroup();
  }

  // Now make sure the correct action is checked
  CheckCurrentGrouping(g);

}

void CollectionFilterWidget::CheckCurrentGrouping(const CollectionModel::Grouping g) {

  if (!group_by_group_) {
    UpdateGroupByActions();
  }

  const QList<QAction*> actions = group_by_group_->actions();
  for (QAction *action : actions) {
    if (action->property("group_by").isNull()) continue;

    if (g == action->property("group_by").value<CollectionModel::Grouping>()) {
      action->setChecked(true);
      return;
    }
  }

  // Check the advanced action
  QAction *action = actions.last();
  action->setChecked(true);

}

void CollectionFilterWidget::SetFilterHint(const QString &hint) {
  ui_->search_field->setPlaceholderText(hint);
}

void CollectionFilterWidget::SetFilterMode(CollectionFilterOptions::FilterMode filter_mode) {

  ui_->search_field->clear();
  ui_->search_field->setEnabled(filter_mode == CollectionFilterOptions::FilterMode::All);

  model_->SetFilterMode(filter_mode);

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
      Q_EMIT UpPressed();
      e->accept();
      break;

    case Qt::Key_Down:
      Q_EMIT DownPressed();
      e->accept();
      break;

    case Qt::Key_Escape:
      ui_->search_field->clear();
      e->accept();
      break;

    default:
      break;
  }

  QWidget::keyReleaseEvent(e);

}

void CollectionFilterWidget::FilterTextChanged(const QString &text) {

  const bool delay = (delay_behaviour_ == DelayBehaviour::AlwaysDelayed) || (delay_behaviour_ == DelayBehaviour::DelayedOnLargeLibraries && !text.isEmpty() && text.length() < 3 && model_->total_song_count() >= 100000);

  if (delay) {
    timer_filter_delay_->start();
  }
  else {
    timer_filter_delay_->stop();
    FilterDelayTimeout();
  }

}

void CollectionFilterWidget::FilterDelayTimeout() {

  if (filter_applies_to_model_) {
    filter_->SetFilterString(ui_->search_field->text());
  }

}
