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

#include "config.h"

#include <QWizard>
#include <QWizardPage>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QStyle>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/iconloader.h"

#include "smartplaylistquerywizardplugin.h"
#include "smartplaylistwizard.h"
#include "smartplaylistwizardplugin.h"
#include "smartplaylistwizardtypepage.h"
#include "smartplaylistwizardfinishpage.h"
#include "ui_smartplaylistwizardfinishpage.h"

using namespace Qt::Literals::StringLiterals;

SmartPlaylistWizard::SmartPlaylistWizard(const SharedPtr<Player> player,
                                         const SharedPtr<PlaylistManager> playlist_manager,
                                         const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
                                         const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
                                         const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                                         QWidget *parent)
    : QWizard(parent),
      collection_backend_(collection_backend),
      type_page_(new SmartPlaylistWizardTypePage(this)),
      finish_page_(new SmartPlaylistWizardFinishPage(this)),
      type_index_(-1) {

  setWindowIcon(IconLoader::Load(u"strawberry"_s));
  setWindowTitle(tr("Smart playlist"));

  resize(788, 628);

#ifdef Q_OS_MACOS
  // MacStyle leaves an ugly empty space on the left side of the dialog.
  setWizardStyle(QWizard::ClassicStyle);
#endif
#ifdef Q_OS_WIN32
  // Workaround QTBUG-123853
  if (QApplication::style() && QApplication::style()->objectName() != u"windowsvista"_s) {
    setWizardStyle(QWizard::ClassicStyle);
  }
#endif

  // Type page
  type_page_->setTitle(tr("Playlist type"));
  type_page_->setSubTitle(tr("A smart playlist is a dynamic list of songs that come from your collection.  There are different types of smart playlist that offer different ways of selecting songs."));
  type_page_->setStyleSheet(u"QRadioButton { font-weight: bold; } QLabel { margin-bottom: 1em; margin-left: 24px; }"_s);
  addPage(type_page_);

  // Finish page
  finish_page_->setTitle(tr("Finish"));
  finish_page_->setSubTitle(tr("Choose a name for your smart playlist"));
  finish_id_ = addPage(finish_page_);

  new QVBoxLayout(type_page_);
  AddPlugin(new SmartPlaylistQueryWizardPlugin(player,
                                               playlist_manager,
                                               collection_backend,
#ifdef HAVE_MOODBAR
                                               moodbar_loader,
#endif
                                               current_albumcover_loader,
                                               this));

  // Skip the type page - remove this when we have more than one type
  setStartId(2);

}

SmartPlaylistWizard::~SmartPlaylistWizard() {
  qDeleteAll(plugins_);
}

void SmartPlaylistWizard::SetGenerator(PlaylistGeneratorPtr gen) {

  // Find the right type and jump to the start page
  for (int i = 0; i < plugins_.count(); ++i) {
    if (plugins_.value(i)->type() == gen->type()) {
      TypeChanged(i);
      // TODO: Put this back in when the setStartId is removed from the ctor next();
      break;
    }
  }

  if (type_index_ == -1) {
    qLog(Error) << "Plugin was not found for generator type" << static_cast<int>(gen->type());
    return;
  }

  // Set the name
  if (!gen->name().isEmpty()) {
    setWindowTitle(windowTitle() + u" - "_s + gen->name());
  }
  finish_page_->ui_->name->setText(gen->name());
  finish_page_->ui_->dynamic->setChecked(gen->is_dynamic());

  // Tell the plugin to load
  SmartPlaylistWizardPlugin *plugin = plugins_.value(type_index_);
  plugin->SetGenerator(gen);

}

void SmartPlaylistWizard::AddPlugin(SmartPlaylistWizardPlugin *plugin) {

  const int index = static_cast<int>(plugins_.count());
  plugins_ << plugin;
  plugin->Init(this, finish_id_);

  // Create the radio button
  QRadioButton *radio_button = new QRadioButton(plugin->name(), type_page_);
  QLabel *description = new QLabel(plugin->description(), type_page_);
  type_page_->layout()->addWidget(radio_button);
  type_page_->layout()->addWidget(description);

  QObject::connect(radio_button, &QRadioButton::clicked, this, [this, index]() { TypeChanged(index); });

  if (index == 0) {
    radio_button->setChecked(true);
    TypeChanged(0);
  }

}

void SmartPlaylistWizard::TypeChanged(const int index) {

  type_index_ = index;
  type_page_->next_id_ = plugins_.value(type_index_)->start_page();

}

PlaylistGeneratorPtr SmartPlaylistWizard::CreateGenerator() const {

  PlaylistGeneratorPtr ret;
  if (type_index_ == -1) return ret;

  ret = plugins_[type_index_]->CreateGenerator();
  if (!ret) return ret;

  ret->set_name(finish_page_->ui_->name->text());
  ret->set_dynamic(finish_page_->ui_->dynamic->isChecked());
  return ret;

}

void SmartPlaylistWizard::initializePage(const int id) {

  if (id == finish_id_) {
    finish_page_->ui_->dynamic_container->setEnabled(plugins_.value(type_index_)->is_dynamic());
  }
  QWizard::initializePage(id);

}
