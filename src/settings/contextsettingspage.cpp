/*
 * Strawberry Music Player
 * This code was part of Clementine.
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

#include <QtGlobal>
#include <QAction>
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

#include "constants/mainwindowsettings.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/contextsettings.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "contextsettingspage.h"
#include "ui_contextsettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace ContextSettings;

ContextSettingsPage::ContextSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_ContextSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"view-choose"_s, true, 0, 32));

  checkboxes_[QLatin1String(kAlbum)] = ui_->checkbox_album;
  checkboxes_[QLatin1String(kTechnicalData)] = ui_->checkbox_technical_data;
  checkboxes_[QLatin1String(kSongLyrics)] = ui_->checkbox_song_lyrics;
  checkboxes_[QLatin1String(kSearchCover)] = ui_->checkbox_search_cover;
  checkboxes_[QLatin1String(kSearchLyrics)] = ui_->checkbox_search_lyrics;

  // Create and populate the helper menus
  QMenu *menu = new QMenu(this);
  menu->addAction(ui_->action_title);
  menu->addAction(ui_->action_titlesort);
  menu->addAction(ui_->action_album);
  menu->addAction(ui_->action_albumsort);
  menu->addAction(ui_->action_artist);
  menu->addAction(ui_->action_artistsort);
  menu->addAction(ui_->action_albumartist);
  menu->addAction(ui_->action_albumartistsort);
  menu->addAction(ui_->action_track);
  menu->addAction(ui_->action_disc);
  menu->addAction(ui_->action_year);
  menu->addAction(ui_->action_originalyear);
  menu->addAction(ui_->action_composer);
  menu->addAction(ui_->action_composersort);
  menu->addAction(ui_->action_performer);
  menu->addAction(ui_->action_performersort);
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
  QObject::connect(menu, &QMenu::hovered, this, &ContextSettingsPage::ShowMenuTooltip);

  QObject::connect(ui_->context_exp_chooser1, &QToolButton::triggered, this, &ContextSettingsPage::InsertVariableFirstLine);
  QObject::connect(ui_->context_exp_chooser2, &QToolButton::triggered, this, &ContextSettingsPage::InsertVariableSecondLine);

  // Icons
  ui_->context_exp_chooser1->setIcon(IconLoader::Load(u"list-add"_s));
  ui_->context_exp_chooser2->setIcon(IconLoader::Load(u"list-add"_s));

  QObject::connect(ui_->font_headline, &QFontComboBox::currentFontChanged, this, &ContextSettingsPage::HeadlineFontChanged);
  QObject::connect(ui_->font_size_headline, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ContextSettingsPage::HeadlineFontChanged);
  QObject::connect(ui_->font_normal, &QFontComboBox::currentFontChanged, this, &ContextSettingsPage::NormalFontChanged);
  QObject::connect(ui_->font_size_normal, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ContextSettingsPage::NormalFontChanged);

  QFile file(u":/text/ghosts.txt"_s);
  if (file.open(QIODevice::ReadOnly)) {
    QString text = QString::fromUtf8(file.readAll());
    ui_->preview_headline->setText(text);
    ui_->preview_normal->setText(text);
    file.close();
  }
  else {
    qLog(Error) << "Could not open" << file.fileName() << "for reading:" << file.errorString();
  }

}

ContextSettingsPage::~ContextSettingsPage() { delete ui_; }

void ContextSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  ui_->context_custom_text1->setText(s.value(kSettingsTitleFmt, u"%title% - %artist%"_s).toString());
  ui_->context_custom_text2->setText(s.value(kSettingsSummaryFmt, u"%album%"_s).toString());

  for (const QString &i : checkboxes_.keys()) {
    checkboxes_[i]->setChecked(s.value(i, checkboxes_[i]->isChecked()).toBool());
  }

  // Fonts
  QString default_font;
  int i = ui_->font_headline->findText(QLatin1String(kDefaultFontFamily));
  if (i >= 0) {
    default_font = QLatin1String(kDefaultFontFamily);
  }
  else {
    default_font = font().family();
  }
  ui_->font_headline->setCurrentFont(s.value(kFontHeadline, default_font).toString());
  ui_->font_normal->setCurrentFont(s.value(kFontNormal, default_font).toString());
  ui_->font_size_headline->setValue(s.value(kFontSizeHeadline, kDefaultFontSizeHeadline).toReal());
  ui_->font_size_normal->setValue(s.value(kFontSizeNormal, font().pointSizeF()).toReal());

  s.endGroup();

  s.beginGroup(MainWindowSettings::kSettingsGroup);
  ui_->checkbox_search_cover->setChecked(s.value(MainWindowSettings::kSearchForCoverAuto, true).toBool());
  s.endGroup();

  Init(ui_->layout_contextsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void ContextSettingsPage::Save() {

  Settings s;

  s.beginGroup(kSettingsGroup);
  s.setValue(kSettingsTitleFmt, ui_->context_custom_text1->text());
  s.setValue(kSettingsSummaryFmt, ui_->context_custom_text2->text());
  for (const QString &i : checkboxes_.keys()) {
    s.setValue(i, checkboxes_[i]->isChecked());
  }
  s.setValue(kFontHeadline, ui_->font_headline->currentFont().family());
  s.setValue(kFontNormal, ui_->font_normal->currentFont().family());
  s.setValue(kFontSizeHeadline, ui_->font_size_headline->value());
  s.setValue(kFontSizeNormal, ui_->font_size_normal->value());
  s.endGroup();

  s.beginGroup(MainWindowSettings::kSettingsGroup);
  s.setValue(MainWindowSettings::kSearchForCoverAuto, ui_->checkbox_search_cover->isChecked());
  s.endGroup();

}

void ContextSettingsPage::InsertVariableFirstLine(QAction *action) {
  // We use action name, therefore those shouldn't be translatable
  ui_->context_custom_text1->insert(action->text());
}

void ContextSettingsPage::InsertVariableSecondLine(QAction *action) {
  // We use action name, therefore those shouldn't be translatable
  ui_->context_custom_text2->insert(action->text());
}

void ContextSettingsPage::ShowMenuTooltip(QAction *action) {
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
