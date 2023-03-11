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

#ifndef SMARTPLAYLISTWIZARD_H
#define SMARTPLAYLISTWIZARD_H

#include "config.h"

#include <QWizard>

#include "playlistgenerator_fwd.h"

class Application;
class CollectionBackend;
class SmartPlaylistWizardPlugin;

class SmartPlaylistWizard : public QWizard {
  Q_OBJECT

 public:
  explicit SmartPlaylistWizard(Application *app, CollectionBackend *collection_backend, QWidget *parent);
  ~SmartPlaylistWizard() override;

  void SetGenerator(PlaylistGeneratorPtr gen);
  PlaylistGeneratorPtr CreateGenerator() const;

 protected:
  void initializePage(const int id) override;

 private:
  class TypePage;
  class FinishPage;

  void AddPlugin(SmartPlaylistWizardPlugin *plugin);

 private slots:
  void TypeChanged(const int index);

 private:
  Application *app_;
  CollectionBackend *collection_backend_;
  TypePage *type_page_;
  FinishPage *finish_page_;
  int finish_id_;

  int type_index_;
  QList<SmartPlaylistWizardPlugin*> plugins_;

};

#endif  // SMARTPLAYLISTWIZARD_H
