/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QMenu>
#include <QSettings>
#include <QShowEvent>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "core/settings.h"
#include "collection/collectionbackend.h"
#include "settings/appearancesettingspage.h"

#include "smartplaylistsviewcontainer.h"
#include "smartplaylistsmodel.h"
#include "smartplaylistsview.h"
#include "smartplaylistwizard.h"
#include "playlistgenerator_fwd.h"

#include "ui_smartplaylistsviewcontainer.h"

SmartPlaylistsViewContainer::SmartPlaylistsViewContainer(Application *app, QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_SmartPlaylistsViewContainer),
      app_(app),
      context_menu_(new QMenu(this)),
      context_menu_selected_(new QMenu(this)),
      action_new_smart_playlist_(nullptr),
      action_edit_smart_playlist_(nullptr),
      action_delete_smart_playlist_(nullptr),
      action_append_to_playlist_(nullptr),
      action_replace_current_playlist_(nullptr),
      action_open_in_new_playlist_(nullptr),
      action_add_to_playlist_enqueue_(nullptr),
      action_add_to_playlist_enqueue_next_(nullptr) {

  ui_->setupUi(this);

  model_ = new SmartPlaylistsModel(app_->collection_backend(), this);
  ui_->view->setModel(model_);

  model_->Init();

  action_new_smart_playlist_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("document-new")), tr("New smart playlist..."), this, &SmartPlaylistsViewContainer::NewSmartPlaylist);

  action_append_to_playlist_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("media-playback-start")), tr("Append to current playlist"), this, &SmartPlaylistsViewContainer::AppendToPlaylist);
  action_replace_current_playlist_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("media-playback-start")), tr("Replace current playlist"), this, &SmartPlaylistsViewContainer::ReplaceCurrentPlaylist);
  action_open_in_new_playlist_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("document-new")), tr("Open in new playlist"), this, &SmartPlaylistsViewContainer::OpenInNewPlaylist);

  context_menu_selected_->addSeparator();
  action_add_to_playlist_enqueue_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("go-next")), tr("Queue track"), this, &SmartPlaylistsViewContainer::AddToPlaylistEnqueue);
  action_add_to_playlist_enqueue_next_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("go-next")), tr("Play next"), this, &SmartPlaylistsViewContainer::AddToPlaylistEnqueueNext);
  context_menu_selected_->addSeparator();

  context_menu_selected_->addSeparator();
  context_menu_selected_->addActions(QList<QAction*>() << action_new_smart_playlist_);
  action_edit_smart_playlist_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("edit-rename")), tr("Edit smart playlist..."), this, &SmartPlaylistsViewContainer::EditSmartPlaylistFromContext);
  action_delete_smart_playlist_ = context_menu_selected_->addAction(IconLoader::Load(QStringLiteral("edit-delete")), tr("Delete smart playlist"), this, &SmartPlaylistsViewContainer::DeleteSmartPlaylistFromContext);

  context_menu_selected_->addSeparator();

  ui_->new_->setDefaultAction(action_new_smart_playlist_);
  ui_->edit_->setIcon(IconLoader::Load(QStringLiteral("edit-rename")));
  ui_->delete_->setIcon(IconLoader::Load(QStringLiteral("edit-delete")));

  QObject::connect(ui_->edit_, &QToolButton::clicked, this, &SmartPlaylistsViewContainer::EditSmartPlaylistFromButton);
  QObject::connect(ui_->delete_, &QToolButton::clicked, this, &SmartPlaylistsViewContainer::DeleteSmartPlaylistFromButton);

  QObject::connect(ui_->view, &SmartPlaylistsView::ItemsSelectedChanged, this, &SmartPlaylistsViewContainer::ItemsSelectedChanged);
  QObject::connect(ui_->view, &SmartPlaylistsView::doubleClicked, this, &SmartPlaylistsViewContainer::ItemDoubleClicked);
  QObject::connect(ui_->view, &SmartPlaylistsView::RightClicked, this, &SmartPlaylistsViewContainer::RightClicked);

  ReloadSettings();

  ItemsSelectedChanged();

}

SmartPlaylistsViewContainer::~SmartPlaylistsViewContainer() { delete ui_; }

SmartPlaylistsView *SmartPlaylistsViewContainer::view() const { return ui_->view; }

void SmartPlaylistsViewContainer::showEvent(QShowEvent *e) {

  ItemsSelectedChanged();

  QWidget::showEvent(e);

}

void SmartPlaylistsViewContainer::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  int iconsize = s.value(AppearanceSettingsPage::kIconSizeLeftPanelButtons, 22).toInt();
  s.endGroup();

  ui_->new_->setIconSize(QSize(iconsize, iconsize));
  ui_->delete_->setIconSize(QSize(iconsize, iconsize));
  ui_->edit_->setIconSize(QSize(iconsize, iconsize));

}

void SmartPlaylistsViewContainer::ItemsSelectedChanged() {

  ui_->edit_->setEnabled(ui_->view->selectionModel()->selectedRows().count() > 0);
  ui_->delete_->setEnabled(ui_->view->selectionModel()->selectedRows().count() > 0);

}

void SmartPlaylistsViewContainer::RightClicked(const QPoint global_pos, const QModelIndex &idx) {

  context_menu_index_ = idx;
  if (context_menu_index_.isValid()) {
    context_menu_selected_->popup(global_pos);
  }
  else {
    context_menu_->popup(global_pos);
  }

}

void SmartPlaylistsViewContainer::ReplaceCurrentPlaylist() {

  QMimeData *q_mimedata = ui_->view->model()->mimeData(ui_->view->selectionModel()->selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  Q_EMIT AddToPlaylist(q_mimedata);

}

void SmartPlaylistsViewContainer::AppendToPlaylist() {

  Q_EMIT AddToPlaylist(ui_->view->model()->mimeData(ui_->view->selectionModel()->selectedIndexes()));

}

void SmartPlaylistsViewContainer::OpenInNewPlaylist() {

  QMimeData *q_mimedata = ui_->view->model()->mimeData(ui_->view->selectionModel()->selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  Q_EMIT AddToPlaylist(q_mimedata);

}

void SmartPlaylistsViewContainer::AddToPlaylistEnqueue() {

  QMimeData *q_mimedata = ui_->view->model()->mimeData(ui_->view->selectionModel()->selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_now_ = true;
  }
  Q_EMIT AddToPlaylist(q_mimedata);

}

void SmartPlaylistsViewContainer::AddToPlaylistEnqueueNext() {

  QMimeData *q_mimedata = ui_->view->model()->mimeData(ui_->view->selectionModel()->selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_next_now_ = true;
  }
  Q_EMIT AddToPlaylist(q_mimedata);

}

void SmartPlaylistsViewContainer::NewSmartPlaylist() {

  SmartPlaylistWizard *wizard = new SmartPlaylistWizard(app_, app_->collection_backend(), this);
  wizard->setAttribute(Qt::WA_DeleteOnClose);
  QObject::connect(wizard, &SmartPlaylistWizard::accepted, this, &SmartPlaylistsViewContainer::NewSmartPlaylistFinished);

  wizard->show();

}

void SmartPlaylistsViewContainer::EditSmartPlaylist(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  SmartPlaylistWizard *wizard = new SmartPlaylistWizard(app_, app_->collection_backend(), this);
  wizard->setAttribute(Qt::WA_DeleteOnClose);
  QObject::connect(wizard, &SmartPlaylistWizard::accepted, this, &SmartPlaylistsViewContainer::EditSmartPlaylistFinished);

  wizard->show();
  wizard->SetGenerator(model_->CreateGenerator(idx));

}

void SmartPlaylistsViewContainer::EditSmartPlaylistFromContext() {

  if (!context_menu_index_.isValid()) return;

  EditSmartPlaylist(context_menu_index_);

}

void SmartPlaylistsViewContainer::EditSmartPlaylistFromButton() {

  if (ui_->view->selectionModel()->selectedIndexes().count() == 0) return;

  EditSmartPlaylist(ui_->view->selectionModel()->selectedIndexes().first());

}

void SmartPlaylistsViewContainer::DeleteSmartPlaylist(const QModelIndex &idx) {

  if (!idx.isValid()) return;
  model_->DeleteGenerator(idx);

}

void SmartPlaylistsViewContainer::DeleteSmartPlaylistFromContext() {

  if (!context_menu_index_.isValid()) return;
  DeleteSmartPlaylist(context_menu_index_);

}

void SmartPlaylistsViewContainer::DeleteSmartPlaylistFromButton() {

  if (ui_->view->selectionModel()->selectedIndexes().count() == 0) return;

  DeleteSmartPlaylist(ui_->view->selectionModel()->selectedIndexes().first());

}

void SmartPlaylistsViewContainer::NewSmartPlaylistFinished() {

  SmartPlaylistWizard *wizard = qobject_cast<SmartPlaylistWizard*>(sender());
  if (!wizard) return;
  QObject::disconnect(wizard, &SmartPlaylistWizard::accepted, this, &SmartPlaylistsViewContainer::NewSmartPlaylistFinished);
  model_->AddGenerator(wizard->CreateGenerator());

}

void SmartPlaylistsViewContainer::EditSmartPlaylistFinished() {

  if (!context_menu_index_.isValid()) return;

  const SmartPlaylistWizard *wizard = qobject_cast<SmartPlaylistWizard*>(sender());
  if (!wizard) return;

  QObject::disconnect(wizard, &SmartPlaylistWizard::accepted, this, &SmartPlaylistsViewContainer::EditSmartPlaylistFinished);

  model_->UpdateGenerator(context_menu_index_, wizard->CreateGenerator());

}

void SmartPlaylistsViewContainer::ItemDoubleClicked(const QModelIndex &idx) {

  QMimeData *q_mimedata = ui_->view->model()->mimeData(QModelIndexList() << idx);
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->from_doubleclick_ = true;
  }
  Q_EMIT AddToPlaylist(q_mimedata);

}
