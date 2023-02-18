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

#include <memory>

#include <QObject>
#include <QString>

#include "smartplaylistwizardplugin.h"
#include "smartplaylistsearch.h"

class QWizard;

class CollectionBackend;
class SmartPlaylistSearch;
class Ui_SmartPlaylistQuerySortPage;

class SmartPlaylistQueryWizardPlugin : public SmartPlaylistWizardPlugin {
  Q_OBJECT

 public:
  explicit SmartPlaylistQueryWizardPlugin(Application *app, CollectionBackend *collection, QObject *parent);
  ~SmartPlaylistQueryWizardPlugin() override;

  PlaylistGenerator::Type type() const override { return PlaylistGenerator::Type::Query; }
  QString name() const override;
  QString description() const override;
  bool is_dynamic() const override { return true; }

  int CreatePages(QWizard *wizard, const int finish_page_id) override;
  void SetGenerator(PlaylistGeneratorPtr) override;
  PlaylistGeneratorPtr CreateGenerator() const override;

 private slots:
  void AddSearchTerm();
  void RemoveSearchTerm();

  void SearchTypeChanged();

  void UpdateTermPreview();
  void UpdateSortPreview();
  void UpdateSortOrder();

  void MoveTermListToBottom(const int min, const int max);

 private:
  class SearchPage;
  class SortPage;

  SmartPlaylistSearch MakeSearch() const;

  std::unique_ptr<Ui_SmartPlaylistQuerySortPage> sort_ui_;
  SearchPage *search_page_;

  int previous_scrollarea_max_;
};

#endif  // SMARTPLAYLISTQUERYWIZARDPLUGIN_H
