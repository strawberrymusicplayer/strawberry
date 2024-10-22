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

#ifndef SMARTPLAYLISTWIZARDPLUGIN_H
#define SMARTPLAYLISTWIZARDPLUGIN_H

#include <QObject>

#include "includes/shared_ptr.h"
#include "playlistgenerator.h"

class QWizard;

class CollectionBackend;

class SmartPlaylistWizardPlugin : public QObject {
  Q_OBJECT

 public:
  explicit SmartPlaylistWizardPlugin(const SharedPtr<CollectionBackend> collection_backend, QObject *parent);

  virtual PlaylistGenerator::Type type() const = 0;
  virtual QString name() const = 0;
  virtual QString description() const = 0;
  virtual bool is_dynamic() const { return false; }
  int start_page() const { return start_page_; }

  virtual void SetGenerator(PlaylistGeneratorPtr gen) = 0;
  virtual PlaylistGeneratorPtr CreateGenerator() const = 0;

  void Init(QWizard *wizard, const int finish_page_id);

 protected:
  virtual int CreatePages(QWizard *wizard, const int finish_page_id) = 0;

  const SharedPtr<CollectionBackend> collection_backend_;

 private:
  int start_page_;
};

#endif  // SMARTPLAYLISTWIZARDPLUGIN_H
