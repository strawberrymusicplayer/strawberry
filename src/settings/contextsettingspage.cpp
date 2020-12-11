/*
 * Strawberry Music Player
 * This code was part of Clementine.
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
#include <QIODevice>
#include <QFile>
#include <QFont>
#include <QMenu>
#include <QCursor>
#include <QCheckBox>
#include <QToolButton>
#include <QToolTip>
#include <QLineEdit>
#include <QTextEdit>
#include <QFontComboBox>
#include <QSettings>

#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "contextsettingspage.h"
#include "ui_contextsettingspage.h"

const char *ContextSettingsPage::kSettingsGroup = "Context";
const char *ContextSettingsPage::kSettingsTitleFmt = "TitleFmt";
const char *ContextSettingsPage::kSettingsSummaryFmt = "SummaryFmt";

const char *ContextSettingsPage::kSettingsGroupEnable[ContextSettingsOrder::NELEMS] = {
  "AlbumEnable",
  "EngineAndDeviceEnable",
  "TechnicalDataEnable",
  "AlbumsByArtistEnable",
  "SongLyricsEnable",
  "SearchCoverEnable",
  "SearchLyricsEnable",
};

const qreal ContextSettingsPage::kDefaultFontSizeHeadline = 11;

ContextSettingsPage::ContextSettingsPage(SettingsDialog* dialog) : SettingsPage(dialog), ui_(new Ui_ContextSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("view-choose"));

  checkboxes_[ContextSettingsOrder::ALBUM] = ui_->checkbox_album;
  checkboxes_[ContextSettingsOrder::ENGINE_AND_DEVICE] = ui_->checkbox_engine_device;
  checkboxes_[ContextSettingsOrder::TECHNICAL_DATA] = ui_->checkbox_technical_data;
  checkboxes_[ContextSettingsOrder::ALBUMS_BY_ARTIST] = ui_->checkbox_albums;
  checkboxes_[ContextSettingsOrder::SONG_LYRICS] = ui_->checkbox_song_lyrics;
  checkboxes_[ContextSettingsOrder::SEARCH_COVER] = ui_->checkbox_search_cover;
  checkboxes_[ContextSettingsOrder::SEARCH_LYRICS] = ui_->checkbox_search_lyrics;

  // Create and populate the helper menus
  QMenu *menu = new QMenu(this);
  menu->addAction(ui_->action_title);
  menu->addAction(ui_->action_album);
  menu->addAction(ui_->action_artist);
  menu->addAction(ui_->action_albumartist);
  menu->addAction(ui_->action_track);
  menu->addAction(ui_->action_disc);
  menu->addAction(ui_->action_year);
  menu->addAction(ui_->action_originalyear);
  menu->addAction(ui_->action_composer);
  menu->addAction(ui_->action_performer);
  menu->addAction(ui_->action_grouping);
  menu->addAction(ui_->action_filename);
  menu->addAction(ui_->action_url);
  menu->addAction(ui_->action_length);
  menu->addAction(ui_->action_genre);
  menu->addAction(ui_->action_playcount);
  menu->addAction(ui_->action_skipcount);
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

  connect(ui_->font_headline, SIGNAL(currentFontChanged(QFont)), SLOT(HeadlineFontChanged()));
  connect(ui_->font_size_headline, SIGNAL(valueChanged(double)), SLOT(HeadlineFontChanged()));
  connect(ui_->font_normal, SIGNAL(currentFontChanged(QFont)), SLOT(NormalFontChanged()));
  connect(ui_->font_size_normal, SIGNAL(valueChanged(double)), SLOT(NormalFontChanged()));

  QFile file(":/text/ghosts.txt");
  if (file.open(QIODevice::ReadOnly)) {
    QString text = file.readAll();
    ui_->preview_headline->setText(text);
    ui_->preview_normal->setText(text);
  }

}

ContextSettingsPage::~ContextSettingsPage() { delete ui_; }

void ContextSettingsPage::Load() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  ui_->context_custom_text1->setText(s.value(kSettingsTitleFmt, "%title% - %artist%").toString());
  ui_->context_custom_text2->setText(s.value(kSettingsSummaryFmt, "%album%").toString());

  for (int i = 0 ; i < ContextSettingsOrder::NELEMS ; ++i) {
    checkboxes_[i]->setChecked(s.value(kSettingsGroupEnable[i], checkboxes_[i]->isChecked()).toBool());
  }

  // Fonts
  QString default_font;
  int i = ui_->font_headline->findText("Noto Sans");
  if (i >= 0) {
    default_font = "Noto Sans";
  }
  else {
    default_font = QWidget().font().family();
  }
  ui_->font_headline->setCurrentFont(s.value("font_headline", default_font).toString());
  ui_->font_normal->setCurrentFont(s.value("font_normal", default_font).toString());
  ui_->font_size_headline->setValue(s.value("font_size_headline", kDefaultFontSizeHeadline).toReal());
  ui_->font_size_normal->setValue(s.value("font_size_normal", font().pointSizeF()).toReal());

  s.endGroup();

  s.beginGroup(MainWindow::kSettingsGroup);
  ui_->checkbox_search_cover->setChecked(s.value("search_for_cover_auto", true).toBool());
  s.endGroup();

  Init(ui_->layout_contextsettingspage->parentWidget());

  if (!QSettings().childGroups().contains(kSettingsGroup)) set_changed();

}

void ContextSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue(kSettingsTitleFmt, ui_->context_custom_text1->text());
  s.setValue(kSettingsSummaryFmt, ui_->context_custom_text2->text());
  for (int i = 0; i < ContextSettingsOrder::NELEMS; ++i) {
    s.setValue(kSettingsGroupEnable[i], checkboxes_[i]->isChecked());
  }
  s.setValue("font_headline", ui_->font_headline->currentFont().family());
  s.setValue("font_normal", ui_->font_normal->currentFont().family());
  s.setValue("font_size_headline", ui_->font_size_headline->value());
  s.setValue("font_size_normal", ui_->font_size_normal->value());
  s.endGroup();

  s.beginGroup(MainWindow::kSettingsGroup);
  s.setValue("search_for_cover_auto", ui_->checkbox_search_cover->isChecked());
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

void ContextSettingsPage::HeadlineFontChanged() {

  QFont font(ui_->font_headline->currentFont());
  if (ui_->font_size_headline->value() > 0) {
    font.setPointSizeF(ui_->font_size_headline->value());
  }
  ui_->preview_headline->setFont(font);

}

void ContextSettingsPage::NormalFontChanged() {

  QFont font(ui_->font_normal->currentFont());
  if (ui_->font_size_normal->value() > 0) {
    font.setPointSizeF(ui_->font_size_normal->value());
  }
  ui_->preview_normal->setFont(font);

}
