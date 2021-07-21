:strawberry: Strawberry Music Player [![Build Status](https://github.com/strawberrymusicplayer/strawberry/workflows/C/C++%20CI/badge.svg)](https://github.com/strawberrymusicplayer/strawberry/actions)
[![PayPal](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/jonaskvinge)
[![Patreon](https://img.shields.io/badge/patreon-donate-green.svg)](https://patreon.com/jonaskvinge)
=======================

Strawberry is a music player and music collection organizer. It is a fork of Clementine released in 2018 aimed at music collectors and audiophiles. It's written in C++ using the Qt toolkit.

![Browse](https://raw.githubusercontent.com/strawberrymusicplayer/strawberry/master/data/screenshot/screenshot.png)

Resources:

  * Website: https://www.strawberrymusicplayer.org/
  * Forum: https://forum.strawberrymusicplayer.org/
  * Github: https://github.com/strawberrymusicplayer/strawberry
  * Buildbot: https://buildbot.strawberrymusicplayer.org/
  * Latest builds: https://builds.strawberrymusicplayer.org/
  * openSUSE buildservice: https://build.opensuse.org/package/show/home:jonaski:audio/strawberry
  * Ubuntu PPA: https://launchpad.net/~jonaski/+archive/ubuntu/strawberry
  * Ubuntu Unstable PPA: https://launchpad.net/~jonaski/+archive/ubuntu/strawberry-unstable
  * Translations: https://translate.zanata.org/iteration/view/strawberry/master

### :bangbang: Opening an issue:

* Search for the issue to see if it is already solved, or if there is an open issue for it already. If there is an open issue already, you can comment on it if you have additional information that could be useful to us.
* For technical problems, questions and feature requests please use our forum on https://forum.strawberrymusicplayer.org/ that is better suited for discussion. It also better allows answers from the community instead of just the developers on GitHub.

### :moneybag:	Sponsoring:

The program is free software, released under GPL. If you like this program and can make use of it, consider sponsoring or donating to help fund the project.
There are currently 3 options for sponsoring:

1. [GitHub Sponsors](https://github.com/sponsors/jonaski)
2. [Patreon](https://www.patreon.com/jonaskvinge)
3. [PayPal](https://paypal.me/jonaskvinge)

Funding developers is a way to contribute to open source projects you appreciate, it helps developers get the resources they need, and recognize contributors working behind the scenes to make open source better for everyone.

### :heavy_check_mark: Features:

  * Play and organize music
  * Supports WAV, FLAC, WavPack, Ogg FLAC, Ogg Vorbis, Ogg Opus, Ogg Speex, MPC, TrueAudio, AIFF, MP4, MP3, ASF and Monkey's Audio.
  * Audio CD playback
  * Native desktop notifications
  * Playlist management
  * Smart and dynamic playlists
  * Advanced audio output and device configuration for bit-perfect playback on Linux
  * Edit tags on audio files
  * Fetch tags from MusicBrainz
  * Album cover art from [Last.fm](https://www.last.fm/), [Musicbrainz](https://musicbrainz.org/), [Discogs](https://www.discogs.com/), [Musixmatch](https://www.musixmatch.com/), [Deezer](https://www.deezer.com/), [Tidal](https://www.tidal.com/), [Qobuz](https://www.qobuz.com/) and [Spotify](https://www.spotify.com/)
  * Song lyrics from [AudD](https://audd.io/), [Genius](https://genius.com/), [Musixmatch](https://www.musixmatch.com/), [ChartLyrics](http://www.chartlyrics.com/), [lyrics.ovh](https://lyrics.ovh/) and [lololyrics.com](https://www.lololyrics.com/)
  * Support for multiple backends
  * Audio analyzer
  * Audio equalizer
  * Transfer music to mass-storage USB players, MTP compatible devices and iPod Nano/Classic
  * Scrobbler with support for [Last.fm](https://www.last.fm/), [Libre.fm](https://libre.fm/) and [ListenBrainz](https://listenbrainz.org/)
  * Subsonic, Tidal and Qobuz streaming support


It has so far been tested to work on Linux, OpenBSD, FreeBSD, macOS and Windows.

**There currently isn't any macOS developers actively working on this project, so we might not be able to help you with issues related to macOS.**

### :heavy_exclamation_mark: Requirements

To build Strawberry from source you need the following installed on your system with the additional development packages/headers:

* [CMake](https://cmake.org/)
* [GNU Make](https://www.gnu.org/software/make/)
* [GCC](https://gcc.gnu.org/) or [clang](https://clang.llvm.org/) compiler
* [Boost](https://www.boost.org/)
* [GLib](https://developer.gnome.org/glib/)
* [Protobuf](https://developers.google.com/protocol-buffers/)
* [Qt 5.8 or higher (or Qt 6) with components Core, Gui, Widgets, Concurrent, Network and Sql](https://www.qt.io/)
* [SQLite 3.9 or newer](https://www.sqlite.org)
* [ALSA (Linux required)](https://www.alsa-project.org/)
* [D-Bus (Linux required)](https://www.freedesktop.org/wiki/Software/dbus/)
* [GStreamer](https://gstreamer.freedesktop.org/) or [VLC](https://www.videolan.org)
* [GnuTLS](https://www.gnutls.org/)
* [TagLib 1.11.1 or higher](https://www.taglib.org/) or [TagParser](https://github.com/Martchus/tagparser)

Optional dependencies:

* Song fingerprinting and MusicBrainz tagging: [Chromaprint](https://acoustid.org/chromaprint)
* Moodbar: [fftw3](http://www.fftw.org/)
* PulseAudio integration: [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/?)
* Audio CD: [libcdio](https://www.gnu.org/software/libcdio/)
* MTP devices: [libmtp](http://libmtp.sourceforge.net/)
* iPod Classic devices: [libgpod](http://www.gtkpod.org/libgpod/)

Either GStreamer or VLC engine is required, but only GStreamer is fully implemented, and works best, it is therefore recommended to use GStreamer.
You should also install the gstreamer plugins base and good, and optionally bad, ugly and libav.

### :wrench:	Compiling from source

### Get the code:

    git clone https://github.com/strawberrymusicplayer/strawberry

### Compile and install:

    cd strawberry
    mkdir build && cd build
    cmake ..
    make -j$(nproc)
    sudo make install
    
To compile with Qt 6 use:

    cmake .. -DBUILD_WITH_QT6=ON

### :penguin:	Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/strawberry.svg)](https://repology.org/metapackage/strawberry/versions)

