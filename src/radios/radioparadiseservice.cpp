/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QUrl>

#include "core/iconloader.h"
#include "radioparadiseservice.h"
#include "radiochannel.h"

RadioParadiseService::RadioParadiseService(Application *app, NetworkAccessManager *network, QObject *parent)
    : RadioService(Song::Source::RadioParadise, "Radio Paradise", IconLoader::Load("radioparadise"), app, network, parent) {}

QUrl RadioParadiseService::Homepage() { return QUrl("https://radioparadise.com/"); }
QUrl RadioParadiseService::Donate() { return QUrl("https://payments.radioparadise.com/rp2s-content.php?name=Support&file=support"); }

void RadioParadiseService::GetChannels() {

  emit NewChannels(RadioChannelList()
    << RadioChannel(source_, "Main Mix 320k AAC", QUrl("https://stream.radioparadise.com/aac-320"))
    << RadioChannel(source_, "Mellow Mix 320k AAC", QUrl("https://stream.radioparadise.com/mellow-320"))
    << RadioChannel(source_, "Rock Mix 320k AAC", QUrl("https://stream.radioparadise.com/rock-320"))
    << RadioChannel(source_, "World/Etc Mix 320k AAC", QUrl("https://stream.radioparadise.com/world-etc-320"))
    << RadioChannel(source_, "Main Mix FLAC", QUrl("https://stream.radioparadise.com/flacm"))
    << RadioChannel(source_, "Mellow Mix FLAC", QUrl("https://stream.radioparadise.com/mellow-flacm"))
    << RadioChannel(source_, "Rock Mix FLAC", QUrl("https://stream.radioparadise.com/rock-flacm"))
    << RadioChannel(source_, "World/Etc Mix FLAC", QUrl("https://stream.radioparadise.com/world-etc-flacm"))
  );

}
