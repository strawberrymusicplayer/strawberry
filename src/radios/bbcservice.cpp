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
#include "bbcservice.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

BBCService::BBCService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::BBC, u"BBC Radio"_s, IconLoader::Load(u"bbc"_s), task_manager, network, parent) {}

QUrl BBCService::Homepage() { return QUrl(u"https://www.bbc.co.uk/sounds"_s); }
QUrl BBCService::Donate() { return QUrl(u"https://www.bbc.co.uk/sounds"_s); }

void BBCService::GetChannels() {

  RadioChannelList channels;

  // Using lsn.lv HLS wrapper (worldwide, 320kbps)
  auto add = [this, &channels](const QString &name, const QString &station_id) {
    RadioChannel channel;
    channel.source = source_;
    channel.name = name;
    channel.url = QUrl(QStringLiteral("https://lsn.lv/bbcradio.m3u8?station=%1&bitrate=320000").arg(station_id));
    channels << channel;
  };

  add(u"BBC Radio 1"_s, u"bbc_radio_one"_s);
  add(u"BBC Radio 1Xtra"_s, u"bbc_1xtra"_s);
  add(u"BBC Radio 2"_s, u"bbc_radio_two"_s);
  add(u"BBC Radio 3"_s, u"bbc_radio_three"_s);
  add(u"BBC Radio 4"_s, u"bbc_radio_fourfm"_s);
  add(u"BBC Radio 4 Extra"_s, u"bbc_radio_four_extra"_s);
  add(u"BBC Radio 5 Live"_s, u"bbc_radio_five_live"_s);
  add(u"BBC Radio 6 Music"_s, u"bbc_6music"_s);
  add(u"BBC Asian Network"_s, u"bbc_asian_network"_s);
  add(u"BBC World Service"_s, u"bbc_world_service"_s);
  add(u"BBC Radio Scotland"_s, u"bbc_radio_scotland_fm"_s);
  add(u"BBC Radio Wales"_s, u"bbc_radio_wales_fm"_s);

  Q_EMIT NewChannels(channels);

}
