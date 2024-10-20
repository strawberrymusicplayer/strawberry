/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QWidget>
#include <QMimeData>
#include <QDesktopServices>
#include <QMenu>
#include <QAction>
#include <QShowEvent>
#include <QContextMenuEvent>

#include "core/mimedata.h"
#include "core/iconloader.h"
#include "radiomodel.h"
#include "radioview.h"
#include "radioservice.h"
#include "radiomimedata.h"
#include "collection/collectionitemdelegate.h"

using namespace Qt::Literals::StringLiterals;

RadioView::RadioView(QWidget *parent)
    : AutoExpandingTreeView(parent),
      menu_(nullptr),
      action_playlist_append_(nullptr),
      action_playlist_replace_(nullptr),
      action_playlist_new_(nullptr),
      action_homepage_(nullptr),
      action_donate_(nullptr),
      initialized_(false) {

  setItemDelegate(new CollectionItemDelegate(this));
  SetExpandOnReset(false);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setSelectionMode(QAbstractItemView::ExtendedSelection);

}

RadioView::~RadioView() { delete menu_; }

void RadioView::showEvent(QShowEvent *e) {

  Q_UNUSED(e)

  if (!initialized_) {
    Q_EMIT GetChannels();
    initialized_ = true;
  }

}

void RadioView::contextMenuEvent(QContextMenuEvent *e) {

  if (!menu_) {
    menu_ = new QMenu;

    action_playlist_append_ = new QAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this);
    QObject::connect(action_playlist_append_, &QAction::triggered, this, &RadioView::AddToPlaylist);
    menu_->addAction(action_playlist_append_);

    action_playlist_replace_ = new QAction(IconLoader::Load(u"media-playback-start"_s), tr("Replace current playlist"), this);
    QObject::connect(action_playlist_replace_, &QAction::triggered, this, &RadioView::ReplacePlaylist);
    menu_->addAction(action_playlist_replace_);

    action_playlist_new_ = new QAction(IconLoader::Load(u"document-new"_s), tr("Open in new playlist"), this);
    QObject::connect(action_playlist_new_, &QAction::triggered, this, &RadioView::OpenInNewPlaylist);
    menu_->addAction(action_playlist_new_);

    action_homepage_ = new QAction(IconLoader::Load(u"download"_s), tr("Open homepage"), this);
    QObject::connect(action_homepage_, &QAction::triggered, this, &RadioView::Homepage);
    menu_->addAction(action_homepage_);

    action_donate_ = new QAction(IconLoader::Load(u"download"_s), tr("Donate"), this);
    QObject::connect(action_donate_, &QAction::triggered, this, &RadioView::Donate);
    menu_->addAction(action_donate_);

    menu_->addAction(IconLoader::Load(u"view-refresh"_s), tr("Refresh channels"), this, &RadioView::GetChannels);
  }

  const bool channels_selected = !selectedIndexes().isEmpty();

  action_playlist_append_->setVisible(channels_selected);
  action_playlist_replace_->setVisible(channels_selected);
  action_playlist_new_->setVisible(channels_selected);
  action_homepage_->setVisible(channels_selected);
  action_donate_->setVisible(channels_selected);

  menu_->popup(e->globalPos());

}

void RadioView::AddToPlaylist() {

  const QModelIndexList selected_indexes = selectedIndexes();
  if (selected_indexes.isEmpty()) return;

  Q_EMIT AddToPlaylistSignal(model()->mimeData(selected_indexes));

}

void RadioView::ReplacePlaylist() {

  const QModelIndexList selected_indexes = selectedIndexes();
  if (selected_indexes.isEmpty()) return;

  QMimeData *qmimedata = model()->mimeData(selected_indexes);
  if (MimeData *mimedata = qobject_cast<MimeData*>(qmimedata)) {
    mimedata->clear_first_ = true;
  }

  Q_EMIT AddToPlaylistSignal(qmimedata);

}

void RadioView::OpenInNewPlaylist() {

  const QModelIndexList selected_indexes = selectedIndexes();
  if (selected_indexes.isEmpty()) return;

  QMimeData *qmimedata = model()->mimeData(selected_indexes);
  if (RadioMimeData *mimedata = qobject_cast<RadioMimeData*>(qmimedata)) {
    mimedata->open_in_new_playlist_ = true;
    if (!mimedata->songs.isEmpty()) {
      mimedata->name_for_new_playlist_ = mimedata->songs.first().title();
    }
  }

  Q_EMIT AddToPlaylistSignal(qmimedata);

}

void RadioView::Homepage() {

  const QModelIndexList selected_indexes = selectedIndexes();
  if (selected_indexes.isEmpty()) return;

  QList<QUrl> urls;
  for (const QModelIndex &idx : selected_indexes) {
    QUrl url = idx.data(RadioModel::Role_Homepage).toUrl();
    if (!urls.contains(url)) {
      urls << url;  // clazy:exclude=reserve-candidates
    }
  }

  for (const QUrl &url : std::as_const(urls)) {
    QDesktopServices::openUrl(url);
  }

}

void RadioView::Donate() {

  const QModelIndexList selected_indexes = selectedIndexes();
  if (selected_indexes.isEmpty()) return;

  QList<QUrl> urls;
  for (const QModelIndex &idx : selected_indexes) {
    QUrl url = idx.data(RadioModel::Role_Donate).toUrl();
    if (!urls.contains(url)) {
      urls << url;  // clazy:exclude=reserve-candidates
    }
  }

  for (const QUrl &url : std::as_const(urls)) {
    QDesktopServices::openUrl(url);
  }

}

