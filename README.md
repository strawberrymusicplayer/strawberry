:strawberry: Strawberry Music Player [![Build Status](https://travis-ci.org/jonaski/strawberry.svg?branch=master)](https://travis-ci.org/jonaski/strawberry)
=======================

Strawberry is a audio player and music collection organizer. It is a fork of Clementine created in 2013 with a diffrent goal.
It's written in C++ and Qt 5. The name is inspired by the band Strawbs.

  * Website: http://www.strawbs.org/
  * Github: https://github.com/jonaski/strawberry
  * Buildbot: http://buildbot.strawbs.net/
  * Latest builds: http://builds.strawbs.org/

### :heavy_check_mark: Features:

  * Play and organize music
  * Supports WAV, FLAC, WavPack, DSF, DSDIFF, Ogg Vorbis, Speex, MPC, TrueAudio, AIFF, MP4, MP3 and ASF
  * Audio CD playback
  * Native desktop notifications
  * Playlists in multiple formats
  * Advanced output and device options with support for bit perfect playback on Linux
  * Edit tags on music files
  * Fetch tags from MusicBrainz
  * Album cover art from Last.fm, Musicbrainz and Discogs
  * Song lyrics from AudD and API Seeds
  * Support for multiple backends
  * Audio analyzer
  * Equalizer
  * Transfer music to iPod, iPhone, MTP or mass-storage USB player
  * Integrated Tidal and Deezer support

It has so far been tested to work on Linux, OpenBSD, MacOs and Windows.

### :heavy_exclamation_mark: Requirements

To build Strawberry from source you need the following installed on your system with the additional development packages/headers:

* [GLib, GIO and GObject](https://developer.gnome.org/glib/)
* [POSIX thread (pthread) libraries](http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html)
* [CMake and Make tools](https://cmake.org/)
* [GCC](https://gcc.gnu.org/) or [clang](https://clang.llvm.org/) compiler
* [Protobuf library and compiler](https://developers.google.com/protocol-buffers/)
* [Boost development headers](https://www.boost.org/)
* [Qt 5 with components Core, Gui, Widgets, Concurrent, Network, Sql, Xml, OpenGL, X11Extras and DBus](https://www.qt.io/)
* [SQLite3](https://www.sqlite.org)
* [TagLib 1.11.1 or higher](http://taglib.org/)
* [Chromaprint library](https://acoustid.org/chromaprint)
* [ALSA library (linux)](https://www.alsa-project.org/)
* [DBus (linux)](https://www.freedesktop.org/wiki/Software/dbus/)
* [PulseAudio (linux optional)](https://www.freedesktop.org/wiki/Software/PulseAudio/?)

Either GStreamer, Xine or VLC engine is required, but only GStreamer is fully implemented so far.
You should also install the gstreamer plugins base and good, and optionally bad and ugly.

Deezer streams with full songs are encrypted and only urls for preview streams (MP3) are exposed by the API.
Full length songs requires the use of deezers own engine (Deezer SDK) or the dzmedia library (I dont have it).
Deezer SDK can be found here: https://build-repo.deezer.com/native_sdk/deezer-native-sdk-v1.2.10.zip

Optional:

* The Qt 5 LastFM library is required for fetching album covers from LastFM.
* [libcdio](https://www.gnu.org/software/libcdio/) - To enable Audio CD support
* [libmtp](http://libmtp.sourceforge.net/) - MTP support.
* [libgpod](http://www.gtkpod.org/libgpod/) - iPod Classic support.

### :wrench:	Compiling from source

### Get the code:

    git clone https://github.com/jonaski/strawberry

### Compile and install:

    mkdir strawberry-build
    cd strawberry-build
    cmake ../strawberry
    make -j8
    sudo make install

    (dont change to the source directory, if you created the build directory inside the source directory type: cmake .. instead).


