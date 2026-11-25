// SPDX-License-Identifier: GPL-3.0-or-later

#include "smartplaylistpreview_android.h"

#include <QObject>
#include <QString>

#include "playlist/playlist.h"
#include "playlist/playlistmanager.h"

namespace AndroidSmartPlaylistPreview {
namespace {
constexpr char kPreviewType[] = "android-smart-playlist-preview";
}

void Open(PlaylistManager *manager, PlaylistGeneratorPtr generator) {
  Q_ASSERT(manager);
  Q_ASSERT(generator);
  Playlist *preview = nullptr;
  for (Playlist *playlist : manager->GetAllPlaylists()) {
    if (playlist->special_type() == QLatin1String(kPreviewType) && playlist->id() != manager->active_id() && !playlist->is_favorite()) {
      preview = playlist;
      break;
    }
  }
  if (preview) {
    manager->SetCurrentPlaylist(preview->id());
  }
  else {
    manager->New(QObject::tr("Smart playlist preview"), SongList(), QLatin1String(kPreviewType));
    preview = manager->current();
  }
  preview->Clear();
  preview->InsertSmartPlaylist(generator);
}

}  // namespace AndroidSmartPlaylistPreview
