/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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
#include "version.h"

#include <cstdlib>
#include <iostream>

#include <QtGlobal>

#ifdef Q_OS_WIN32
#  include <windows.h>
#endif

#include <QObject>
#include <QIODevice>
#include <QDataStream>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QString>
#include <QUrl>

#include "commandlineoptions.h"
#include "core/logging.h"

#include <getopt.h>

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kHelpText[] =
    "%1: strawberry [%2] [%3]\n"
    "\n"
    "%4:\n"
    "  -p, --play                 %5\n"
    "  -t, --play-pause           %6\n"
    "  -u, --pause                %7\n"
    "  -s, --stop                 %8\n"
    "  -q, --stop-after-current   %9\n"
    "  -r, --previous             %10\n"
    "  -f, --next                 %11\n"
    "  -v, --volume <value>       %12\n"
    "  --volume-up                %13\n"
    "  --volume-down              %14\n"
    "  --volume-increase-by       %15\n"
    "  --volume-decrease-by       %16\n"
    "  --seek-to <seconds>        %17\n"
    "  --seek-by <seconds>        %18\n"
    "  --restart-or-previous      %19\n"
    "\n"
    "%20:\n"
    "  -c, --create <name>        %21\n"
    "  -a, --append               %22\n"
    "  -l, --load                 %23\n"
    "  -k, --play-track <n>       %24\n"
    "  -i, --play-playlist <name> %25\n"
    "\n"
    "%26:\n"
    "  -o, --show-osd             %27\n"
    "  -y, --toggle-pretty-osd    %28\n"
    "  -g, --language <lang>      %29\n"
    "  -w, --resize-window <WxH>  %30\n"
    "      --quiet                %31\n"
    "      --verbose              %32\n"
    "      --log-levels <levels>  %33\n"
    "      --version              %34\n";

constexpr char kVersionText[] = "Strawberry %1";

}  // namespace

CommandlineOptions::CommandlineOptions(int argc, char **argv)
    : argc_(argc),
#ifdef Q_OS_WIN32
      argv_(CommandLineToArgvW(GetCommandLineW(), &argc)),
#else
      argv_(argv),
#endif
      url_list_action_(UrlListAction::None),
      player_action_(PlayerAction::None),
      set_volume_(-1),
      volume_modifier_(0),
      seek_to_(-1),
      seek_by_(0),
      play_track_at_(-1),
      show_osd_(false),
      toggle_pretty_osd_(false),
      log_levels_(QLatin1String(logging::kDefaultLogLevels)) {

#ifdef Q_OS_WIN32
  Q_UNUSED(argv);
#endif

#ifdef Q_OS_MACOS
  // Remove -psn_xxx option that Mac passes when opened from Finder.
  RemoveArg(u"-psn"_s, 1);
#endif

  // Remove the -session option that KDE passes
  RemoveArg(u"-session"_s, 2);

}

void CommandlineOptions::RemoveArg(const QString &starts_with, int count) {

  for (int i = 0; i < argc_; ++i) {
    const QString opt = OptArgToString(argv_[i]);
    if (opt.startsWith(starts_with)) {
      for (int j = i; j < argc_ - count + 1; ++j) {
        argv_[j] = argv_[j + count];
      }
      argc_ -= count;
      break;
    }
  }

}

bool CommandlineOptions::Parse() {

  static const struct option kOptions[] = {
#ifdef Q_OS_WIN32
      {L"help", no_argument, nullptr, 'h'},
      {L"play", no_argument, nullptr, 'p'},
      {L"play-pause", no_argument, nullptr, 't'},
      {L"pause", no_argument, nullptr, 'u'},
      {L"stop", no_argument, nullptr, 's'},
      {L"stop-after-current", no_argument, nullptr, 'q'},
      {L"previous", no_argument, nullptr, 'r'},
      {L"next", no_argument, nullptr, 'f'},
      {L"volume", required_argument, nullptr, 'v'},
      {L"volume-up", no_argument, nullptr, LongOptions::VolumeUp},
      {L"volume-down", no_argument, nullptr, LongOptions::VolumeDown},
      {L"volume-increase-by", required_argument, nullptr, LongOptions::VolumeIncreaseBy},
      {L"volume-decrease-by", required_argument, nullptr, LongOptions::VolumeDecreaseBy},
      {L"seek-to", required_argument, nullptr, LongOptions::SeekTo},
      {L"seek-by", required_argument, nullptr, LongOptions::SeekBy},
      {L"restart-or-previous", no_argument, nullptr, LongOptions::RestartOrPrevious },
      {L"create", required_argument, nullptr, 'c' },
      {L"append", no_argument, nullptr, 'a' },
      {L"load", no_argument, nullptr, 'l'},
      {L"play-track", required_argument, nullptr, 'k'},
      {L"play-playlist", required_argument, nullptr, 'i'},
      {L"show-osd", no_argument, nullptr, 'o'},
      {L"toggle-pretty-osd", no_argument, nullptr, 'y'},
      {L"language", required_argument, nullptr, 'g'},
      {L"resize-window", required_argument, nullptr, 'w'},
      {L"quiet", no_argument, nullptr, LongOptions::Quiet},
      {L"verbose", no_argument, nullptr, LongOptions::Verbose},
      {L"log-levels", required_argument, nullptr, LongOptions::LogLevels},
      {L"version", no_argument, nullptr, LongOptions::Version},
      {nullptr, 0, nullptr, 0}
#else
    { "help", no_argument, nullptr, 'h' },
    { "play", no_argument, nullptr, 'p' },
    { "play-pause", no_argument, nullptr, 't' },
    { "pause", no_argument, nullptr, 'u' },
    { "stop", no_argument, nullptr, 's' },
    { "stop-after-current", no_argument, nullptr, 'q' },
    { "previous", no_argument, nullptr, 'r' },
    { "next", no_argument, nullptr, 'f' },
    { "volume", required_argument, nullptr, 'v' },
    { "volume-up", no_argument, nullptr, LongOptions::VolumeUp },
    { "volume-down", no_argument, nullptr, LongOptions::VolumeDown },
    { "volume-increase-by", required_argument, nullptr, LongOptions::VolumeIncreaseBy },
    { "volume-decrease-by", required_argument, nullptr, LongOptions::VolumeDecreaseBy },
    { "seek-to", required_argument, nullptr, LongOptions::SeekTo },
    { "seek-by", required_argument, nullptr, LongOptions::SeekBy },
    { "restart-or-previous", no_argument, nullptr, LongOptions::RestartOrPrevious },
    { "create", required_argument, nullptr, 'c' },
    { "append", no_argument, nullptr, 'a' },
    { "load", no_argument, nullptr, 'l' },
    { "play-track", required_argument, nullptr, 'k' },
    { "play-playlist", required_argument, nullptr, 'i' },
    { "show-osd", no_argument, nullptr, 'o' },
    { "toggle-pretty-osd", no_argument, nullptr, 'y' },
    { "language", required_argument, nullptr, 'g' },
    { "resize-window", required_argument, nullptr, 'w' },
    { "quiet", no_argument, nullptr, LongOptions::Quiet },
    { "verbose", no_argument, nullptr, LongOptions::Verbose },
    { "log-levels", required_argument, nullptr, LongOptions::LogLevels },
    { "version", no_argument, nullptr, LongOptions::Version },
    { nullptr, 0, nullptr, 0 }
#endif
};

  // Parse the arguments
  bool ok = false;
  Q_FOREVER {
#ifdef Q_OS_WIN32
    int c = getopt_long(argc_, argv_, L"hptusqrfv:c:alk:i:oyg:w:", kOptions, nullptr);
#else
    int c = getopt_long(argc_, argv_, "hptusqrfv:c:alk:i:oyg:w:", kOptions, nullptr);
#endif

    // End of the options
    if (c == -1) break;

    switch (c) {
      case 'h':{
        QString translated_help_text =
            QString::fromUtf8(kHelpText)
                .arg(QObject::tr("Usage"), QObject::tr("options"), QObject::tr("URL(s)"),
                     QObject::tr("Player options"),
                     QObject::tr("Start the playlist currently playing"),
                     QObject::tr("Play if stopped, pause if playing"),
                     QObject::tr("Pause playback"), QObject::tr("Stop playback"),
                     QObject::tr("Stop playback after current track"))
                .arg(QObject::tr("Skip backwards in playlist"),
                     QObject::tr("Skip forwards in playlist"),
                     QObject::tr("Set the volume to <value> percent"),
                     QObject::tr("Increase the volume by 4 percent"),
                     QObject::tr("Decrease the volume by 4 percent"),
                     QObject::tr("Increase the volume by <value> percent"),
                     QObject::tr("Decrease the volume by <value> percent"))
                .arg(QObject::tr("Seek the currently playing track to an absolute position"),
                     QObject::tr("Seek the currently playing track by a relative amount"),
                     QObject::tr("Restart the track, or play the previous track if within 8 seconds of start."),
                     QObject::tr("Playlist options"),
                     QObject::tr("Create a new playlist with files"),
                     QObject::tr("Append files/URLs to the playlist"),
                     QObject::tr("Loads files/URLs, replacing current playlist"),
                     QObject::tr("Play the <n>th track in the playlist"),
                     QObject::tr("Play given playlist"))
                .arg(QObject::tr("Other options"), QObject::tr("Display the on-screen-display"),
                     QObject::tr("Toggle visibility for the pretty on-screen-display"),
                     QObject::tr("Change the language"),
                     QObject::tr("Resize the window"),
                     QObject::tr("Equivalent to --log-levels *:1"),
                     QObject::tr("Equivalent to --log-levels *:3"),
                     QObject::tr("Comma separated list of class:level, level is 0-3"))
                .arg(QObject::tr("Print out version information"));

        std::cout << translated_help_text.toLocal8Bit().constData();
        return false;
      }

      case 'p':
        player_action_ = PlayerAction::Play;
        break;
      case 't':
        player_action_ = PlayerAction::PlayPause;
        break;
      case 'u':
        player_action_ = PlayerAction::Pause;
        break;
      case 's':
        player_action_ = PlayerAction::Stop;
        break;
      case 'q':
        player_action_ = PlayerAction::StopAfterCurrent;
        break;
      case 'r':
        player_action_ = PlayerAction::Previous;
        break;
      case 'f':
        player_action_ = PlayerAction::Next;
        break;
      case 'i':
        player_action_ = PlayerAction::PlayPlaylist;
        playlist_name_ = OptArgToString(optarg);
        break;
      case 'c':
        url_list_action_ = UrlListAction::CreateNew;
        playlist_name_ = OptArgToString(optarg);
        break;
      case 'a':
        url_list_action_ = UrlListAction::Append;
        break;
      case 'l':
        url_list_action_ = UrlListAction::Load;
        break;
      case 'o':
        show_osd_ = true;
        break;
      case 'y':
        toggle_pretty_osd_ = true;
        break;
      case 'g':
        language_ = OptArgToString(optarg);
        break;
      case LongOptions::VolumeUp:
        volume_modifier_ = +4;
        break;
      case LongOptions::VolumeDown:
        volume_modifier_ = -4;
        break;
      case LongOptions::Quiet:
        log_levels_ = u"1"_s;
        break;
      case LongOptions::Verbose:
        log_levels_ = u"3"_s;
        break;
      case LongOptions::LogLevels:
        log_levels_ = OptArgToString(optarg);
        break;
      case LongOptions::Version:{
        QString version_text = QString::fromUtf8(kVersionText).arg(QLatin1String(STRAWBERRY_VERSION_DISPLAY));
        std::cout << version_text.toLocal8Bit().constData() << std::endl;
        std::exit(0);
      }
      case 'v':
        set_volume_ = OptArgToString(optarg).toInt(&ok);
        if (!ok) set_volume_ = -1;
        break;

      case LongOptions::VolumeIncreaseBy:
        volume_modifier_ = OptArgToString(optarg).toInt(&ok);
        if (!ok) volume_modifier_ = 0;
        break;

      case LongOptions::VolumeDecreaseBy:
        volume_modifier_ = -OptArgToString(optarg).toInt(&ok);
        if (!ok) volume_modifier_ = 0;
        break;

      case LongOptions::SeekTo:
        seek_to_ = OptArgToString(optarg).toInt(&ok);
        if (!ok) seek_to_ = -1;
        break;

      case LongOptions::SeekBy:
        seek_by_ = OptArgToString(optarg).toInt(&ok);
        if (!ok) seek_by_ = 0;
        break;

      case LongOptions::RestartOrPrevious:
        player_action_ = PlayerAction::RestartOrPrevious;
        break;

      case 'k':
        play_track_at_ = OptArgToString(optarg).toInt(&ok);
        if (!ok) play_track_at_ = -1;
        break;

      case 'w':
        window_size_ = OptArgToString(optarg);
        player_action_ = PlayerAction::ResizeWindow;
        break;

      case '?':
      default:
        return false;
    }
  }

  // Get any filenames or URLs following the arguments
  for (int i = optind; i < argc_; ++i) {
    const QString value = DecodeName(argv_[i]);
    QFileInfo fileinfo(value);
    if (fileinfo.exists()) {
      urls_ << QUrl::fromLocalFile(fileinfo.absoluteFilePath());
    }
    else {
      urls_ << QUrl::fromUserInput(value);
    }
  }

  return true;

}

bool CommandlineOptions::is_empty() const {
  return player_action_ == PlayerAction::None &&
         set_volume_ == -1 &&
         volume_modifier_ == 0 &&
         seek_to_ == -1 &&
         seek_by_ == 0 &&
         play_track_at_ == -1 &&
         !show_osd_ &&
         !toggle_pretty_osd_ &&
         urls_.isEmpty();
}

bool CommandlineOptions::contains_play_options() const {
  return player_action_ != PlayerAction::None || play_track_at_ != -1 || !urls_.isEmpty();
}

QByteArray CommandlineOptions::Serialize() const {

  QBuffer buf;
  if (buf.open(QIODevice::WriteOnly)) {
    QDataStream s(&buf);
    s << *this;
    buf.close();
  }

  return buf.data().toBase64();

}

void CommandlineOptions::Load(const QByteArray &serialized) {

  QByteArray copy = QByteArray::fromBase64(serialized);
  QBuffer buf(&copy);
  if (buf.open(QIODevice::ReadOnly)) {
    QDataStream s(&buf);
    s >> *this;
  }

}

#ifdef Q_OS_WIN32
QString CommandlineOptions::OptArgToString(const wchar_t *opt) {

  return QString::fromWCharArray(opt);

}

QString CommandlineOptions::DecodeName(wchar_t *opt) {

  return QString::fromWCharArray(opt);
}
#else
QString CommandlineOptions::OptArgToString(const char *opt) {

  return QString::fromUtf8(opt);
}

QString CommandlineOptions::DecodeName(char *opt) {

  return QFile::decodeName(opt);

}
#endif

QDataStream &operator<<(QDataStream &s, const CommandlineOptions &a) {

  s << static_cast<quint32>(a.player_action_)
    << static_cast<quint32>(a.url_list_action_)
    << a.set_volume_
    << a.volume_modifier_
    << a.seek_to_
    << a.seek_by_
    << a.play_track_at_
    << a.show_osd_
    << a.urls_
    << a.log_levels_
    << a.toggle_pretty_osd_
    << a.playlist_name_
    << a.window_size_;

  return s;

}

QDataStream &operator>>(QDataStream &s, CommandlineOptions &a) {

  quint32 player_action = 0;
  quint32 url_list_action = 0;

  s >> player_action
    >> url_list_action
    >> a.set_volume_
    >> a.volume_modifier_
    >> a.seek_to_
    >> a.seek_by_
    >> a.play_track_at_
    >> a.show_osd_
    >> a.urls_
    >> a.log_levels_
    >> a.toggle_pretty_osd_
    >> a.playlist_name_
    >> a.window_size_;

  a.player_action_ = static_cast<CommandlineOptions::PlayerAction>(player_action);
  a.url_list_action_ = static_cast<CommandlineOptions::UrlListAction>(url_list_action);

  return s;

}
