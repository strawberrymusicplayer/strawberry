3rdparty libraries located in this directory
============================================

KDSingleApplication
-------------------
A small library used by Strawberry to prevent it from starting twice per user session.
If the user tries to start strawberry twice, the main window will maximize instead of starting another instance.
It is also used to pass command-line options through to the first instance.
This 3rdparty copy is used only if KDSingleApplication 1.1 or higher is not found on the system.

URL: https://github.com/KDAB/KDSingleApplication/


SPMediaKeyTap
-------------
A library used on macOS to exclusively grab global media shortcuts.

The library is no longer maintained by the original author.

The directory can safely be deleted on other platforms.

getopt
------
getopt included on Windows for command line options parsing with Unicode support .

The directory can safely be deleted on other platforms.

URL: https://github.com/ludvikjerabek/getopt-win
