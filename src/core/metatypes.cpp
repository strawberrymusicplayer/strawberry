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
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QItemSelection>
#ifdef HAVE_DBUS
#  include <QDBusMetaType>
#  include <QDBusArgument>
#endif

#include "song.h"

#include "engine/engine_fwd.h"
#include "engine/enginebase.h"
#include "engine/enginetype.h"
#ifdef HAVE_GSTREAMER
#  include "engine/gstenginepipeline.h"
#endif
#include "collection/directory.h"
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

#include "internet/internetsearchview.h"

#include "smartplaylists/playlistgenerator_fwd.h"

#include "radios/radiochannel.h"
#include "widgets/collapsibleinfopane.h"

void RegisterMetaTypes() {

  qRegisterMetaType<const char*>("const char*");
  qRegisterMetaType<QList<int>>("QList<int>");
  qRegisterMetaType<QList<QUrl>>("QList<QUrl>");
  qRegisterMetaType<QVector<int>>("QVector<int>");
  qRegisterMetaType<QFileInfo>("QFileInfo");
  qRegisterMetaType<QAbstractSocket::SocketState>();
  qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
  qRegisterMetaType<QNetworkCookie>("QNetworkCookie");
  qRegisterMetaType<QList<QNetworkCookie>>("QList<QNetworkCookie>");
  qRegisterMetaType<QNetworkReply*>("QNetworkReply*");
  qRegisterMetaType<QNetworkReply**>("QNetworkReply**");
  qRegisterMetaType<QItemSelection>("QItemSelection");
  qRegisterMetaType<QMap<int, Qt::Alignment>>("ColumnAlignmentMap");
  qRegisterMetaType<QMap<int, int>>("ColumnAlignmentIntMap");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qRegisterMetaTypeStreamOperators<QMap<int, Qt::Alignment>>("ColumnAlignmentMap");
  qRegisterMetaTypeStreamOperators<QMap<int, int>>("ColumnAlignmentIntMap");
#endif
  qRegisterMetaType<Directory>("Directory");
  qRegisterMetaType<DirectoryList>("DirectoryList");
  qRegisterMetaType<Subdirectory>("Subdirectory");
  qRegisterMetaType<SubdirectoryList>("SubdirectoryList");
  qRegisterMetaType<Song>("Song");
  qRegisterMetaType<QList<Song>>("QList<Song>");
  qRegisterMetaType<SongList>("SongList");
  qRegisterMetaType<Engine::EngineType>("EngineType");
  qRegisterMetaType<Engine::SimpleMetaBundle>("Engine::SimpleMetaBundle");
  qRegisterMetaType<Engine::State>("Engine::State");
  qRegisterMetaType<Engine::TrackChangeFlags>("Engine::TrackChangeFlags");
  qRegisterMetaType<EngineBase::OutputDetails>("EngineBase::OutputDetails");
#ifdef HAVE_GSTREAMER
  qRegisterMetaType<GstBuffer*>("GstBuffer*");
  qRegisterMetaType<GstElement*>("GstElement*");
  qRegisterMetaType<GstEnginePipeline*>("GstEnginePipeline*");
#endif
  qRegisterMetaType<PlaylistItemList>("PlaylistItemList");
  qRegisterMetaType<PlaylistItemPtr>("PlaylistItemPtr");
  qRegisterMetaType<QList<PlaylistItemPtr>>("QList<PlaylistItemPtr>");
  qRegisterMetaType<PlaylistSequence::RepeatMode>("PlaylistSequence::RepeatMode");
  qRegisterMetaType<PlaylistSequence::ShuffleMode>("PlaylistSequence::ShuffleMode");
  qRegisterMetaType<AlbumCoverLoaderResult>("AlbumCoverLoaderResult");
  qRegisterMetaType<AlbumCoverLoaderResult::Type>("AlbumCoverLoaderResult::Type");
  qRegisterMetaType<CoverProviderSearchResult>("CoverProviderSearchResult");
  qRegisterMetaType<CoverSearchStatistics>("CoverSearchStatistics");
  qRegisterMetaType<QList<CoverProviderSearchResult>>("QList<CoverProviderSearchResult>");
  qRegisterMetaType<CoverProviderSearchResults>("CoverProviderSearchResults");
  qRegisterMetaType<Equalizer::Params>("Equalizer::Params");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qRegisterMetaTypeStreamOperators<Equalizer::Params>("Equalizer::Params");
#endif
#ifdef HAVE_DBUS
  qDBusRegisterMetaType<QList<QByteArray>>();
  qDBusRegisterMetaType<QImage>();
  qDBusRegisterMetaType<TrackMetadata>();
  qDBusRegisterMetaType<Track_Ids>();
  qDBusRegisterMetaType<MprisPlaylist>();
  qDBusRegisterMetaType<MprisPlaylistList>();
  qDBusRegisterMetaType<MaybePlaylist>();
  qDBusRegisterMetaType<InterfacesAndProperties>();
  qDBusRegisterMetaType<ManagedObjectList>();
#endif

  qRegisterMetaType<InternetSearchView::ResultList>("InternetSearchView::ResultList");
  qRegisterMetaType<InternetSearchView::Result>("InternetSearchView::Result");

  qRegisterMetaType<PlaylistGeneratorPtr>("PlaylistGeneratorPtr");

  qRegisterMetaType<RadioChannelList>("RadioChannelList");

  qRegisterMetaType<CollapsibleInfoPane::Data>("CollapsibleInfoPane::Data");

}
