// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SMARTPLAYLISTPREVIEW_ANDROID_H
#define SMARTPLAYLISTPREVIEW_ANDROID_H

#include "playlistgenerator_fwd.h"

class PlaylistManager;

namespace AndroidSmartPlaylistPreview {
void Open(PlaylistManager *manager, PlaylistGeneratorPtr generator);
}

#endif  // SMARTPLAYLISTPREVIEW_ANDROID_H
