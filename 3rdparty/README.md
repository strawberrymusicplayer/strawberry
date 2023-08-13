3rdparty libraries located in this directory
============================================

KDSingleApplication
-----------------
This is a small static library used by Strawberry to prevent it from starting twice per user session.
If the user tries to start strawberry twice, the main window will maximize instead of starting another instance.
It is also used to pass command-line options through to the first instance.

URL: https://github.com/KDAB/KDSingleApplication/


SPMediaKeyTap
-------------
Used on macOS to exclusively enable strawberry to grab global media shortcuts.
Can safely be deleted on other platforms.


getopt
------
getopt included only when compiling on Windows.
