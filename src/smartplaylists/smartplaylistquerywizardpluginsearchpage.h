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

#ifndef SMARTPLAYLISTQUERYWIZARDPLUGINSEARCHPAGE_H
#define SMARTPLAYLISTQUERYWIZARDPLUGINSEARCHPAGE_H

#include <QWizardPage>

#include "includes/scoped_ptr.h"

#include "ui_smartplaylistquerysearchpage.h"

class QVBoxLayout;

class SmartPlaylistSearchTermWidget;
class SmartPlaylistSearchPreview;

class SmartPlaylistQueryWizardPluginSearchPage : public QWizardPage {
  Q_OBJECT

  friend class SmartPlaylistQueryWizardPlugin;

 public:
  explicit SmartPlaylistQueryWizardPluginSearchPage(QWidget *parent = nullptr);

  bool isComplete() const override;

 private:
  QVBoxLayout *layout_;
  QList<SmartPlaylistSearchTermWidget*> terms_;
  SmartPlaylistSearchTermWidget *new_term_;

  SmartPlaylistSearchPreview *preview_;

  ScopedPtr<Ui_SmartPlaylistQuerySearchPage> ui_;
};

#endif  // SMARTPLAYLISTQUERYWIZARDPLUGINSEARCHPAGE_H
