/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "metatypes.h"

#ifdef HAVE_GSTREAMER
#  include <gst/gstbuffer.h>
#  include <gst/gstelement.h>
#endif

#include <QDataStream>
#include <QAbstractSocket>
#include <QMetaType>
#include <QFileInfo>
#include <QList>
#include <QVector>
#include <QMap>
#include <QByteArray>
#include <QUrl>
#include <QImage>
#include <QNetworkReply>
#include <QItemSelection>
#ifdef HAVE_DBUS
#  include <QDBusMetaType>
#  include <QDBusArgument>
#endif

#include "song.h"

#include "engine/enginebase.h"
#include "engine/enginemetadata.h"
#ifdef HAVE_GSTREAMER
#  include "engine/gstenginepipeline.h"
#endif
#include "collection/collectiondirectory.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistsequence.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/albumcoverfetcher.h"
#include "covermanager/coversearchstatistics.h"
#include "equalizer/equalizer.h"

#ifdef HAVE_DBUS
#  include "mpris2.h"
#  include "osd/osddbus.h"
#  include "dbus/metatypes.h"
#endif

#include "streaming/streamingsearchview.h"

#include "smartplaylists/playlistgenerator_fwd.h"

#include "radios/radiochannel.h"

#ifdef HAVE_LIBMTP
#  include "device/mtpconnection.h"
#endif

#include "settings/playlistsettingspage.h"

#include "smartplaylists/smartplaylistsearchterm.h"
#include "smartplaylists/smartplaylistsitem.h"

#include "lyrics/lyricssearchresult.h"

void RegisterMetaTypes() {

  qRegisterMetaType<const char*>("const char*");
  qRegisterMetaType<QList<int>>("QList<int>");
  qRegisterMetaType<QVector<int>>("QVector<int>");
  qRegisterMetaType<QList<QUrl>>("QList<QUrl>");
  qRegisterMetaType<QFileInfo>("QFileInfo");
  qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
  qRegisterMetaType<QNetworkReply*>("QNetworkReply*");
  qRegisterMetaType<QNetworkReply**>("QNetworkReply**");
  qRegisterMetaType<QItemSelection>("QItemSelection");
  qRegisterMetaType<QMap<int, Qt::Alignment>>("ColumnAlignmentMap");
  qRegisterMetaType<QMap<int, int>>("ColumnAlignmentIntMap");
  qRegisterMetaType<Song>("Song");
  qRegisterMetaType<SongList>("SongList");
  qRegisterMetaType<SongMap>("SongMap");
  qRegisterMetaType<Song::Source>("Song::Source");
  qRegisterMetaType<Song::FileType>("Song::FileType");
  qRegisterMetaType<EngineBase::Type>("EngineBase::Type");
  qRegisterMetaType<EngineBase::State>("EngineBase::State");
  qRegisterMetaType<EngineBase::TrackChangeFlags>("EngineBase::TrackChangeFlags");
  qRegisterMetaType<EngineBase::OutputDetails>("EngineBase::OutputDetails");
  qRegisterMetaType<EngineMetadata>("EngineMetadata");
#ifdef HAVE_GSTREAMER
  qRegisterMetaType<GstBuffer*>("GstBuffer*");
  qRegisterMetaType<GstElement*>("GstElement*");
  qRegisterMetaType<GstState>("GstState");
  qRegisterMetaType<GstEnginePipeline*>("GstEnginePipeline*");
#endif
  qRegisterMetaType<CollectionDirectory>("CollectionDirectory");
  qRegisterMetaType<CollectionDirectoryList>("CollectionDirectoryList");
  qRegisterMetaType<CollectionSubdirectory>("CollectionSubdirectory");
  qRegisterMetaType<CollectionSubdirectoryList>("CollectionSubdirectoryList");
  qRegisterMetaType<CollectionModel::Grouping>("CollectionModel::Grouping");
  qRegisterMetaType<PlaylistItemPtr>("PlaylistItemPtr");
  qRegisterMetaType<PlaylistItemPtrList>("PlaylistItemPtrList");
  qRegisterMetaType<PlaylistSequence::RepeatMode>("PlaylistSequence::RepeatMode");
  qRegisterMetaType<PlaylistSequence::ShuffleMode>("PlaylistSequence::ShuffleMode");
  qRegisterMetaType<AlbumCoverLoaderResult>("AlbumCoverLoaderResult");
  qRegisterMetaType<AlbumCoverLoaderResult::Type>("AlbumCoverLoaderResult::Type");
  qRegisterMetaType<CoverProviderSearchResult>("CoverProviderSearchResult");
  qRegisterMetaType<CoverProviderSearchResults>("CoverProviderSearchResults");
  qRegisterMetaType<CoverSearchStatistics>("CoverSearchStatistics");

  qRegisterMetaType<Equalizer::Params>("Equalizer::Params");

#ifdef HAVE_DBUS
  qDBusRegisterMetaType<QByteArrayList>();
  qDBusRegisterMetaType<QImage>();
  qDBusRegisterMetaType<TrackMetadata>();
  qDBusRegisterMetaType<Track_Ids>();
  qDBusRegisterMetaType<MprisPlaylist>();
  qDBusRegisterMetaType<MprisPlaylistList>();
  qDBusRegisterMetaType<MaybePlaylist>();
  qDBusRegisterMetaType<InterfacesAndProperties>();
  qDBusRegisterMetaType<ManagedObjectList>();
#endif

  qRegisterMetaType<StreamingSearchView::Result>("StreamingSearchView::Result");
  qRegisterMetaType<StreamingSearchView::ResultList>("StreamingSearchView::ResultList");

  qRegisterMetaType<RadioChannel>("RadioChannel");
  qRegisterMetaType<RadioChannelList>("RadioChannelList");

#ifdef HAVE_LIBMTP
  qRegisterMetaType<MtpConnection*>("MtpConnection*");
#endif

  qRegisterMetaType<PlaylistSettingsPage::PathType>("PlaylistSettingsPage::PathType");

  qRegisterMetaType<PlaylistGeneratorPtr>("PlaylistGeneratorPtr");
  qRegisterMetaType<SmartPlaylistSearchTerm::Field>("SmartPlaylistSearchTerm::Field");
  qRegisterMetaType<SmartPlaylistSearchTerm::Operator>("SmartPlaylistSearchTerm::Operator");
  qRegisterMetaType<SmartPlaylistSearchTerm::OperatorList>("SmartPlaylistSearchTerm::OperatorList");
  qRegisterMetaType<SmartPlaylistSearchTerm::Type>("SmartPlaylistSearchTerm::Type");
  qRegisterMetaType<SmartPlaylistSearchTerm::DateType>("SmartPlaylistSearchTerm::DateType");
  qRegisterMetaType<SmartPlaylistsItem::Type>("SmartPlaylistsItem::Type");

  qRegisterMetaType<LyricsSearchResults>("LyricsSearchResults");

}
