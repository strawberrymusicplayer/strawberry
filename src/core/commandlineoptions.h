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

#ifndef COMMANDLINEOPTIONS_H
#define COMMANDLINEOPTIONS_H

#include "config.h"

#include <QtGlobal>
#include <QDataStream>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#ifdef Q_OS_WIN32
#  include <windows.h>
#endif

class CommandlineOptions {
  friend QDataStream &operator<<(QDataStream &s, const CommandlineOptions &a);
  friend QDataStream &operator>>(QDataStream &s, CommandlineOptions &a);

 public:
  explicit CommandlineOptions(int argc = 0, char **argv = nullptr);

  // Don't change the values or order, these get serialised and sent to
  // possibly a different version of Strawberry
  enum class UrlListAction {
    Append = 0,
    Load = 1,
    None = 2,
    CreateNew = 3
  };
  enum class PlayerAction {
    None = 0,
    Play = 1,
    PlayPause = 2,
    Pause = 3,
    Stop = 4,
    Previous = 5,
    Next = 6,
    RestartOrPrevious = 7,
    StopAfterCurrent = 8,
    PlayPlaylist = 9,
    ResizeWindow = 10
  };

  bool Parse();

  bool is_empty() const;
  bool contains_play_options() const;

  UrlListAction url_list_action() const { return url_list_action_; }
  PlayerAction player_action() const { return player_action_; }
  int set_volume() const { return set_volume_; }
  int volume_modifier() const { return volume_modifier_; }
  int seek_to() const { return seek_to_; }
  int seek_by() const { return seek_by_; }
  int play_track_at() const { return play_track_at_; }
  bool show_osd() const { return show_osd_; }
  bool toggle_pretty_osd() const { return toggle_pretty_osd_; }
  QList<QUrl> urls() const { return urls_; }
  QString language() const { return language_; }
  QString log_levels() const { return log_levels_; }
  QString playlist_name() const { return playlist_name_; }
  QString window_size() const { return window_size_; }

  QByteArray Serialize() const;
  void Load(const QByteArray &serialized);

 private:
  // These are "invalid" characters to pass to getopt_long for options that shouldn't have a short (single character) option.
  enum LongOptions {
    VolumeUp = 256,
    VolumeDown,
    SeekTo,
    SeekBy,
    Quiet,
    Verbose,
    LogLevels,
    Version,
    VolumeIncreaseBy,
    VolumeDecreaseBy,
    RestartOrPrevious
  };

  void RemoveArg(const QString &starts_with, int count);

#ifdef Q_OS_WIN32
  static QString OptArgToString(const wchar_t *opt);
  static QString DecodeName(wchar_t *opt);
#else
  static QString OptArgToString(const char *opt);
  static QString DecodeName(char *opt);
#endif

 private:
  int argc_;
#ifdef Q_OS_WIN32
  LPWSTR *argv_;
#else
  char **argv_;
#endif

  UrlListAction url_list_action_;
  PlayerAction player_action_;

  // Don't change the type of these.
  int set_volume_;
  int volume_modifier_;
  int seek_to_;
  int seek_by_;
  int play_track_at_;
  bool show_osd_;
  bool toggle_pretty_osd_;
  QString language_;
  QString log_levels_;
  QString playlist_name_;
  QString window_size_;

  QList<QUrl> urls_;
};

QDataStream &operator<<(QDataStream &s, const CommandlineOptions &a);
QDataStream &operator>>(QDataStream &s, CommandlineOptions &a);

#endif  // COMMANDLINEOPTIONS_H
