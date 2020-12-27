3rdparty libraries located in this directory
============================================

singleapplication
-----------------
This is a small static library used by Strawberry to prevent it from starting twice per user session.
If the user tries to start strawberry twice, the main window will maximize instead of starting another instance.
If you dynamically link to your systems version, you'll need two versions, one defined as QApplication and
one as a QCoreApplication.
It is included here because it is not packed by distros and is also used on macOS and Windows.

URL: https://github.com/itay-grudev/SingleApplication


taglib
------

TagLib is a library for reading and editing the meta-data of several popular audio formats. It is also used
by Strawberry to identify audio files. It is important that it is kept up-to-date for Strawberry to function
correctly.

It is kept in 3rdparty because there currently is no official release of TagLib with the features and bugfixes
that are in the official repository. And also because some distros use older, or unpatched versions.

There is a bug in the latest version (1.11.1) corrupting Ogg files,
see: https://github.com/taglib/taglib/issues/864
If you decide to use the systems taglib, make sure it has been patched with the following commit:
https://github.com/taglib/taglib/commit/9336c82da3a04552168f208cd7a5fa4646701ea4

The current taglib in 3rdparty also has the following features:
- Audio file detection by content.
- DSF and DSDIFF support

URL: https://github.com/taglib/taglib


utf8-cpp
--------

This is 2 header files used by taglib, but kept in a separate directory because it is maintained by others.

URL: http://utfcpp.sourceforge.net/


SPMediaKeyTap
-------------

This is used for macOS only to enable strawberry to grab global shortcuts and can safely be deleted on other
platforms.
