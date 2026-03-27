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
#include "radiofranceservice.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

RadioFranceService::RadioFranceService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::RadioFrance, u"Radio France"_s, IconLoader::Load(u"radiofrance"_s), task_manager, network, parent) {}

QUrl RadioFranceService::Homepage() { return QUrl(u"https://www.radiofrance.fr/"_s); }
QUrl RadioFranceService::Donate() { return QUrl(u"https://www.radiofrance.fr/"_s); }

void RadioFranceService::GetChannels() {

  RadioChannelList channels;

  auto add = [this, &channels](const QString &name, const QString &url) {
    RadioChannel channel;
    channel.source = source_;
    channel.name = name;
    channel.url = QUrl(url);
    channels << channel;
  };

  // France Inter & France Culture
  add(u"France Inter"_s, u"https://icecast.radiofrance.fr/franceinter-hifi.aac?id=radiofrance"_s);
  add(u"France Culture"_s, u"https://icecast.radiofrance.fr/franceculture-hifi.aac?id=radiofrance"_s);

  // FIP
  add(u"FIP"_s, u"https://icecast.radiofrance.fr/fip-hifi.aac?id=radiofrance"_s);
  add(u"FIP Electro"_s, u"https://icecast.radiofrance.fr/fipelectro-hifi.aac?id=radiofrance"_s);
  add(u"FIP Groove"_s, u"https://icecast.radiofrance.fr/fipgroove-hifi.aac?id=radiofrance"_s);
  add(u"FIP Hip-Hop"_s, u"https://icecast.radiofrance.fr/fiphiphop-hifi.aac?id=radiofrance"_s);
  add(u"FIP Jazz"_s, u"https://icecast.radiofrance.fr/fipjazz-hifi.aac?id=radiofrance"_s);
  add(u"FIP Metal"_s, u"https://icecast.radiofrance.fr/fipmetal-hifi.aac?id=radiofrance"_s);
  add(u"FIP Nouveautés"_s, u"https://icecast.radiofrance.fr/fipnouveautes-hifi.aac?id=radiofrance"_s);
  add(u"FIP Pop"_s, u"https://icecast.radiofrance.fr/fippop-hifi.aac?id=radiofrance"_s);
  add(u"FIP Reggae"_s, u"https://icecast.radiofrance.fr/fipreggae-hifi.aac?id=radiofrance"_s);
  add(u"FIP Rock"_s, u"https://icecast.radiofrance.fr/fiprock-hifi.aac?id=radiofrance"_s);
  add(u"FIP Sacré Français"_s, u"https://icecast.radiofrance.fr/fipsacrefrancais-hifi.aac?id=radiofrance"_s);
  add(u"FIP World"_s, u"https://icecast.radiofrance.fr/fipworld-hifi.aac?id=radiofrance"_s);

  // France Musique
  add(u"France Musique"_s, u"https://icecast.radiofrance.fr/francemusique-hifi.aac?id=radiofrance"_s);
  add(u"France Musique Baroque"_s, u"https://icecast.radiofrance.fr/francemusiquebaroque-hifi.aac?id=radiofrance"_s);
  add(u"France Musique Classique Plus"_s, u"https://icecast.radiofrance.fr/francemusiqueclassiqueplus-hifi.aac?id=radiofrance"_s);
  add(u"France Musique Concerts"_s, u"https://icecast.radiofrance.fr/francemusiqueconcertsradiofrance-hifi.aac?id=radiofrance"_s);
  add(u"France Musique Easy Classique"_s, u"https://icecast.radiofrance.fr/francemusiqueeasyclassique-hifi.aac?id=radiofrance"_s);
  add(u"France Musique La Contemporaine"_s, u"https://icecast.radiofrance.fr/francemusiquelacontemporaine-hifi.aac?id=radiofrance"_s);
  add(u"France Musique La Jazz"_s, u"https://icecast.radiofrance.fr/francemusiquelajazz-hifi.aac?id=radiofrance"_s);
  add(u"France Musique Labo"_s, u"https://icecast.radiofrance.fr/francemusiquelabo-hifi.aac?id=radiofrance"_s);
  add(u"France Musique Ocora Monde"_s, u"https://icecast.radiofrance.fr/francemusiqueocoramonde-hifi.aac?id=radiofrance"_s);

  Q_EMIT NewChannels(channels);

}
