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
#include "lyricssearchrequest.h"
#include "lyricsmodecomlyricsprovider.h"

const char LyricsModeComLyricsProvider::kUrl[] = "https://www.lyricsmode.com/lyrics/";
const char LyricsModeComLyricsProvider::kStartTag[] = "<div[^>]*>";
const char LyricsModeComLyricsProvider::kEndTag[] = "<\\/div>";
const char LyricsModeComLyricsProvider::kLyricsStart[] = "<div id=\"lyrics_text\" [^>]*>";

LyricsModeComLyricsProvider::LyricsModeComLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider("lyricsmode.com", true, kStartTag, kEndTag, kLyricsStart, false, network, parent) {}

QUrl LyricsModeComLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(kUrl + request.artist[0].toLower() + "/" + StringFixup(request.artist) + "/" + StringFixup(request.title) + ".html");

}

QString LyricsModeComLyricsProvider::StringFixup(QString text) {

  return text
    .remove(QRegularExpression("[^\\w0-9_\\- ]"))
    .replace(QRegularExpression(" {2,}"), " ")
    .simplified()
    .replace(' ', '_')
    .replace('-', '_')
    .toLower();

}
