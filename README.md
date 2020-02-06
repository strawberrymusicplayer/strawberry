:strawberry: Strawberry Music Player [![Build Status](https://github.com/strawberrymusicplayer/strawberry/workflows/C/C++%20CI/badge.svg)](https://github.com/strawberrymusicplayer/strawberry/actions)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/jonaskvinge)
=======================

Strawberry is a music player and music collection organizer. It is a fork of Clementine released in 2018 aimed at music collectors, audio enthusiasts and audiophiles. The name is inspired by the band Strawbs. It's based on a heavily modified version of Clementine created in 2012-2013. It's written in C++ and Qt 5.

  * Website: https://www.strawberrymusicplayer.org/
  * Github: https://github.com/strawberrymusicplayer/strawberry
  * Buildbot: http://buildbot.strawberrymusicplayer.org/
  * Latest builds: https://builds.strawberrymusicplayer.org/

### :heavy_check_mark: Features:

  * Play and organize music
  * Supports WAV, FLAC, WavPack, DSF, DSDIFF, Ogg FLAC, Ogg Vorbis, Ogg Opus, Ogg Speex, MPC, TrueAudio, AIFF, MP4, MP3, ASF and Monkey's Audio.
  * Audio CD playback
  * Native desktop notifications
  * Playlists in multiple formats
  * Advanced audio output and device configuration for bit-perfect playback on Linux
  * Edit tags on music files
  * Fetch tags from MusicBrainz
  * Album cover art from [Last.fm](https://www.last.fm/), [Musicbrainz](https://musicbrainz.org/), [Discogs](https://www.discogs.com/), [Deezer](https://www.deezer.com/) and [Tidal](https://tidal.com/)
  * Song lyrics from [AudD](https://audd.io/), [lyrics.ovh](https://lyrics.ovh/) and [lololyrics.com](https://www.lololyrics.com/)
  * Support for multiple backends
  * Audio analyzer
  * Audio equalizer
  * Transfer music to iPod, iPhone, MTP or mass-storage USB player
  * Subsonic streaming support
  * Unofficial streaming support for [Tidal](https://tidal.com/) and [Qobuz](https://www.qobuz.com/)
  * Scrobbler with support for [Last.fm](https://www.last.fm/), [Libre.fm](https://libre.fm/) and [ListenBrainz](https://listenbrainz.org/)
  
**Tidal and Qobuz streaming in Strawberry is unofficial. You need an official API token (or App ID/Secret) to use it, we can not provide API tokens, or help getting them. Tidal will not work with Tidal Masters (MQA), because MQA is a proprietary format in lossy quality without an open source decoder, we can't support it.**

It has so far been tested to work on Linux, OpenBSD and Windows.

**We do not provide releases for macOS. There currently isn't any macOS developers actively working on this project.**

### :heavy_exclamation_mark: Requirements

To build Strawberry from source you need the following installed on your system with the additional development packages/headers:

* [CMake and Make tools](https://cmake.org/)
* [GCC](https://gcc.gnu.org/) or [clang](https://clang.llvm.org/) compiler
* [Boost](https://www.boost.org/)
* [POSIX thread (pthread)](http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html)
* [GLib](https://developer.gnome.org/glib/)
* [Protobuf library and compiler](https://developers.google.com/protocol-buffers/)
* [Qt 5.5 or higher with components Core, Gui, Widgets, Concurrent, Network and Sql](https://www.qt.io/)
* [Qt 5 components X11Extras and DBus for Linux/BSD, MacExtras for macOS and WinExtras for Windows](https://www.qt.io/)
* [SQLite 3.9 or newer with FTS5](https://www.sqlite.org)
* [Chromaprint library](https://acoustid.org/chromaprint)
* [ALSA library (linux)](https://www.alsa-project.org/)
* [DBus (linux)](https://www.freedesktop.org/wiki/Software/dbus/)
* [PulseAudio (linux optional)](https://www.freedesktop.org/wiki/Software/PulseAudio/?)
* [GStreamer](https://gstreamer.freedesktop.org/), [Xine](https://www.xine-project.org), [VLC](https://www.videolan.org) or [Phonon](https://techbase.kde.org/Phonon)
* [GnuTLS](https://www.gnutls.org/)

Optional dependencies:

* Audio CD: [libcdio](https://www.gnu.org/software/libcdio/)
* MTP devices: [libmtp](http://libmtp.sourceforge.net/)
* iPod Classic devices: [libgpod](http://www.gtkpod.org/libgpod/)
* iPhone, iPod Touch, iPad and Apple TV devices: [libimobiledevice, libplist and libusbmuxd](https://www.libimobiledevice.org/)
* Moodbar: [fftw3](http://www.fftw.org/)

Either GStreamer, Xine, VLC or Phonon engine is required, but only GStreamer is fully implemented so far.
You should also install the gstreamer plugins base and good, and optionally bad and ugly.

### :wrench:	Compiling from source

### Get the code:

    git clone https://github.com/strawberrymusicplayer/strawberry

### Compile and install:

    cd strawberry
    mkdir build && cd build
    cmake ..
    make -j4
    sudo make install

### :penguin:	Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/strawberry.svg)](https://repology.org/metapackage/strawberry/versions)

### :computer:	Screenshot


![Browse](https://www.strawberrymusicplayer.org/pictures/screenshot-002-large.png)

### :moneybag: Donate

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://paypal.me/jonaskvinge)
