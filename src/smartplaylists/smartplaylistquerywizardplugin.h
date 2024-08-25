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

#ifndef SMARTPLAYLISTQUERYWIZARDPLUGIN_H
#define SMARTPLAYLISTQUERYWIZARDPLUGIN_H

#include "config.h"

#include <QString>

#include "core/scoped_ptr.h"
#include "core/shared_ptr.h"
#include "smartplaylistwizardplugin.h"
#include "smartplaylistsearch.h"

class QWizard;

class CollectionBackend;
class SmartPlaylistSearch;
class SmartPlaylistQueryWizardPluginSearchPage;
class Ui_SmartPlaylistQuerySortPage;

class SmartPlaylistQueryWizardPlugin : public SmartPlaylistWizardPlugin {
  Q_OBJECT

 public:
  explicit SmartPlaylistQueryWizardPlugin(Application *app, SharedPtr<CollectionBackend> collection_backend, QObject *parent);
  ~SmartPlaylistQueryWizardPlugin() override;

  PlaylistGenerator::Type type() const override { return PlaylistGenerator::Type::Query; }
  QString name() const override;
  QString description() const override;
  bool is_dynamic() const override { return true; }

  int CreatePages(QWizard *wizard, const int finish_page_id) override;
  void SetGenerator(PlaylistGeneratorPtr) override;
  PlaylistGeneratorPtr CreateGenerator() const override;

 public Q_SLOTS:
  void UpdateSortPreview();

 private Q_SLOTS:
  void AddSearchTerm();
  void RemoveSearchTerm();

  void SearchTypeChanged();

  void UpdateTermPreview();
  void UpdateSortOrder();

  void MoveTermListToBottom(const int min, const int max);

 private:
  SmartPlaylistSearch MakeSearch() const;

  ScopedPtr<Ui_SmartPlaylistQuerySortPage> sort_ui_;
  SmartPlaylistQueryWizardPluginSearchPage *search_page_;

  int previous_scrollarea_max_;
};

#endif  // SMARTPLAYLISTQUERYWIZARDPLUGIN_H
