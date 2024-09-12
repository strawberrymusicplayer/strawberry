/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QStringList>
#include <QImage>
#include <QSettings>

#include "osdbase.h"
#include "osdpretty.h"

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/settings.h"
#ifdef Q_OS_MACOS
#  include "core/macsystemtrayicon.h"
#else
#  include "core/qtsystemtrayicon.h"
#endif
#include "utilities/strutils.h"
#include "covermanager/currentalbumcoverloader.h"

using namespace Qt::StringLiterals;

const char *OSDBase::kSettingsGroup = "OSD";

OSDBase::OSDBase(SharedPtr<SystemTrayIcon> tray_icon, Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      tray_icon_(tray_icon),
      pretty_popup_(new OSDPretty(OSDPretty::Mode::Popup)),
      app_name_(QCoreApplication::applicationName()),
      timeout_msec_(5000),
      behaviour_(Behaviour::Native),
      show_on_volume_change_(false),
      show_art_(true),
      show_on_play_mode_change_(true),
      show_on_pause_(true),
      show_on_resume_(false),
      use_custom_text_(false),
      force_show_next_(false),
      ignore_next_stopped_(false),
      playing_(false) {

  QObject::connect(&*app_->current_albumcover_loader(), &CurrentAlbumCoverLoader::ThumbnailLoaded, this, &OSDBase::AlbumCoverLoaded);

  app_name_[0] = app_name_[0].toUpper();

}

OSDBase::~OSDBase() {
  delete pretty_popup_;
}

void OSDBase::ReloadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  behaviour_ = static_cast<OSDBase::Behaviour>(s.value("Behaviour", static_cast<int>(Behaviour::Native)).toInt());
  timeout_msec_ = s.value("Timeout", 5000).toInt();
  show_on_volume_change_ = s.value("ShowOnVolumeChange", false).toBool();
  show_art_ = s.value("ShowArt", true).toBool();
  show_on_play_mode_change_ = s.value("ShowOnPlayModeChange", true).toBool();
  show_on_pause_ = s.value("ShowOnPausePlayback", true).toBool();
  show_on_resume_ = s.value("ShowOnResumePlayback", false).toBool();
  use_custom_text_ = s.value(("CustomTextEnabled"), false).toBool();
  custom_text1_ = s.value("CustomText1").toString();
  custom_text2_ = s.value("CustomText2").toString();
  s.endGroup();

#ifdef Q_OS_WIN32
  if (!SupportsNativeNotifications() && !SupportsTrayPopups() && behaviour_ == Behaviour::Native) {
#else
  if (!SupportsNativeNotifications() && behaviour_ == Behaviour::Native) {
#endif
    behaviour_ = Behaviour::Pretty;
  }

  if (!SupportsTrayPopups() && behaviour_ == Behaviour::TrayPopup) {
    behaviour_ = Behaviour::Disabled;
  }

  ReloadPrettyOSDSettings();

}

// Reload just Pretty OSD settings, not everything
void OSDBase::ReloadPrettyOSDSettings() {

  pretty_popup_->set_popup_duration(timeout_msec_);
  pretty_popup_->ReloadSettings();

}

void OSDBase::ReshowCurrentSong() {

  force_show_next_ = true;
  ShowPlaying(last_song_, last_image_uri_, last_image_);

}

void OSDBase::AlbumCoverLoaded(const Song &song, const QUrl &cover_url, const QImage &image) {

  if (song != song_playing_) return;

  last_song_ = song;
  last_image_ = image;
  last_image_uri_ = cover_url;

  ShowPlaying(song, cover_url, image);

}

void OSDBase::ShowPlaying(const Song &song, const QUrl &cover_url, const QImage &image, const bool preview) {

  // Don't change tray icon details if it's a preview
  if (!preview && tray_icon_) tray_icon_->SetNowPlaying(song, cover_url);

  QStringList message_parts;
  QString summary;
  bool html_escaped = false;
  if (use_custom_text_) {
    summary = ReplaceMessage(MessageType::Summary, custom_text1_, song);
    message_parts << ReplaceMessage(MessageType::Message, custom_text2_, song);
  }
  else {
    summary = song.PrettyTitle();
    if (!song.artist().isEmpty()) {
      summary = QStringLiteral("%1 - %2").arg(song.artist(), summary);
    }
    if (!song.album().isEmpty()) {
      message_parts << song.album();
    }
    if (song.disc() > 0) {
      message_parts << tr("disc %1").arg(song.disc());
    }
    if (song.track() > 0) {
      message_parts << tr("track %1").arg(song.track());
    }
    if (behaviour_ == Behaviour::Pretty) {
      summary = summary.toHtmlEscaped();
      html_escaped = true;
    }
#if defined(HAVE_DBUS) && !defined(Q_OS_MACOS)
    else if (behaviour_ == Behaviour::Native) {
      html_escaped = true;
    }
#endif
  }

  QString message = message_parts.join(", "_L1);
  if (html_escaped) message = message.toHtmlEscaped();

  if (show_art_) {
    ShowMessage(summary, message, QStringLiteral("notification-audio-play"), image);
  }
  else {
    ShowMessage(summary, message, QStringLiteral("notification-audio-play"), QImage());
  }

  // Reload the saved settings if they were changed for preview
  if (preview) {
    ReloadSettings();
  }

}

void OSDBase::SongChanged(const Song &song) {
  playing_ = true;
  song_playing_ = song;
}

void OSDBase::Paused() {

  if (show_on_pause_) {
    QString summary;
    if (use_custom_text_) {
      summary = ReplaceMessage(MessageType::Summary, custom_text1_, last_song_);
    }
    else {
      summary = last_song_.PrettyTitle();
      if (!last_song_.artist().isEmpty()) {
        summary.prepend(" - "_L1);
        summary.prepend(last_song_.artist());
      }
      if (behaviour_ == Behaviour::Pretty) {
        summary = summary.toHtmlEscaped();
      }
    }
    if (show_art_) {
      ShowMessage(summary, tr("Paused"), QString(), last_image_);
    }
    else {
      ShowMessage(summary, tr("Paused"));
    }
  }

}

void OSDBase::Resumed() {

  if (show_on_resume_) {
    ShowPlaying(last_song_, last_image_uri_, last_image_);
  }

}

void OSDBase::Stopped() {

  if (!playing_) return;

  playing_ = false;
  song_playing_ = Song();

  if (tray_icon_) tray_icon_->ClearNowPlaying();
  if (ignore_next_stopped_) {
    ignore_next_stopped_ = false;
    return;
  }

  QString summary;
  if (use_custom_text_) {
    summary = ReplaceMessage(MessageType::Summary, custom_text1_, last_song_);
  }
  else {
    summary = last_song_.PrettyTitle();
    if (!last_song_.artist().isEmpty()) {
      summary.prepend(" - "_L1);
      summary.prepend(last_song_.artist());
    }
    if (behaviour_ == Behaviour::Pretty) {
      summary = summary.toHtmlEscaped();
    }
  }

  if (show_art_) {
    ShowMessage(summary, tr("Stopped"), QString(), last_image_);
  }
  else {
    ShowMessage(summary, tr("Stopped"));
  }

  last_song_ = Song();
  last_image_ = QImage();
  last_image_uri_.clear();

}

void OSDBase::StopAfterToggle(const bool stop) {
  ShowMessage(app_name_, tr("Stop playing after track: %1").arg(stop ? tr("On") : tr("Off")));
}

void OSDBase::PlaylistFinished() {

  // We get a PlaylistFinished followed by a Stopped from the player
  ignore_next_stopped_ = true;

  ShowMessage(app_name_, tr("Playlist finished"));

}

void OSDBase::VolumeChanged(const uint value) {

  if (!show_on_volume_change_) return;

  QString message = tr("Volume %1%").arg(value);
  if (behaviour_ == Behaviour::Pretty) {
    message = message.toHtmlEscaped();
  }
#if defined(HAVE_DBUS) && !defined(Q_OS_MACOS)
  else if (behaviour_ == Behaviour::Native) {
    message = message.toHtmlEscaped();
  }
#endif

  ShowMessage(app_name_, message);

}

void OSDBase::ShowMessage(const QString &summary, const QString &message, const QString &icon, const QImage &image) {

  if (pretty_popup_->toggle_mode()) {
    pretty_popup_->ShowMessage(summary, message, image);
  }
  else {
    switch (behaviour_) {
      case Behaviour::Native:
#ifdef Q_OS_WIN32
        Q_UNUSED(icon)
        [[fallthrough]];
#else
        if (image.isNull()) {
          ShowMessageNative(summary, message, icon, QImage());
        }
        else {
          ShowMessageNative(summary, message, QString(), image);
        }
        break;
#endif
      case Behaviour::TrayPopup:
#ifdef Q_OS_MACOS
        [[fallthrough]];
#else
        if (tray_icon_) tray_icon_->ShowPopup(summary, message, timeout_msec_);
        break;
#endif
      case Behaviour::Disabled:
        if (!force_show_next_) break;
        force_show_next_ = false;
      [[fallthrough]];
      case Behaviour::Pretty:
        pretty_popup_->ShowMessage(summary, message, image);
        break;

      default:
        break;
    }
  }

}

void OSDBase::ShuffleModeChanged(const PlaylistSequence::ShuffleMode mode) {

  if (show_on_play_mode_change_) {
    QString current_mode = QString();
    switch (mode) {
      case PlaylistSequence::ShuffleMode::Off:         current_mode = tr("Don't shuffle");   break;
      case PlaylistSequence::ShuffleMode::All:         current_mode = tr("Shuffle all");     break;
      case PlaylistSequence::ShuffleMode::InsideAlbum: current_mode = tr("Shuffle tracks in this album"); break;
      case PlaylistSequence::ShuffleMode::Albums:      current_mode = tr("Shuffle albums");  break;
    }
    ShowMessage(app_name_, current_mode);
  }

}

void OSDBase::RepeatModeChanged(const PlaylistSequence::RepeatMode mode) {

  if (show_on_play_mode_change_) {
    QString current_mode = QString();
    switch (mode) {
      case PlaylistSequence::RepeatMode::Off:      current_mode = tr("Don't repeat");   break;
      case PlaylistSequence::RepeatMode::Track:    current_mode = tr("Repeat track");   break;
      case PlaylistSequence::RepeatMode::Album:    current_mode = tr("Repeat album"); break;
      case PlaylistSequence::RepeatMode::Playlist: current_mode = tr("Repeat playlist"); break;
      case PlaylistSequence::RepeatMode::OneByOne: current_mode = tr("Stop after every track"); break;
      case PlaylistSequence::RepeatMode::Intro:    current_mode = tr("Intro tracks"); break;
    }
    ShowMessage(app_name_, current_mode);
  }

}

QString OSDBase::ReplaceMessage(const MessageType type, const QString &message, const Song &song) {

#if !defined(HAVE_DBUS) || defined(Q_OS_MACOS)
  Q_UNUSED(type)
#endif

  bool html_escaped = false;
  QString newline = ""_L1;

  // We need different strings depending on notification type
  switch (behaviour_) {
    case Behaviour::Native:
#if defined(Q_OS_MACOS)
      html_escaped = false;
      newline = QLatin1String("\n");
      break;
#elif defined(HAVE_DBUS)
      switch (type) {
        case MessageType::Summary:{
          html_escaped = false;
          newline = ""_L1;
          break;
        }
        case MessageType::Message:{
          html_escaped = true;
          newline = "<br />"_L1;
          break;
        }
      }
      break;
#elif defined(Q_OS_WIN32)
      [[fallthrough]];
#else
      // Other OSes doesn't support native notifications.
      qLog(Debug) << "Native notifications are not supported on this OS.";
      break;
#endif
    case Behaviour::TrayPopup:
      qLog(Debug) << "New line not supported by this notification type.";
      html_escaped = false;
      newline = ""_L1;
      break;
    case Behaviour::Disabled:  // When notifications are disabled, we force the PrettyOSD
    case Behaviour::Pretty:
      html_escaped = true;
      newline = "<br />"_L1;
      break;
  }

  return Utilities::ReplaceMessage(message, song, newline, html_escaped);

}

void OSDBase::ShowPreview(const Behaviour type, const QString &line1, const QString &line2, const Song &song) {

  behaviour_ = type;
  custom_text1_ = line1;
  custom_text2_ = line2;
  if (!use_custom_text_) use_custom_text_ = true;

  // We want to reload the settings, but we can't do this here because the cover art loading is asynch
  ShowPlaying(song, QUrl(), QImage(), true);

}

void OSDBase::SetPrettyOSDToggleMode(const bool toggle) {
  pretty_popup_->set_toggle_mode(toggle);
}

bool OSDBase::SupportsNativeNotifications() {
  return false;
}

bool OSDBase::SupportsTrayPopups() {
  return tray_icon_->IsSystemTrayAvailable();
}

void OSDBase::ShowMessageNative(const QString&, const QString&, const QString&, const QImage&) {
  qLog(Warning) << "Native notifications are not supported on this OS.";
}
