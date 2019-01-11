3rdparty libraries located in this directory
============================================

singleapplication
-----------------
This is a small static library used by Strawberry to prevent it from starting twice per user session.
If the user tries to start strawberry twice, the main window will maximize instead of starting another instance.
If you dynamically link to your systems version, you'll need two versions, one defined as QApplication and
one as a QCoreApplication.
It is included here because it is normally not packaged by distros, and is also used on macOS and Windows.

URL: https://github.com/itay-grudev/SingleApplication


qocoa
--------------
This is a small static library currently used for the search fields above the collection, playlist and in
the cover manager. It is slightly modified from original version, so it should not be used as a dynamic
library.
The plan in the long run is to replace it with something else.

URL: https://github.com/mikemcquaid/Qocoa


SPMediaKeyTap
----------------------

This is used for macOS only to enable strawberry to grab global shortcuts and can safely be deleted on other
platforms.


taglib
---------------

TagLib is a library for reading and editing the meta-data of several popular audio formats. It is also used
by Strawberry to identify audio files. It is important that it is kept up-to-date for Strawberry to work
correctly.

It is kept in 3rdparty because there currently is no offical release of TagLib with the features and bugfixes
that are in the official repository. And also because some distros use older, or unpatched versions.
This version is a unmodified copy of commit 5cb589a (sha: 5cb589a5b82c13ba8f0542e5e79629da7645cb3c).

Also, there is a bug in version 1.11.1 corrupting Ogg files, see: https://github.com/taglib/taglib/issues/864
If you decide to use the systems taglib, make sure it has been patched with the following commit:
https://github.com/taglib/taglib/commit/9336c82da3a04552168f208cd7a5fa4646701ea4

The current taglib in 3rdparty also has the following features:
- Audio file detection by content.
- DSF and DSDIFF support

URL: https://github.com/taglib/taglib


utf8-cpp
-----------------

This is 2 header files used by taglib, but kept in a seperate directory because it is maintained by others.

URL: http://utfcpp.sourceforge.net/
