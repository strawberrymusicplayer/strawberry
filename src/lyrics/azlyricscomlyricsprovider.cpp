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
#include "azlyricscomlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrl[] = "https://www.azlyrics.com/lyrics/";
constexpr char kStartTag[] = "<div>";
constexpr char kEndTag[] = "</div>";
constexpr char kLyricsStart[] = "<!-- Usage of azlyrics.com content by any third-party lyrics provider is prohibited by our licensing agreement. Sorry about that. -->";
}  // namespace

AzLyricsComLyricsProvider::AzLyricsComLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : HtmlLyricsProvider(u"azlyrics.com"_s, true, QLatin1String(kStartTag), QLatin1String(kEndTag), QLatin1String(kLyricsStart), false, network, parent) {}

QUrl AzLyricsComLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + StringFixup(request.artist) + QLatin1Char('/') + StringFixup(request.title) + u".html"_s);

}

QString AzLyricsComLyricsProvider::StringFixup(const QString &text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  static const QRegularExpression regex_words_numbers_and_dash(u"[^\\w0-9\\-]"_s);
  return Utilities::Transliterate(text).remove(regex_words_numbers_and_dash).toLower();

}
