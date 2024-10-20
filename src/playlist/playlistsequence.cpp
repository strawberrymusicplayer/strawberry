/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QWidget>
#include <QVariant>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QSize>
#include <QAction>
#include <QActionGroup>
#include <QToolButton>

#include "core/iconloader.h"
#include "core/settingsprovider.h"
#include "playlistsequence.h"
#include "ui_playlistsequence.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "PlaylistSequence";
}

PlaylistSequence::PlaylistSequence(QWidget *parent, SettingsProvider *settings)
    : QWidget(parent),
      ui_(new Ui_PlaylistSequence),
      settings_(settings ? settings : new DefaultSettingsProvider),
      repeat_menu_(new QMenu(this)),
      shuffle_menu_(new QMenu(this)),
      loading_(false),
      repeat_mode_(RepeatMode::Off),
      shuffle_mode_(ShuffleMode::Off) {

  ui_->setupUi(this);

  // Icons
  ui_->repeat->setIcon(AddDesaturatedIcon(IconLoader::Load(u"media-playlist-repeat"_s)));
  ui_->shuffle->setIcon(AddDesaturatedIcon(IconLoader::Load(u"media-playlist-shuffle"_s)));

  // Remove arrow indicators
  ui_->repeat->setStyleSheet(u"QToolButton::menu-indicator { image: none; }"_s);
  ui_->shuffle->setStyleSheet(u"QToolButton::menu-indicator { image: none; }"_s);

  settings_->set_group(kSettingsGroup);

  QActionGroup *repeat_group = new QActionGroup(this);
  repeat_group->addAction(ui_->action_repeat_off);
  repeat_group->addAction(ui_->action_repeat_track);
  repeat_group->addAction(ui_->action_repeat_album);
  repeat_group->addAction(ui_->action_repeat_playlist);
  repeat_group->addAction(ui_->action_repeat_onebyone);
  repeat_group->addAction(ui_->action_repeat_intro);
  repeat_menu_->addActions(repeat_group->actions());
  ui_->repeat->setMenu(repeat_menu_);

  QActionGroup *shuffle_group = new QActionGroup(this);
  shuffle_group->addAction(ui_->action_shuffle_off);
  shuffle_group->addAction(ui_->action_shuffle_all);
  shuffle_group->addAction(ui_->action_shuffle_inside_album);
  shuffle_group->addAction(ui_->action_shuffle_albums);
  shuffle_menu_->addActions(shuffle_group->actions());
  ui_->shuffle->setMenu(shuffle_menu_);

  QObject::connect(repeat_group, &QActionGroup::triggered, this, &PlaylistSequence::RepeatActionTriggered);
  QObject::connect(shuffle_group, &QActionGroup::triggered, this, &PlaylistSequence::ShuffleActionTriggered);

  Load();

}

PlaylistSequence::~PlaylistSequence() {
  delete ui_;
}

void PlaylistSequence::Load() {

  loading_ = true;  // Stops these setter functions calling Save()
  SetShuffleMode(static_cast<ShuffleMode>(settings_->value(u"shuffle_mode"_s, static_cast<int>(ShuffleMode::Off)).toInt()));
  SetRepeatMode(static_cast<RepeatMode>(settings_->value(u"repeat_mode"_s, static_cast<int>(RepeatMode::Off)).toInt()));
  loading_ = false;

}

void PlaylistSequence::Save() {

  if (loading_) return;

  settings_->setValue(u"shuffle_mode"_s, static_cast<int>(shuffle_mode_));
  settings_->setValue(u"repeat_mode"_s, static_cast<int>(repeat_mode_));

}

QIcon PlaylistSequence::AddDesaturatedIcon(const QIcon &icon) {

  QIcon ret;
  const QList<QSize> sizes = icon.availableSizes();
  for (const QSize &size : sizes) {
    QPixmap on(icon.pixmap(size));
    QPixmap off(DesaturatedPixmap(on));

    ret.addPixmap(off, QIcon::Normal, QIcon::Off);
    ret.addPixmap(on, QIcon::Normal, QIcon::On);
  }
  return ret;

}

QPixmap PlaylistSequence::DesaturatedPixmap(const QPixmap &pixmap) {

  QPixmap ret(pixmap.size());
  ret.fill(Qt::transparent);

  QPainter p(&ret);
  p.setOpacity(0.5);
  p.drawPixmap(0, 0, pixmap);
  p.end();

  return ret;

}

void PlaylistSequence::RepeatActionTriggered(QAction *action) {

  RepeatMode mode = RepeatMode::Off;
  if (action == ui_->action_repeat_track) mode = RepeatMode::Track;
  if (action == ui_->action_repeat_album) mode = RepeatMode::Album;
  if (action == ui_->action_repeat_playlist) mode = RepeatMode::Playlist;
  if (action == ui_->action_repeat_onebyone) mode = RepeatMode::OneByOne;
  if (action == ui_->action_repeat_intro) mode = RepeatMode::Intro;

  SetRepeatMode(mode);

}

void PlaylistSequence::ShuffleActionTriggered(QAction *action) {

  ShuffleMode mode = ShuffleMode::Off;
  if (action == ui_->action_shuffle_all) mode = ShuffleMode::All;
  if (action == ui_->action_shuffle_inside_album) mode = ShuffleMode::InsideAlbum;
  if (action == ui_->action_shuffle_albums) mode = ShuffleMode::Albums;

  SetShuffleMode(mode);

}

void PlaylistSequence::SetRepeatMode(const RepeatMode mode) {

  ui_->repeat->setChecked(mode != RepeatMode::Off);

  switch (mode) {
    case RepeatMode::Off:      ui_->action_repeat_off->setChecked(true);      break;
    case RepeatMode::Track:    ui_->action_repeat_track->setChecked(true);    break;
    case RepeatMode::Album:    ui_->action_repeat_album->setChecked(true);    break;
    case RepeatMode::Playlist: ui_->action_repeat_playlist->setChecked(true); break;
    case RepeatMode::OneByOne: ui_->action_repeat_onebyone->setChecked(true); break;
    case RepeatMode::Intro: ui_->action_repeat_intro->setChecked(true);       break;

  }

  if (mode != repeat_mode_) {
    repeat_mode_ = mode;
    Q_EMIT RepeatModeChanged(mode);
  }

  Save();

}

void PlaylistSequence::SetShuffleMode(const ShuffleMode mode) {

  ui_->shuffle->setChecked(mode != ShuffleMode::Off);

  switch (mode) {
    case ShuffleMode::Off:         ui_->action_shuffle_off->setChecked(true);          break;
    case ShuffleMode::All:         ui_->action_shuffle_all->setChecked(true);          break;
    case ShuffleMode::InsideAlbum: ui_->action_shuffle_inside_album->setChecked(true); break;
    case ShuffleMode::Albums:      ui_->action_shuffle_albums->setChecked(true);       break;
  }

  if (mode != shuffle_mode_) {
    shuffle_mode_ = mode;
    Q_EMIT ShuffleModeChanged(mode);
  }

  Save();

}

PlaylistSequence::ShuffleMode PlaylistSequence::shuffle_mode() const {
  return shuffle_mode_;
}

PlaylistSequence::RepeatMode PlaylistSequence::repeat_mode() const {
  return repeat_mode_;
}

// Called from global shortcut
void PlaylistSequence::CycleShuffleMode() {

  ShuffleMode mode = ShuffleMode::Off;
  // We cycle through the shuffle modes
  switch (shuffle_mode()) {
    case ShuffleMode::Off:         mode = ShuffleMode::All;           break;
    case ShuffleMode::All:         mode = ShuffleMode::InsideAlbum;   break;
    case ShuffleMode::InsideAlbum: mode = ShuffleMode::Albums;        break;
    case ShuffleMode::Albums: break;
  }

  SetShuffleMode(mode);

}

//called from global shortcut
void PlaylistSequence::CycleRepeatMode() {

  RepeatMode mode = RepeatMode::Off;
  //we cycle through the repeat modes
  switch (repeat_mode()) {
    case RepeatMode::Off:       mode = RepeatMode::Track;     break;
    case RepeatMode::Track:     mode = RepeatMode::Album;     break;
    case RepeatMode::Album:     mode = RepeatMode::Playlist;  break;
    case RepeatMode::Playlist:  mode = RepeatMode::OneByOne;  break;
    case RepeatMode::OneByOne:  mode = RepeatMode::Intro;     break;
    case RepeatMode::Intro:
      break;
  }

  SetRepeatMode(mode);

}
