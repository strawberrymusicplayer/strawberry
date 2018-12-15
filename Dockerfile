from opensuse:tumbleweed

run zypper --non-interactive --gpg-auto-import-keys ref
run zypper --non-interactive --gpg-auto-import-keys dup -l -y

run zypper --non-interactive --gpg-auto-import-keys install \
    lsb-release git tar make cmake gcc gcc-c++ pkg-config \
    glibc-devel glib2-devel glib2-tools dbus-1-devel alsa-devel libpulse-devel libnotify-devel \
    boost-devel protobuf-devel sqlite3-devel taglib-devel \
    gstreamer-devel gstreamer-plugins-base-devel libxine-devel vlc-devel \
    libQt5Core-devel libQt5Gui-devel libQt5Widgets-devel libQt5Concurrent-devel libQt5Network-devel libQt5Sql-devel libQt5Xml-devel \
    libQt5DBus-devel libqt5-qtx11extras-devel libQt5Gui-private-headers-devel libqt5-qtbase-common-devel \
    libcdio-devel libgpod-devel libplist-devel libmtp-devel libusbmuxd-devel libchromaprint-devel

run mkdir -p /usr/src/app
workdir /usr/src/app
copy . /usr/src/app
