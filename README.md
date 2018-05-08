Strawberry Music Player
=======================
README
------

Strawberry is a audio player and music collection organizer. It is a fork of Clementine created in 2013 with a diffrent goal.
It's written in C++ and Qt5. The name is inspired by the band Strawbs.

### Features:

  * Play and organize music
  * Native desktop notifications
  * Playlists in multiple formats
  * Edit tags on music files
  * Fetch tags from MusicBrainz
  * Album cover art from Lastfm, Musicbrainz, Discogs and Amazon
  * Support for multiple backends
  * Transfer music to iPod, iPhone, MTP or mass-storage USB player

You can obtain and view the sourcecode on github at: https://github.com/jonaski/strawberry

It has so far been tested on Linux and cross compiled for Windows. I have not had a chance to test it on Mac OS X since I don't have a mac.


Requirements
------------

To build Strawberry from source you need the following installed on your system:

* glib2, glib2-devel, git, cmake, make, gcc and gcc-c++
* protobuf and development packages
* boost development headers

* The following Qt5 components are required with additional development packages: Qt5Core, Qt5Widgets, Qt5Network, Qt5Sql, Qt5Xml, Qt5OpenGL, Qt5Concurrent, Qt5Test, 5X11Extras, Qt5WebKit, Qt5WebKitWidget and Qt5DBus.

* ALSA and libasound2 with development files
* SQLite3 with development files
* TagLib 1.8 or higher with development files
* libchromaprint with development files
* libglu with development files

Either GStreamer, Xine or VLC engine is required, but only GStreamer is fully implemented so far.
You should also install the gstreamer plugins: gstreamer-plugins-base, gstreamer-plugins-good and gstreamer-plugins-bad

* The Qt5 specific LastFM library and development files are required for fetching album covers from LastFM.
* To enable CD support for playing audio cd's you need libcdio.
* If you want MTP support you need libmtp.
* If you need iPod Classic support you need libgpod.


Compiling from source
---------------------

### Get the code:

    git clone https://github.com/jonaski/strawberry

### Compile and install:

    mkdir strawberry-build
    cd strawberry-build
    cmake ../strawberry
    make -j8
    sudo make install

    (dont change to the source directory, if you created the build directory inside the source directory type: cmake .. instead).
    

