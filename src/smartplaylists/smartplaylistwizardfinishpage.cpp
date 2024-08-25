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

#include "smartplaylistwizardfinishpage.h"
#include "ui_smartplaylistwizardfinishpage.h"

SmartPlaylistWizardFinishPage::SmartPlaylistWizardFinishPage(QWidget *parent) : QWizardPage(parent), ui_(new Ui_SmartPlaylistWizardFinishPage) {

  ui_->setupUi(this);
  QObject::connect(ui_->name, &QLineEdit::textChanged, this, &SmartPlaylistWizardFinishPage::completeChanged);

}

SmartPlaylistWizardFinishPage::~SmartPlaylistWizardFinishPage() { delete ui_; }

int SmartPlaylistWizardFinishPage::nextId() const { return -1; }
bool SmartPlaylistWizardFinishPage::isComplete() const { return !ui_->name->text().isEmpty(); }
