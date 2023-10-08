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
#include "songlyricscomlyricsprovider.h"

const char SongLyricsComLyricsProvider::kUrl[] = "https://www.songlyrics.com/";
const char SongLyricsComLyricsProvider::kStartTag[] = "<p[^>]*>";
const char SongLyricsComLyricsProvider::kEndTag[] = "<\\/p>";
const char SongLyricsComLyricsProvider::kLyricsStart[] = "<p id=\"songLyricsDiv\"[^>]+>";

SongLyricsComLyricsProvider::SongLyricsComLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider("songlyrics.com", true, kStartTag, kEndTag, kLyricsStart, false, network, parent) {}

QUrl SongLyricsComLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(kUrl + StringFixup(request.artist) + "/" + StringFixup(request.title) + "-lyrics/");

}

QString SongLyricsComLyricsProvider::StringFixup(QString text) {

  return text.replace('/', '-')
             .replace('\'', '-')
             .remove(QRegularExpression("[^\\w0-9\\- ]"))
             .replace(QRegularExpression(" {2,}"), " ")
             .simplified()
             .replace(' ', '-')
             .replace(QRegularExpression("(-)\\1+"), "-")
             .toLower();

}
