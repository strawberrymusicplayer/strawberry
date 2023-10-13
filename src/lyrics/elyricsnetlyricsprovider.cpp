/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/transliterate.h"
#include "lyricssearchrequest.h"
#include "elyricsnetlyricsprovider.h"

const char ElyricsNetLyricsProvider::kUrl[] = "https://www.elyrics.net/read/";
const char ElyricsNetLyricsProvider::kStartTag[] = "<div[^>]*>";
const char ElyricsNetLyricsProvider::kEndTag[] = "<\\/div>";
const char ElyricsNetLyricsProvider::kLyricsStart[] = "<div id='inlyr'>";

ElyricsNetLyricsProvider::ElyricsNetLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider("elyrics.net", true, kStartTag, kEndTag, kLyricsStart, false, network, parent) {}

QUrl ElyricsNetLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(kUrl + request.artist[0].toLower() + "/" + StringFixup(request.artist) + "-lyrics/" + StringFixup(request.title) + "-lyrics.html");

}

QString ElyricsNetLyricsProvider::StringFixup(const QString &text) {

  return Utilities::Transliterate(text)
    .replace(QRegularExpression("[^\\w0-9_,&\\-\\(\\) ]"), "_")
    .replace(QRegularExpression(" {2,}"), " ")
    .simplified()
    .replace(' ', '-')
    .toLower();

}
