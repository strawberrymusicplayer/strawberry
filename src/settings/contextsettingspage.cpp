/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QAction>
#include <QVariant>
#include <QMenu>
#include <QCursor>
#include <QCheckBox>
#include <QToolButton>
#include <QToolTip>
#include <QLineEdit>
#include <QSettings>

#include "core/iconloader.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "contextsettingspage.h"
#include "ui_contextsettingspage.h"

const char *ContextSettingsPage::kSettingsGroup = "Context";
const char *ContextSettingsPage::kSettingsTitleFmt = "TitleFmt";
const char *ContextSettingsPage::kSettingsSummaryFmt = "SummaryFmt";
const char *ContextSettingsPage::kSettingsGroupLabels[ContextSettingsOrder::NELEMS] = {
  "Technical Data",
  "Engine and Device",
  "Albums by Artist",
  "Song Lyrics",
  "Album",
};
const char *ContextSettingsPage::kSettingsGroupEnable[ContextSettingsOrder::NELEMS] = {
  "TechnicalDataEnable",
  "EngineAndDeviceEnable",
  "AlbumsByArtistEnable",
  "SongLyricsEnable",
  "AlbumEnable",
};

ContextSettingsPage::ContextSettingsPage(SettingsDialog* dialog) : SettingsPage(dialog), ui_(new Ui_ContextSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("view-choose"));

  checkboxes[ContextSettingsOrder::ALBUM] = ui_->context_item1_enable;
  checkboxes[ContextSettingsOrder::TECHNICAL_DATA] = ui_->context_item2_enable;
  checkboxes[ContextSettingsOrder::ENGINE_AND_DEVICE] = ui_->context_item3_enable;
  checkboxes[ContextSettingsOrder::ALBUMS_BY_ARTIST] = ui_->context_item4_enable;
  checkboxes[ContextSettingsOrder::SONG_LYRICS] = ui_->context_item5_enable;

  // Create and populate the helper menus
  QMenu *menu = new QMenu(this);
  menu->addAction(ui_->action_albumartist);
  menu->addAction(ui_->action_artist);
  menu->addAction(ui_->action_album);
  menu->addAction(ui_->action_title);
  menu->addAction(ui_->action_year);
  menu->addAction(ui_->action_composer);
  menu->addAction(ui_->action_performer);
  menu->addAction(ui_->action_grouping);
  menu->addAction(ui_->action_length);
  menu->addAction(ui_->action_disc);
  menu->addAction(ui_->action_track);
  menu->addAction(ui_->action_genre);
  menu->addAction(ui_->action_playcount);
  menu->addAction(ui_->action_skipcount);
  menu->addAction(ui_->action_filename);
  menu->addSeparator();
  menu->addAction(ui_->action_newline);
  ui_->context_exp_chooser1->setMenu(menu);
  ui_->context_exp_chooser2->setMenu(menu);
  ui_->context_exp_chooser1->setPopupMode(QToolButton::InstantPopup);
  ui_->context_exp_chooser2->setPopupMode(QToolButton::InstantPopup);
  // We need this because by default menus don't show tooltips
  connect(menu, SIGNAL(hovered(QAction*)), SLOT(ShowMenuTooltip(QAction*)));

  connect(ui_->context_exp_chooser1, SIGNAL(triggered(QAction*)), SLOT(InsertVariableFirstLine(QAction*)));
  connect(ui_->context_exp_chooser2, SIGNAL(triggered(QAction*)), SLOT(InsertVariableSecondLine(QAction*)));

  // Icons
  ui_->context_exp_chooser1->setIcon(IconLoader::Load("list-add"));
  ui_->context_exp_chooser2->setIcon(IconLoader::Load("list-add"));

}

ContextSettingsPage::~ContextSettingsPage() { delete ui_; }

void ContextSettingsPage::Load() {

  QSettings s;

  s.beginGroup(ContextSettingsPage::kSettingsGroup);
  ui_->context_custom_text1->setText(s.value(kSettingsTitleFmt, "%title% - %artist%").toString());
  ui_->context_custom_text2->setText(s.value(kSettingsSummaryFmt, "%album%").toString());
  for (int i = 0 ; i < ContextSettingsOrder::NELEMS ; ++i) {
    checkboxes[i]->setChecked(s.value(kSettingsGroupEnable[i], i != ContextSettingsOrder::ALBUMS_BY_ARTIST).toBool());
  }
  s.endGroup();

}

void ContextSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue(kSettingsTitleFmt, ui_->context_custom_text1->text());
  s.setValue(kSettingsSummaryFmt, ui_->context_custom_text2->text());
  for (int i = 0; i < ContextSettingsOrder::NELEMS; ++i) {
    s.setValue(kSettingsGroupEnable[i], checkboxes[i]->isChecked());
  }
  s.endGroup();

}

void ContextSettingsPage::InsertVariableFirstLine(QAction* action) {
  // We use action name, therefore those shouldn't be translatable
  ui_->context_custom_text1->insert(action->text());
}

void ContextSettingsPage::InsertVariableSecondLine(QAction* action) {
  // We use action name, therefore those shouldn't be translatable
  ui_->context_custom_text2->insert(action->text());
}

void ContextSettingsPage::ShowMenuTooltip(QAction* action) {
  QToolTip::showText(QCursor::pos(), action->toolTip());
}
