/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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
#include <QString>
#include <QUrl>

#include "core/iconloader.h"
#include "cbcservice.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

CBCService::CBCService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::CBC, u"CBC Radio"_s, IconLoader::Load(u"cbc"_s), task_manager, network, parent) {}

QUrl CBCService::Homepage() { return QUrl(u"https://www.cbc.ca/listen"_s); }
QUrl CBCService::Donate() { return QUrl(u"https://www.cbc.ca/listen"_s); }

void CBCService::GetChannels() {

  RadioChannelList channels;

  auto add = [this, &channels](const QString &name, const QString &url) {
    RadioChannel channel;
    channel.source = source_;
    channel.name = name;
    channel.url = QUrl(url);
    channels << channel;
  };

  // CBC Music (English)
  add(u"CBC Music Atlantic"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041055/ES_R2AHF/master.m3u8"_s);
  add(u"CBC Music Central"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041056/ES_R2CWP/master.m3u8"_s);
  add(u"CBC Music Eastern"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041057/ES_R2ETR/master.m3u8"_s);
  add(u"CBC Music Mountain"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041058/ES_R2MED/master.m3u8"_s);
  add(u"CBC Music Pacific"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041059/ES_R2PVC/master.m3u8"_s);

  // CBC Radio One (English)
  add(u"CBC Radio One Toronto"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041036/ES_R1ETR/master.m3u8"_s);
  add(u"CBC Radio One Ottawa"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041036/ES_R1OTT/master.m3u8"_s);
  add(u"CBC Radio One Montréal"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041030/ES_R1EMT/master.m3u8"_s);
  add(u"CBC Radio One Vancouver"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041050/ES_R1PVC/master.m3u8"_s);
  add(u"CBC Radio One Calgary"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041039/ES_R1MCG/master.m3u8"_s);
  add(u"CBC Radio One Edmonton"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041039/ES_R1MED/master.m3u8"_s);

  // ICI Musique (French)
  add(u"ICI Musique Montréal"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041032/ES_EMETM/master.m3u8"_s);

  // ICI Première (French)
  add(u"ICI Première Montréal"_s, u"https://cbcradiolive.akamaized.net/hls/live/2041031/ES_EPMT/master.m3u8"_s);

  Q_EMIT NewChannels(channels);

}
