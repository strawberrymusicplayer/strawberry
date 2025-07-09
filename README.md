:strawberry: Strawberry Music Player [![Build Status](https://github.com/strawberrymusicplayer/strawberry/workflows/Build/badge.svg)](https://github.com/strawberrymusicplayer/strawberry/actions)
=======================
[![Sponsor](https://img.shields.io/badge/-Sponsor-green?logo=github)](https://github.com/sponsors/jonaski)
[![Patreon](https://img.shields.io/badge/patreon-donate-green.svg)](https://patreon.com/jonaskvinge)
[![PayPal](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/jonaskvinge)

Strawberry is a music player and music collection organizer. It is a fork of Clementine released in 2018 aimed at music collectors and audiophiles. It's written in C++ using the Qt framework.

![Browse](https://raw.githubusercontent.com/strawberrymusicplayer/strawberry/master/data/screenshot/screenshot.png)

Resources:

  * Website: https://www.strawberrymusicplayer.org/
  * Wiki: https://wiki.strawberrymusicplayer.org/
  * Forum: https://forum.strawberrymusicplayer.org/
  * Github: https://github.com/strawberrymusicplayer/strawberry
  * Latest builds: https://builds.strawberrymusicplayer.org/
  * openSUSE buildservice: https://build.opensuse.org/package/show/home:jonaski:audio/strawberry
  * Ubuntu PPA: https://launchpad.net/~jonaski/+archive/ubuntu/strawberry
  * Ubuntu Unstable PPA: https://launchpad.net/~jonaski/+archive/ubuntu/strawberry-unstable
  * Translations: https://crowdin.com/project/strawberrymusicplayer/

### :bangbang: Opening an issue

* Read the FAQ: https://wiki.strawberrymusicplayer.org/wiki/FAQ
* Search for the issue to see if it is already solved, or if there is an open issue for it already. If there is an open issue already, you can comment on it if you have additional information that could be useful to us.
* For technical problems, discussion, questions and feature suggestions use the forum (https://forum.strawberrymusicplayer.org/) instead. The forum is better suited for discussion.
* We do not take feature requests from users on GitHub. Any issues related to feature requests will be closed. This does not necessarily mean that we won't add new features, but we don't have time to take feature requests or answer questions about new features from users. It is still possible to suggest or discuss new features on the forum (https://forum.strawberrymusicplayer.org/).
* We do not maintain the Flatpak package. Do not report issues related to Flatpak unless the issue can be reproduced with a native package, use Flatpak support instead https://flatpak.org/about/

### :moneybag: Sponsoring

The program is free software, released under GPL. If you like this program and can make use of it, consider sponsoring or donating to help fund the project.
There are currently 4 options for sponsoring:

1. [Patreon](https://www.patreon.com/jonaskvinge)
2. [GitHub](https://github.com/sponsors/jonaski)
3. [Ko-fi](https://ko-fi.com/jonaskvinge)
4. [PayPal](https://paypal.me/jonaskvinge)

Funding developers is a way to contribute to open source projects you appreciate, it helps developers get the resources they need, and recognize contributors working behind the scenes to make open source better for everyone.

### :heavy_check_mark: Features

  * Play and organize music
  * Supports WAV, FLAC, WavPack, Ogg FLAC, Ogg Vorbis, Ogg Opus, Ogg Speex, MPC, TrueAudio, AIFF, MP4, MP3, ASF and Monkey's Audio.
  * Audio CD playback
  * Native desktop notifications
  * Playlist management
  * Smart and dynamic playlists
  * Advanced audio output and device configuration for bit-perfect playback on Linux
  * In-player song loudness analysis and song playback loudness normalization, as per EBU R 128
  * Edit tags on audio files
  * Fetch tags from MusicBrainz
  * Album cover art from [Last.fm](https://www.last.fm/), [Musicbrainz](https://musicbrainz.org/), [Discogs](https://www.discogs.com/), [Musixmatch](https://www.musixmatch.com/), [Deezer](https://www.deezer.com/), [Tidal](https://www.tidal.com/), [Qobuz](https://www.qobuz.com/) and [Spotify](https://www.spotify.com/)
  * Song lyrics from [Genius](https://genius.com/), [Musixmatch](https://www.musixmatch.com/), [ChartLyrics](http://www.chartlyrics.com/), [lyrics.ovh](https://lyrics.ovh/), [lololyrics.com](https://www.lololyrics.com/), [songlyrics.com](https://www.songlyrics.com/), [azlyrics.com](https://www.azlyrics.com/), [elyrics.net](https://www.elyrics.net/), [letras.mus.br](https://www.letras.mus.br) and [LyricFind](https://lyrics.lyricfind.com)
  * Support for multiple backends
  * Audio analyzer
  * Audio equalizer
  * Transfer music to mass-storage USB players, MTP compatible devices and iPod Nano/Classic
  * Scrobbler with support for [Last.fm](https://www.last.fm/), [Libre.fm](https://libre.fm/) and [ListenBrainz](https://listenbrainz.org/)
  * Streaming from Subsonic compatible servers
  * Unofficial Tidal, Spotify and Qobuz integration
  * Discord rich presence


It has so far been tested to work on Linux, OpenBSD, FreeBSD, macOS and Windows.

**Access to macOS and Windows releases are currently restricted to sponsors, a 5 USD monthly sponsorship is required. You can sponsor strawberry through <a href="https://www.patreon.com/jonaskvinge">Patreon</a> for direct access to new releases. If you are sponsoring through GitHub, Ko-fi or PayPal, please e-mail support AT strawberrymusicplayer.org for access to downloads.**

### :heavy_exclamation_mark: Requirements

To build Strawberry from source you need the following installed on your system with the additional development packages/headers:

* [CMake 3.13 or higher](https://cmake.org/)
* C/C++ compiler ([GCC](https://gcc.gnu.org/), [Clang](https://clang.llvm.org/) or [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/))
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) or [pkgconf](https://github.com/pkgconf/pkgconf)
* [Boost](https://www.boost.org/)
* [GLib](https://developer.gnome.org/glib/)
* [Qt 6.4.0 or higher with components Core, Concurrent, Gui, Widgets, Network, Sql and D-Bus](https://www.qt.io/)
* [SQLite 3.9 or newer](https://www.sqlite.org)
* [ALSA (Required on Linux)](https://www.alsa-project.org/)
* [GStreamer](https://gstreamer.freedesktop.org/)
* [TagLib 1.12 or higher](https://www.taglib.org/)
* [ICU](https://unicode-org.github.io/icu/)
* [KDSingleApplication 1.1.0 or higher](https://github.com/KDAB/KDSingleApplication)

Optional dependencies:

* Song fingerprinting and MusicBrainz tagging: [Chromaprint](https://acoustid.org/chromaprint)
* Moodbar: [fftw3](http://www.fftw.org/)
* PulseAudio integration: [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/?)
* Audio CD: [libcdio](https://www.gnu.org/software/libcdio/)
* MTP devices: [libmtp](http://libmtp.sourceforge.net/)
* iPod Classic devices: [libgpod](http://www.gtkpod.org/libgpod/)
* EBU R 128 loudness normalization [libebur128](https://github.com/jiixyj/libebur128)
* Discord rich presence [RapidJSON](https://rapidjson.org/)

You should also install the gstreamer plugins base and good, and optionally bad, ugly and libav to support all audio formats.

### :wrench: Build from source

### Get the code:

    git clone --recursive https://github.com/strawberrymusicplayer/strawberry

### Build and install:

    cd strawberry
    cmake -S . -B build
    cmake --build build --parallel $(nproc)
    sudo cmake --install build

To build on Windows with Visual Studio 2022, see https://github.com/strawberrymusicplayer/strawberry-msvc

### :penguin: Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/strawberry.svg?columns=3&header=Strawberry&exclude_unsupported=1)](https://repology.org/metapackage/strawberry/versions)
