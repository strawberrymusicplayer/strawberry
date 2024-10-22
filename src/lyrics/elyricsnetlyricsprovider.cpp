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

#include <QApplication>
#include <QThread>
#include <QString>
#include <QUrl>
#include <QRegularExpression>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/transliterate.h"
#include "lyricssearchrequest.h"
#include "elyricsnetlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrl[] = "https://www.elyrics.net/read/";
constexpr char kStartTag[] = "<div[^>]*>";
constexpr char kEndTag[] = "<\\/div>";
constexpr char kLyricsStart[] = "<div id='inlyr'>";
}  // namespace

ElyricsNetLyricsProvider::ElyricsNetLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider(u"elyrics.net"_s, true, QLatin1String(kStartTag), QLatin1String(kEndTag), QLatin1String(kLyricsStart), false, network, parent) {}

QUrl ElyricsNetLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + request.artist[0].toLower() + QLatin1Char('/') + StringFixup(request.artist) + "-lyrics/"_L1 + StringFixup(request.title) + "-lyrics.html"_L1);

}

QString ElyricsNetLyricsProvider::StringFixup(const QString &text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  static const QRegularExpression regex_illegal_characters(u"[^\\w0-9_,&\\-\\(\\) ]"_s);
  static const QRegularExpression regex_duplicate_whitespaces(u" {2,}"_s);

  return Utilities::Transliterate(text)
    .replace(regex_illegal_characters, u"_"_s)
    .replace(regex_duplicate_whitespaces, u" "_s)
    .simplified()
    .replace(u' ', u'-')
    .toLower();

}
