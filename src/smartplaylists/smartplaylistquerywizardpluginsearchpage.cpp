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

#include <algorithm>

#include "smartplaylistquerywizardpluginsearchpage.h"
#include "smartplaylistsearchtermwidget.h"
#include "ui_smartplaylistquerysearchpage.h"

SmartPlaylistQueryWizardPluginSearchPage::SmartPlaylistQueryWizardPluginSearchPage(QWidget *parent)
    : QWizardPage(parent),
      layout_(nullptr),
      new_term_(nullptr),
      preview_(nullptr),
      ui_(new Ui_SmartPlaylistQuerySearchPage) {

  ui_->setupUi(this);

}

bool SmartPlaylistQueryWizardPluginSearchPage::isComplete() const {

  if (ui_->type->currentIndex() == 2) {  // All songs
    return true;
  }
  return !std::any_of(terms_.begin(), terms_.end(), [](SmartPlaylistSearchTermWidget *widget) { return !widget->Term().is_valid(); });

}
