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
#ifdef Q_OS_ANDROID
#  include <QHash>
#  include <QPointer>
class QAbstractButton;
class QComboBox;
class QStackedWidget;
#endif

#include "includes/shared_ptr.h"

#include "playlistgenerator_fwd.h"

class Player;
class PlaylistManager;
class CollectionBackend;
class CurrentAlbumCoverLoader;
class SmartPlaylistWizardPlugin;
class SmartPlaylistWizardTypePage;
class SmartPlaylistWizardFinishPage;

#ifdef HAVE_MOODBAR
class MoodbarLoader;
#endif

class SmartPlaylistWizard : public QWizard {
  Q_OBJECT

 public:
  explicit SmartPlaylistWizard(const SharedPtr<Player> player,
                               const SharedPtr<PlaylistManager> playlist_manager,
                               const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
                               const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
                               const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                               QWidget *parent);

  ~SmartPlaylistWizard() override;

  void SetGenerator(PlaylistGeneratorPtr gen);
  PlaylistGeneratorPtr CreateGenerator() const;
#ifdef Q_OS_ANDROID
  void SetupAndroid(QStackedWidget *pages);
  void AdaptAndroidControls(QWidget *content);
#endif

 protected:
  void initializePage(const int id) override;
#ifdef Q_OS_ANDROID
  bool eventFilter(QObject *object, QEvent *event) override;
#endif

 private:
  void AddPlugin(SmartPlaylistWizardPlugin *plugin);
#ifdef Q_OS_ANDROID
  void AndroidBack();
  void CleanupAndroid();
  void OpenAndroidChoices(QComboBox *combo, const QString &title);
  QStackedWidget *android_pages_ = nullptr;
  QPointer<QWidget> android_choices_;
  QHash<QObject*, QPointer<QAbstractButton>> android_value_buttons_;
#endif

 private Q_SLOTS:
  void TypeChanged(const int index);

 private:
  const SharedPtr<CollectionBackend> collection_backend_;
  SmartPlaylistWizardTypePage *type_page_;
  SmartPlaylistWizardFinishPage *finish_page_;
  int finish_id_;

  int type_index_;
  QList<SmartPlaylistWizardPlugin*> plugins_;
};

#endif  // SMARTPLAYLISTWIZARD_H
