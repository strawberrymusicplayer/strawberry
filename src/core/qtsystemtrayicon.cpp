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

#include <QObject>
#include <QCoreApplication>
#include <QSystemTrayIcon>
#include <QAction>
#include <QIODevice>
#include <QFile>
#include <QMenu>
#include <QIcon>
#include <QString>
#include <QUrl>
#include <QtEvents>
#include <QSettings>

#include "song.h"
#include "iconloader.h"
#include "utilities.h"

#include "systemtrayicon.h"
#include "qtsystemtrayicon.h"

#include "settings/behavioursettingspage.h"

QtSystemTrayIcon::QtSystemTrayIcon(QObject *parent)
    : SystemTrayIcon(parent),
      tray_(new QSystemTrayIcon(this)),
      menu_(new QMenu),
      app_name_(QCoreApplication::applicationName()),
      icon_(IconLoader::Load("strawberry")),
      normal_icon_(icon_.pixmap(48, QIcon::Normal)),
      grey_icon_(icon_.pixmap(48, QIcon::Disabled)),
      action_play_pause_(nullptr),
      action_stop_(nullptr),
      action_stop_after_this_track_(nullptr),
      action_mute_(nullptr) {

  app_name_[0] = app_name_[0].toUpper();

  tray_->setIcon(normal_icon_);
  tray_->installEventFilter(this);
  ClearNowPlaying();

#ifndef Q_OS_WIN
  de_ = Utilities::DesktopEnvironment().toLower();
  QFile pattern_file(":/html/playing-tooltip.html");
  if (pattern_file.open(QIODevice::ReadOnly)) {
    pattern_ = QString::fromLatin1(pattern_file.readAll());
    pattern_file.close();
  }

#endif

  connect(tray_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(Clicked(QSystemTrayIcon::ActivationReason)));

}

QtSystemTrayIcon::~QtSystemTrayIcon() {
  delete menu_;
}

bool QtSystemTrayIcon::eventFilter(QObject *object, QEvent *event) {

  if (QObject::eventFilter(object, event)) return true;

  if (object != tray_) return false;

  if (event->type() == QEvent::Wheel) {
    QWheelEvent *e = static_cast<QWheelEvent*>(event);
    if (e->modifiers() == Qt::ShiftModifier) {
      if (e->angleDelta().y() > 0) {
        emit SeekForward();
      }
      else {
        emit SeekBackward();
      }
    }
    else if (e->modifiers() == Qt::ControlModifier) {
      if (e->angleDelta().y() < 0) {
        emit NextTrack();
      }
      else {
        emit PreviousTrack();
      }
    }
    else {
      QSettings s;
      s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
      bool prev_next_track = s.value("scrolltrayicon").toBool();
      s.endGroup();
      if (prev_next_track) {
        if (e->angleDelta().y() < 0) {
          emit NextTrack();
        }
        else {
          emit PreviousTrack();
        }
      }
      else {
        emit ChangeVolume(e->angleDelta().y());
      }
    }
    return true;
  }

  return false;

}

void QtSystemTrayIcon::SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit) {

  // Creating new actions and connecting them to old ones.
  // This allows us to use old actions without displaying shortcuts that can not be used when Strawberry's window is hidden
  menu_->addAction(previous->icon(), previous->text(), previous, SLOT(trigger()));
  action_play_pause_ = menu_->addAction(play->icon(), play->text(), play, SLOT(trigger()));
  action_stop_ = menu_->addAction(stop->icon(), stop->text(), stop, SLOT(trigger()));
  action_stop_after_this_track_ = menu_->addAction(stop_after->icon(), stop_after->text(), stop_after, SLOT(trigger()));
  menu_->addAction(next->icon(), next->text(), next, SLOT(trigger()));

  menu_->addSeparator();
  action_mute_ = menu_->addAction(mute->icon(), mute->text(), mute, SLOT(trigger()));
  action_mute_->setCheckable(true);
  action_mute_->setChecked(mute->isChecked());

  menu_->addSeparator();
  action_love_ = menu_->addAction(love->icon(), love->text(), love, SLOT(trigger()));
  action_love_->setVisible(love->isVisible());
  action_love_->setEnabled(love->isEnabled());
  menu_->addSeparator();
  menu_->addAction(quit->icon(), quit->text(), quit, SLOT(trigger()));

  tray_->setContextMenu(menu_);

}

void QtSystemTrayIcon::Clicked(QSystemTrayIcon::ActivationReason reason) {

  switch (reason) {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:
      emit ShowHide();
      break;

    case QSystemTrayIcon::MiddleClick:
      emit PlayPause();
      break;

    default:
      break;
  }

}

void QtSystemTrayIcon::ShowPopup(const QString &summary, const QString &message, int timeout) {
  tray_->showMessage(summary, message, QSystemTrayIcon::NoIcon, timeout);
}

void QtSystemTrayIcon::UpdateIcon() {
  tray_->setIcon(CreateIcon(normal_icon_, grey_icon_));
}

void QtSystemTrayIcon::SetPaused() {

  SystemTrayIcon::SetPaused();

  action_stop_->setEnabled(true);
  action_stop_after_this_track_->setEnabled(true);
  action_play_pause_->setIcon(IconLoader::Load("media-playback-start"));
  action_play_pause_->setText(tr("Play"));

  action_play_pause_->setEnabled(true);

}

void QtSystemTrayIcon::SetPlaying(bool enable_play_pause) {

  SystemTrayIcon::SetPlaying();

  action_stop_->setEnabled(true);
  action_stop_after_this_track_->setEnabled(true);
  action_play_pause_->setIcon(IconLoader::Load("media-playback-pause"));
  action_play_pause_->setText(tr("Pause"));
  action_play_pause_->setEnabled(enable_play_pause);

}

void QtSystemTrayIcon::SetStopped() {

  SystemTrayIcon::SetStopped();

  action_stop_->setEnabled(false);
  action_stop_after_this_track_->setEnabled(false);
  action_play_pause_->setIcon(IconLoader::Load("media-playback-start"));
  action_play_pause_->setText(tr("Play"));

  action_play_pause_->setEnabled(true);

  action_love_->setEnabled(false);

}

void QtSystemTrayIcon::MuteButtonStateChanged(bool value) {
  if (action_mute_) action_mute_->setChecked(value);
}

bool QtSystemTrayIcon::IsVisible() const {
  return tray_->isVisible();
}

void QtSystemTrayIcon::SetVisible(bool visible) {
  tray_->setVisible(visible);
}

void QtSystemTrayIcon::SetNowPlaying(const Song &song, const QUrl &cover_url) {

#ifdef Q_OS_WIN
  Q_UNUSED(song);
  Q_UNUSED(cover_url);
  // Windows doesn't support HTML in tooltips, so just show something basic
  tray_->setToolTip(song.PrettyTitleWithArtist());
#else

  if (de_ == "kde" || de_ == "cinnamon" || de_ == "x-cinnamon") {
    tray_->setToolTip(song.PrettyTitleWithArtist());
    return;
  }

  int columns = cover_url.isEmpty() ? 1 : 2;

  QString tooltip(pattern_);

  tooltip.replace("%columns"    , QString::number(columns));
  tooltip.replace("%appName"    , app_name_);

  tooltip.replace("%titleKey"   , tr("Title") % ":");
  tooltip.replace("%titleValue" , song.PrettyTitle().toHtmlEscaped());
  tooltip.replace("%artistKey"  , tr("Artist") % ":");
  tooltip.replace("%artistValue", song.artist().toHtmlEscaped());
  tooltip.replace("%albumKey"   , tr("Album") % ":");
  tooltip.replace("%albumValue" , song.album().toHtmlEscaped());

  tooltip.replace("%lengthKey"  , tr("Length") % ":");
  tooltip.replace("%lengthValue", song.PrettyLength().toHtmlEscaped());

  if (columns == 2) {
    QString final_path = cover_url.isLocalFile() ? cover_url.path() : cover_url.toString();
    tooltip.replace("%image", "    <td>      <img src=\"" % final_path % "\" />   </td>");
  }
  else {
    tooltip.replace("<td>%image</td>", "");
    tooltip.replace("%image", "");
  }

  // TODO: we should also repaint this
  tray_->setToolTip(tooltip);

#endif

}

void QtSystemTrayIcon::ClearNowPlaying() {
  tray_->setToolTip(app_name_);
}

void QtSystemTrayIcon::LoveVisibilityChanged(bool value) {
  action_love_->setVisible(value);
}

void QtSystemTrayIcon::LoveStateChanged(bool value) {
  action_love_->setEnabled(value);
}
