# :strawberry: Strawberry Music Player [![Build Status](https://github.com/strawberrymusicplayer/strawberry/workflows/Build/badge.svg)](https://github.com/strawberrymusicplayer/strawberry/actions)
[![Sponsor](https://img.shields.io/badge/-Sponsor-green?logo=github)](https://github.com/sponsors/jonaski)
[![Patreon](https://img.shields.io/badge/patreon-donate-green.svg)](https://patreon.com/jonaskvinge)
[![PayPal](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/jonaskvinge)

Strawberry is a **music player and music collection organizer**, originally forked from *Clementine* in 2018.
It’s written in **C++ using the Qt framework**, designed for **audiophiles and music collectors**.

![Screenshot of Strawberry Music Player](https://raw.githubusercontent.com/strawberrymusicplayer/strawberry/master/data/screenshot/screenshot.png)

---

## :globe_with_meridians: Resources

- **Website:** https://www.strawberrymusicplayer.org
- **Wiki:** https://wiki.strawberrymusicplayer.org
- **Forum:** https://forum.strawberrymusicplayer.org
- **GitHub:** https://github.com/strawberrymusicplayer/strawberry
- **Latest builds:** https://builds.strawberrymusicplayer.org
- **openSUSE Build Service:** https://build.opensuse.org/package/show/home:jonaski:audio/strawberry
- **Ubuntu PPAs:**
  - Stable: https://launchpad.net/~jonaski/+archive/ubuntu/strawberry
  - Unstable: https://launchpad.net/~jonaski/+archive/ubuntu/strawberry-unstable
- **Translations:** https://crowdin.com/project/strawberrymusicplayer

---

## :warning: Opening an Issue

Before creating a new GitHub issue:

1. **Read the [FAQ](https://wiki.strawberrymusicplayer.org/wiki/FAQ)**.
2. **Search existing issues** to avoid duplicates. If one already exists, comment there with any additional information.
3. **Use the [forum](https://forum.strawberrymusicplayer.org/)** for technical problems, discussions or feature suggestions — it’s better suited for back-and-forth conversation.
4. **Feature requests are not accepted on GitHub.** Issues created for feature requests will be closed. You can still discuss ideas on the forum.
5. **Flatpak users:** We do **not** maintain the Flatpak package. Report Flatpak-specific issues via [Flatpak support](https://flatpak.org/about/).

---

## :moneybag: Sponsoring

Strawberry is **free software released under the GPL**.
If you enjoy using it, please consider **supporting development** through sponsorship or donation.

**Sponsorship options:**
1. [Patreon](https://www.patreon.com/jonaskvinge)
2. [GitHub](https://github.com/sponsors/jonaski)
3. [Ko-fi](https://ko-fi.com/jonaskvinge)
4. [PayPal](https://paypal.me/jonaskvinge)

Supporting open-source developers helps ensure continued maintenance and improvements.

---

## :white_check_mark: Features

- Play and organize your music collection
- Supports formats: WAV, FLAC, WavPack, Ogg Vorbis, Opus, MPC, TrueAudio, AIFF, MP4, MP3, ASF, and Monkey’s Audio
- Audio CD playback
- Bit-perfect playback on Linux
- Native desktop notifications
- Advanced playlist management
- Smart and dynamic playlists
- Loudness analysis and EBU R128 normalization
- Editing tags and fetching missing tags via [MusicBrainz](https://musicbrainz.org/)
- Album art from: [Last.fm](https://www.last.fm/), [MusicBrainz](https://musicbrainz.org/), [Discogs](https://www.discogs.com/), [Musixmatch](https://www.musixmatch.com/), [Deezer](https://www.deezer.com/), [Tidal](https://www.tidal.com/), [Qobuz](https://www.qobuz.com/), [Spotify](https://www.spotify.com/)
- Lyrics from: [Genius](https://genius.com/), [Musixmatch](https://www.musixmatch.com/), [ChartLyrics](http://www.chartlyrics.com/), [lyrics.ovh](https://lyrics.ovh/), [lololyrics](https://www.lololyrics.com/), [songlyrics](https://www.songlyrics.com/), [azlyrics](https://www.azlyrics.com/), [elyrics](https://www.elyrics.net/), [letras](https://www.letras.mus.br), [LyricFind](https://lyrics.lyricfind.com)
- Audio analyzer and equalizer
- Transfer music to USB, MTP and iPod devices
- Scrobbling to [Last.fm](https://www.last.fm/) and [ListenBrainz](https://listenbrainz.org/)
- Streaming from Subsonic-compatible servers
- Unofficial integrations: Tidal, Spotify, and Qobuz
- Discord Rich Presence

---

:white_check_mark: Tested on **Linux**, **OpenBSD**, **FreeBSD**, **macOS**, and **Windows**.

> **Note:** macOS and Windows releases are currently **available to sponsors only**.
> A monthly sponsorship via [Patreon](https://www.patreon.com/jonaskvinge) grants direct access to new releases.

---

## :gear: Requirements

To build Strawberry from source, you’ll need:

**Dependencies:**
- [CMake ≥= 3.13](https://cmake.org/)
- C/C++ compiler ([GCC](https://gcc.gnu.org/), [Clang](https://clang.llvm.org/), or [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/))
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) or [pkgconf](https://github.com/pkgconf/pkgconf)
- [Boost](https://www.boost.org/)
- [GLib](https://developer.gnome.org/glib/)
- [Qt ≥= 6.4](https://www.qt.io/) (Core, Concurrent, Gui, Widgets, Network, SQL, D-Bus)
- [SQLite ≥= 3.9](https://www.sqlite.org)
- [ALSA (Linux only)](https://www.alsa-project.org/)
- [GStreamer](https://gstreamer.freedesktop.org/)
- [TagLib ≥= 1.12](https://www.taglib.org/)
- [ICU](https://unicode-org.github.io/icu/)
- [KDSingleApplication ≥= 1.1.0](https://github.com/KDAB/KDSingleApplication)

**Dependencies for optional features:**
- Fingerprinting & tagging: [Chromaprint](https://acoustid.org/chromaprint)
- Moodbar: [FFTW3](http://www.fftw.org/)
- PulseAudio integration: [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)
- Audio CD support: [libcdio](https://www.gnu.org/software/libcdio/)
- MTP devices: [libmtp](http://libmtp.sourceforge.net/)
- iPod Classic: [libgpod](http://www.gtkpod.org/libgpod/)
- EBU R128 normalization: [libebur128](https://github.com/jiixyj/libebur128)
- Discord presence: [RapidJSON](https://rapidjson.org/)

Also install GStreamer plugins **base**, **good**, and optionally **bad**, **ugly** and **libav** for full codec support.

---

## :wrench: Build from Source

**Get the code:**

    git clone --recursive https://github.com/strawberrymusicplayer/strawberry

**Build and install:**

    cd strawberry
    cmake -S . -B build
    cmake --build build --parallel $(nproc)
    sudo cmake --install build

For building on Windows with Visual Studio 2022, see: :point_right: https://github.com/strawberrymusicplayer/strawberry-msvc

---

## :package: Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/strawberry.svg?columns=3&header=Strawberry&exclude_unsupported=1)](https://repology.org/metapackage/strawberry/versions)
