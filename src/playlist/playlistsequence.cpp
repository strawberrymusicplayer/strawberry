/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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
      shuffle_mode_(ShuffleMode::Off),
      half_playing_time_s_(0),
      percent_interest_song_(0) {

  ui_->setupUi(this);

  // Icons
  ui_->repeat->setIcon(AddDesaturatedIcon(IconLoader::Load(u"media-playlist-repeat"_s)));
  ui_->shuffle->setIcon(AddDesaturatedIcon(IconLoader::Load(u"media-playlist-shuffle"_s)));
  ui_->show_zapping_values->setIcon(AddDesaturatedIcon(IconLoader::Load(u"configure"_s)));
  const int base_icon_size = static_cast<int>(fontMetrics().height() * 1.2);
  ui_->repeat->setIconSize(QSize(base_icon_size, base_icon_size));
  ui_->shuffle->setIconSize(QSize(base_icon_size, base_icon_size));
  ui_->display_option->setIconSize(QSize(base_icon_size, base_icon_size));
  ui_->playing_time_before_or_after->setFixedHeight(base_icon_size * 1.2);
  ui_->playing_center_time->setFixedHeight(base_icon_size * 1.2);

  // Remove arrow indicators
  ui_->repeat->setStyleSheet(u"QToolButton::menu-indicator { image: none; }"_s);
  ui_->shuffle->setStyleSheet(u"QToolButton::menu-indicator { image: none; }"_s);

  settings_->set_group(kSettingsGroup);

  QActionGroup *zapping_position_group = new QActionGroup(this);
  zapping_position_group->addAction(ui_->action_zapping_intro_000);
  zapping_position_group->addAction(ui_->action_zapping_intro_010);
  zapping_position_group->addAction(ui_->action_zapping_intro_020);
  zapping_position_group->addAction(ui_->action_zapping_intro_033);
  zapping_position_group->addAction(ui_->action_zapping_intro_050);
  zapping_position_group->addAction(ui_->action_zapping_intro_066);
  zapping_position_group->addAction(ui_->action_zapping_intro_080);
  zapping_position_group->addAction(ui_->action_zapping_intro_090);
  zapping_position_group->addAction(ui_->action_zapping_intro_100);
  ui_->menu_param_zapping_intro_time->addActions(zapping_position_group->actions());

  QActionGroup *zapping_time_group = new QActionGroup(this);
  zapping_time_group->addAction(ui_->action_zapping_time_010);
  zapping_time_group->addAction(ui_->action_zapping_time_020);
  zapping_time_group->addAction(ui_->action_zapping_time_030);
  zapping_time_group->addAction(ui_->action_zapping_time_045);
  zapping_time_group->addAction(ui_->action_zapping_time_060);
  zapping_time_group->addAction(ui_->action_zapping_time_090);
  zapping_time_group->addAction(ui_->action_zapping_time_120);
  zapping_time_group->addAction(ui_->action_zapping_time_180);
  zapping_time_group->addAction(ui_->action_zapping_time_240);
  ui_->menu_param_zapping_playing_time->addActions(zapping_time_group->actions());

  QActionGroup *repeat_group = new QActionGroup(this);
  repeat_group->addAction(ui_->action_repeat_off);
  repeat_group->addAction(ui_->action_repeat_track);
  repeat_group->addAction(ui_->action_repeat_album);
  repeat_group->addAction(ui_->action_repeat_playlist);
  repeat_group->addAction(ui_->action_repeat_onebyone);
  repeat_group->addAction(ui_->action_repeat_zapping);
  repeat_group->addAction(ui_->action_param_zapping);
  repeat_menu_->addActions(repeat_group->actions());
  ui_->repeat->setMenu(repeat_menu_);
  ui_->action_param_zapping->setMenu(ui_->menu_param_zapping);

  QActionGroup *shuffle_group = new QActionGroup(this);
  shuffle_group->addAction(ui_->action_shuffle_off);
  shuffle_group->addAction(ui_->action_shuffle_all);
  shuffle_group->addAction(ui_->action_shuffle_inside_album);
  shuffle_group->addAction(ui_->action_shuffle_albums);
  shuffle_group->addAction(ui_->action_shuffle_grouping);
  shuffle_menu_->addActions(shuffle_group->actions());
  ui_->shuffle->setMenu(shuffle_menu_);

  ui_->playing_time_before_or_after->setToolTip(PlaylistSequence::ToolTipPlayingTime());
  ui_->playing_center_time->setToolTip(PlaylistSequence::ToolTipPositionTime());

  ui_->display_option->setDefaultAction(ui_->show_zapping_values);

  QObject::connect(repeat_group, &QActionGroup::triggered, this, &PlaylistSequence::RepeatActionTriggered);
  QObject::connect(shuffle_group, &QActionGroup::triggered, this, &PlaylistSequence::ShuffleActionTriggered);
  QObject::connect(ui_->menu_param_zapping_intro_time, &QMenu::triggered, this, &PlaylistSequence::UpdateActionPlayingPosition);
  QObject::connect(ui_->menu_param_zapping_playing_time, &QMenu::triggered, this, &PlaylistSequence::UpdateActionPlayingTime);
  QObject::connect(ui_->show_zapping_values, &QAction::triggered, this, &PlaylistSequence::DisplayPlayingOption);
  QObject::connect(ui_->playing_time_before_or_after, &QSpinBox::valueChanged, this, &PlaylistSequence::UpdatePlayingTime);
  QObject::connect(ui_->playing_center_time, &QSpinBox::valueChanged, this, &PlaylistSequence::UpdatePlayingPosition);

  // Default init on zapping parameters to check the menus
  loading_ = true;
  UpdatePlayingTime(20);
  UpdatePlayingPosition(0);
  loading_ = false;

  Load();

}

PlaylistSequence::~PlaylistSequence() {
  delete ui_;
}

void PlaylistSequence::Load() {

  loading_ = true;  // Stops these setter functions calling Save()
  SetShuffleMode(static_cast<ShuffleMode>(settings_->value(u"shuffle_mode"_s, static_cast<int>(ShuffleMode::Off)).toInt()));
  SetRepeatMode(static_cast<RepeatMode>(settings_->value(u"repeat_mode"_s, static_cast<int>(RepeatMode::Off)).toInt()));
  ui_->playing_time_before_or_after->setValue(settings_->value(u"half_playing_time_s"_s, 20).toInt());
  ui_->playing_center_time->setValue(settings_->value(u"percent_interest_song"_s, 0).toInt());
  loading_ = false;

}

void PlaylistSequence::Save() {

  if (loading_) return;

  settings_->setValue(u"shuffle_mode"_s, static_cast<int>(shuffle_mode_));
  settings_->setValue(u"repeat_mode"_s, static_cast<int>(repeat_mode_));
  settings_->setValue(u"half_playing_time_s"_s, half_playing_time_s_);
  settings_->setValue(u"percent_interest_song"_s, percent_interest_song_);

}

QIcon PlaylistSequence::AddDesaturatedIcon(const QIcon &icon) {

  const QList<QSize> icon_sizes = icon.availableSizes();
  if (icon_sizes.isEmpty()) {
    return icon;
  }

  QIcon new_icon;
  for (const QSize &icon_size : icon_sizes) {
    QPixmap on_pixmap(icon.pixmap(icon_size));
    QPixmap off_pixmap(DesaturatedPixmap(on_pixmap));
    new_icon.addPixmap(off_pixmap, QIcon::Normal, QIcon::Off);
    new_icon.addPixmap(on_pixmap, QIcon::Normal, QIcon::On);
  }

  return new_icon;

}

QPixmap PlaylistSequence::DesaturatedPixmap(const QPixmap &pixmap) {

  QPixmap ret(pixmap.size());
  ret.setDevicePixelRatio(pixmap.devicePixelRatio());
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
  if (action == ui_->action_repeat_zapping) mode = RepeatMode::Zapping;

  SetRepeatMode(mode);

}

void PlaylistSequence::ShuffleActionTriggered(QAction *action) {

  ShuffleMode mode = ShuffleMode::Off;
  if (action == ui_->action_shuffle_all) mode = ShuffleMode::All;
  if (action == ui_->action_shuffle_inside_album) mode = ShuffleMode::InsideAlbum;
  if (action == ui_->action_shuffle_albums) mode = ShuffleMode::Albums;
  if (action == ui_->action_shuffle_grouping) mode = ShuffleMode::Grouping;

  SetShuffleMode(mode);

}

void PlaylistSequence::UpdateActionPlayingTime(QAction *action) {

  int playing_time = 10;

  if (action == ui_->action_zapping_time_020) playing_time = 20;
  else if (action == ui_->action_zapping_time_030) playing_time = 30;
  else if (action == ui_->action_zapping_time_045) playing_time = 45;
  else if (action == ui_->action_zapping_time_060) playing_time = 60;
  else if (action == ui_->action_zapping_time_090) playing_time = 90;
  else if (action == ui_->action_zapping_time_120) playing_time = 120;
  else if (action == ui_->action_zapping_time_180) playing_time = 180;
  else if (action == ui_->action_zapping_time_240) playing_time = 240;

  ui_->playing_time_before_or_after->setValue(playing_time);

}

void PlaylistSequence::UpdateActionPlayingPosition(QAction *action) {

  int percent_time = 0;

  if (action == ui_->action_zapping_intro_010) percent_time = 10;
  else if (action == ui_->action_zapping_intro_020) percent_time = 20;
  else if (action == ui_->action_zapping_intro_033) percent_time = 33;
  else if (action == ui_->action_zapping_intro_050) percent_time = 50;
  else if (action == ui_->action_zapping_intro_066) percent_time = 66;
  else if (action == ui_->action_zapping_intro_080) percent_time = 80;
  else if (action == ui_->action_zapping_intro_090) percent_time = 90;
  else if (action == ui_->action_zapping_intro_100) percent_time = 100;

  ui_->playing_center_time->setValue(percent_time);

}

void PlaylistSequence::SetRepeatMode(const RepeatMode mode) {

  ui_->repeat->setChecked(mode != RepeatMode::Off);

  switch (mode) {
    case RepeatMode::Off:      ui_->action_repeat_off->setChecked(true);      break;
    case RepeatMode::Track:    ui_->action_repeat_track->setChecked(true);    break;
    case RepeatMode::Album:    ui_->action_repeat_album->setChecked(true);    break;
    case RepeatMode::Playlist: ui_->action_repeat_playlist->setChecked(true); break;
    case RepeatMode::OneByOne: ui_->action_repeat_onebyone->setChecked(true); break;
    case RepeatMode::Zapping:  ui_->action_repeat_zapping->setChecked(true);  break;

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
    case ShuffleMode::Grouping:    ui_->action_shuffle_grouping->setChecked(true);     break;
  }

  if (mode != shuffle_mode_) {
    shuffle_mode_ = mode;
    Q_EMIT ShuffleModeChanged(mode);
  }

  Save();

}

void PlaylistSequence::DisplayPlayingOption() {

  ui_->playing_time_before_or_after->setVisible(!ui_->playing_time_before_or_after->isVisible());
  ui_->playing_center_time->setVisible(!ui_->playing_center_time->isVisible());

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
    case ShuffleMode::Albums:      mode = ShuffleMode::Grouping;      break;
    case ShuffleMode::Grouping: break;
  }

  SetShuffleMode(mode);

}

// called from global shortcut
void PlaylistSequence::CycleRepeatMode() {

  RepeatMode mode = RepeatMode::Off;
  // we cycle through the repeat modes
  switch (repeat_mode()) {
    case RepeatMode::Off:       mode = RepeatMode::Track;     break;
    case RepeatMode::Track:     mode = RepeatMode::Album;     break;
    case RepeatMode::Album:     mode = RepeatMode::Playlist;  break;
    case RepeatMode::Playlist:  mode = RepeatMode::OneByOne;  break;
    case RepeatMode::OneByOne:  mode = RepeatMode::Zapping;   break;
    case RepeatMode::Zapping:
      break;
  }

  SetRepeatMode(mode);

}

void PlaylistSequence::UpdatePlayingTime(const int time_s) {

  if (time_s < 15) ui_->action_zapping_time_010->setChecked(true);
  else if (time_s < 25) ui_->action_zapping_time_020->setChecked(true);
  else if (time_s < 38) ui_->action_zapping_time_030->setChecked(true);
  else if (time_s < 52) ui_->action_zapping_time_045->setChecked(true);
  else if (time_s < 75) ui_->action_zapping_time_060->setChecked(true);
  else if (time_s < 115) ui_->action_zapping_time_090->setChecked(true);
  else if (time_s < 150) ui_->action_zapping_time_120->setChecked(true);
  else if (time_s < 210) ui_->action_zapping_time_180->setChecked(true);
  else ui_->action_zapping_time_240->setChecked(true);

  half_playing_time_s_ = time_s;

  Save();

}

void PlaylistSequence::UpdatePlayingPosition(const int percent_time) {

  if (percent_time < 5) ui_->action_zapping_intro_000->setChecked(true);
  else if (percent_time < 15) ui_->action_zapping_intro_010->setChecked(true);
  else if (percent_time < 27) ui_->action_zapping_intro_020->setChecked(true);
  else if (percent_time < 42) ui_->action_zapping_intro_033->setChecked(true);
  else if (percent_time < 58) ui_->action_zapping_intro_050->setChecked(true);
  else if (percent_time < 73) ui_->action_zapping_intro_066->setChecked(true);
  else if (percent_time < 85) ui_->action_zapping_intro_080->setChecked(true);
  else if (percent_time < 95) ui_->action_zapping_intro_090->setChecked(true);
  else ui_->action_zapping_intro_100->setChecked(true);

  percent_interest_song_ = percent_time;

  Save();

}
QString PlaylistSequence::ToolTipPlayingTime() {

  return "<html><head/><body><p>"_L1 +
         QObject::tr("The time played before and after the position time selected in seconds (0 for playing the complete track)") +
         "</p></body></html>"_L1;

}

QString PlaylistSequence::ToolTipPositionTime() {

  return "<html><head/><body><p>"_L1 +
         QObject::tr("The position time reference selected in percent of the track length") +
         "</p></body></html>"_L1;

}
