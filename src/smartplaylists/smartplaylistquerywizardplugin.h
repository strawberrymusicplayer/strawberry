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

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "smartplaylistwizardplugin.h"
#include "smartplaylistsearch.h"

class QWizard;

class Player;
class PlaylistManager;
class CollectionBackend;
class CurrentAlbumCoverLoader;
class SmartPlaylistSearch;
class SmartPlaylistQueryWizardPluginSearchPage;
class Ui_SmartPlaylistQuerySortPage;

#ifdef HAVE_MOODBAR
class MoodbarLoader;
#endif

class SmartPlaylistQueryWizardPlugin : public SmartPlaylistWizardPlugin {
  Q_OBJECT

 public:
  explicit SmartPlaylistQueryWizardPlugin(const SharedPtr<Player> player,
                                          const SharedPtr<PlaylistManager> playlist_manager,
                                          const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
                                          const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
                                          const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                          QObject *parent);

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
  const SharedPtr<Player> player_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  const SharedPtr<CollectionBackend> collection_backend_;
#ifdef HAVE_MOODBAR
  const SharedPtr<MoodbarLoader> moodbar_loader_;
#endif
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;

  SmartPlaylistSearch MakeSearch() const;

  ScopedPtr<Ui_SmartPlaylistQuerySortPage> sort_ui_;
  SmartPlaylistQueryWizardPluginSearchPage *search_page_;

  int previous_scrollarea_max_;
};

#endif  // SMARTPLAYLISTQUERYWIZARDPLUGIN_H
