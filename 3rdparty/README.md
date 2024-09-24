3rdparty libraries located in this directory
============================================

KDSingleApplication
-------------------
A small library used by Strawberry to prevent it from starting twice per user session.
If the user tries to start strawberry twice, the main window will maximize instead of starting another instance.
It is also used to pass command-line options through to the first instance.
This 3rdparty copy is used only if KDSingleApplication 1.1 or higher is not found on the system.

URL: https://github.com/KDAB/KDSingleApplication/
