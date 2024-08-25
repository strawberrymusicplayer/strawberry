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

#include "smartplaylistquerywizardplugin.h"
#include "smartplaylistquerywizardpluginsortpage.h"

SmartPlaylistQueryWizardPluginSortPage::SmartPlaylistQueryWizardPluginSortPage(SmartPlaylistQueryWizardPlugin *plugin, QWidget *parent, const int next_id)
      : QWizardPage(parent), next_id_(next_id), plugin_(plugin) {}

void SmartPlaylistQueryWizardPluginSortPage::showEvent(QShowEvent *e) {

  Q_UNUSED(e)

  plugin_->UpdateSortPreview();

}

int SmartPlaylistQueryWizardPluginSortPage::nextId() const { return next_id_; }
