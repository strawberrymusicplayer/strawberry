/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "metatypes.h"
#include "config.h"

#include <QMetaType>
#include <QNetworkReply>
#include <QNetworkCookie>

#include "metatypes.h"

#include "engine/enginebase.h"
#include "engine/enginetype.h"
#ifdef HAVE_GSTREAMER
#  include "engine/gstengine.h"
#  include "engine/gstenginepipeline.h"
#endif
#include "collection/directory.h"
#include "playlist/playlist.h"
#include "equalizer/equalizer.h"
#include "covermanager/albumcoverfetcher.h"

#ifdef HAVE_DBUS
#include <QDBusMetaType>
#include "core/mpris2.h"
#include "dbus/metatypes.h"
#endif

class QNetworkReply;
#ifdef HAVE_GSTREAMER
  class GstEnginePipeline;
#endif

void RegisterMetaTypes() {
  //qRegisterMetaType<CollapsibleInfoPane::Data>("CollapsibleInfoPane::Data");
  qRegisterMetaType<const char*>("const char*");
  qRegisterMetaType<CoverSearchResult>("CoverSearchResult");
  qRegisterMetaType<CoverSearchResults>("CoverSearchResults");
  qRegisterMetaType<Directory>("Directory");
  qRegisterMetaType<DirectoryList>("DirectoryList");
  qRegisterMetaType<Engine::SimpleMetaBundle>("Engine::SimpleMetaBundle");
  qRegisterMetaType<Engine::State>("Engine::State");
  qRegisterMetaType<Engine::TrackChangeFlags>("Engine::TrackChangeFlags");
  qRegisterMetaType<Equalizer::Params>("Equalizer::Params");
  qRegisterMetaType<EngineBase::OutputDetails>("EngineBase::OutputDetails");
#ifdef HAVE_GSTREAMER
  qRegisterMetaType<GstBuffer*>("GstBuffer*");
  qRegisterMetaType<GstElement*>("GstElement*");
  qRegisterMetaType<GstEnginePipeline*>("GstEnginePipeline*");
#endif
  qRegisterMetaType<PlaylistItemList>("PlaylistItemList");
  qRegisterMetaType<PlaylistItemPtr>("PlaylistItemPtr");
  qRegisterMetaType<QList<CoverSearchResult> >("QList<CoverSearchResult>");
  qRegisterMetaType<QList<int>>("QList<int>");
  qRegisterMetaType<QList<PlaylistItemPtr> >("QList<PlaylistItemPtr>");
  qRegisterMetaType<PlaylistSequence::RepeatMode>("PlaylistSequence::RepeatMode");
  qRegisterMetaType<PlaylistSequence::ShuffleMode>("PlaylistSequence::ShuffleMode");
  qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
  qRegisterMetaType<QList<QNetworkCookie> >("QList<QNetworkCookie>");
  qRegisterMetaType<QList<Song> >("QList<Song>");
  qRegisterMetaType<QNetworkCookie>("QNetworkCookie");
  qRegisterMetaType<QNetworkReply*>("QNetworkReply*");
  qRegisterMetaType<QNetworkReply**>("QNetworkReply**");
  qRegisterMetaType<SongList>("SongList");
  qRegisterMetaType<Song>("Song");
  qRegisterMetaTypeStreamOperators<Equalizer::Params>("Equalizer::Params");
  qRegisterMetaTypeStreamOperators<QMap<int, int> >("ColumnAlignmentMap");
  qRegisterMetaType<SubdirectoryList>("SubdirectoryList");
  qRegisterMetaType<Subdirectory>("Subdirectory");
  qRegisterMetaType<QList<QUrl>>("QList<QUrl>");
  qRegisterMetaType<QAbstractSocket::SocketState>();
  
  qRegisterMetaType<Engine::EngineType>("EngineType");

#ifdef HAVE_DBUS
  qDBusRegisterMetaType<QImage>();
  qDBusRegisterMetaType<TrackMetadata>();
  qDBusRegisterMetaType<TrackIds>();
  qDBusRegisterMetaType<QList<QByteArray>>();
  qDBusRegisterMetaType<MprisPlaylist>();
  qDBusRegisterMetaType<MaybePlaylist>();
  qDBusRegisterMetaType<MprisPlaylistList>();
  qDBusRegisterMetaType<InterfacesAndProperties>();
  qDBusRegisterMetaType<ManagedObjectList>();
#endif

}

