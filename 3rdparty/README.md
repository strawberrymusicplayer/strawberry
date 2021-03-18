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


SPMediaKeyTap
-------------
Used on macOS to exclusively enable strawberry to grab global media shortcuts.
Can safely be deleted on other platforms.


macdeployqt
-----------
A modified version of Qt's official macdeployqt utility that fixes some issues,
this version also deploys gstreamer plugins.
Can safely be deleted on other platforms.
