:strawberry: Strawberry Music Player
=======================

Strawberry is a audio player and music collection organizer. It is a fork of Clementine created in 2013 with a diffrent goal.
It's written in C++ and Qt 5. The name is inspired by the band Strawbs.

### :heavy_check_mark: Features:

  * Play and organize music
  * Native desktop notifications
  * Playlists in multiple formats
  * Edit tags on music files
  * Fetch tags from MusicBrainz
  * Album cover art from Lastfm, Musicbrainz, Discogs and Amazon
  * Support for multiple backends
  * Transfer music to iPod, iPhone, MTP or mass-storage USB player

It has so far been tested to work on Linux, OpenBSD and Windows (cross compiled using mingw).


### :heavy_exclamation_mark: Requirements

To build Strawberry from source you need the following installed on your system with the additional development packages/headers:

* GLib, GIO and GObject
* POSIX thread (pthread) libraries
* CMake and Make tools
* GCC or clang compiler
* Protobuf library and compiler
* Boost development headers
* Qt 5 with components Core, Widgets, Network, Sql, Xml, OpenGL, Concurrent, Test, WebKitWidget, X11Extras and DBus
* SQLite3
* TagLib 1.11.1 or higher
* Chromaprint library
* libxml library
* ALSA library (linux)
* DBus (linux)
* PulseAudio (linux optional)

Either GStreamer, Xine or VLC engine is required, but only GStreamer is fully implemented so far.
You should also install the gstreamer plugins base and good, and optionally bad and ugly.

Optional:

* The Qt 5 LastFM library is required for fetching album covers from LastFM.
* To enable CD support for playing audio cd's you need libcdio.
* If you want MTP support you need libmtp.
* If you need iPod Classic support you need libgpod.

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
    

