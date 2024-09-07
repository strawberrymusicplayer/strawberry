/*
 * Strawberry Music Player
 * Copyright 2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QApplication>
#include <QThread>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "htmllyricsprovider.h"
#include "lyricssearchrequest.h"

using namespace Qt::StringLiterals;

HtmlLyricsProvider::HtmlLyricsProvider(const QString &name, const bool enabled, const QString &start_tag, const QString &end_tag, const QString &lyrics_start, const bool multiple, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : LyricsProvider(name, enabled, false, network, parent), start_tag_(start_tag), end_tag_(end_tag), lyrics_start_(lyrics_start), multiple_(multiple) {}

HtmlLyricsProvider::~HtmlLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool HtmlLyricsProvider::StartSearchAsync(const int id, const LyricsSearchRequest &request) {

  if (request.artist.isEmpty() || request.title.isEmpty()) return false;

  QMetaObject::invokeMethod(this, "StartSearch", Qt::QueuedConnection, Q_ARG(int, id), Q_ARG(LyricsSearchRequest, request));

  return true;

}

void HtmlLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  QUrl url(Url(request));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64; rv:122.0) Gecko/20100101 Firefox/122.0"));
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleLyricsReply(reply, id, request); });

  qLog(Debug) << name_ << "Sending request for" << url;

}

void HtmlLyricsProvider::HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() == QNetworkReply::ContentNotFoundError) {
      qLog(Debug) << name_ << "No lyrics for" << request.artist << request.album << request.title;
    }
    else {
      qLog(Error) << name_ << reply->errorString() << reply->error();
    }
    Q_EMIT SearchFinished(id);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    qLog(Error) << name_ << "Received HTTP code" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    Q_EMIT SearchFinished(id);
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    qLog(Error) << name_ << "Empty reply received from server.";
    Q_EMIT SearchFinished(id);
    return;
  }

  const QString lyrics = ParseLyricsFromHTML(QString::fromUtf8(data), QRegularExpression(start_tag_), QRegularExpression(end_tag_), QRegularExpression(lyrics_start_), multiple_);
  if (lyrics.isEmpty() || lyrics.contains("we do not have the lyrics for"_L1, Qt::CaseInsensitive)) {
    qLog(Debug) << name_ << "No lyrics for" << request.artist << request.album << request.title;
    Q_EMIT SearchFinished(id);
    return;
  }

  qLog(Debug) << name_ << "Got lyrics for" << request.artist << request.album << request.title;

  LyricsSearchResult result(lyrics);
  Q_EMIT SearchFinished(id, LyricsSearchResults() << result);

}

QString HtmlLyricsProvider::ParseLyricsFromHTML(const QString &content, const QRegularExpression &start_tag, const QRegularExpression &end_tag, const QRegularExpression &lyrics_start, const bool multiple) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  QString lyrics;
  qint64 start_idx = 0;

  do {

    QRegularExpressionMatch rematch = lyrics_start.match(content, start_idx);
    if (!rematch.hasMatch()) break;

    const qint64 start_lyrics_idx = rematch.capturedEnd();
    qint64 end_lyrics_idx = -1;

    // Find the index of the end tag.
    qint64 idx = start_lyrics_idx;
    QRegularExpressionMatch rematch_start_tag;
    QRegularExpressionMatch rematch_end_tag;
    int tags = 1;
    do {
      rematch_start_tag = start_tag.match(content, idx);
      const qint64 start_tag_idx = rematch_start_tag.hasMatch() ? rematch_start_tag.capturedStart() : -1;
      rematch_end_tag = end_tag.match(content, idx);
      const qint64 end_tag_idx = rematch_end_tag.hasMatch() ? rematch_end_tag.capturedStart() : -1;
      if (rematch_start_tag.hasMatch() && start_tag_idx <= end_tag_idx) {
        ++tags;
        idx = start_tag_idx + rematch_start_tag.capturedLength();
      }
      else if (rematch_end_tag.hasMatch()) {
        --tags;
        idx = end_tag_idx + rematch_end_tag.capturedLength();
        if (tags == 0) {
          end_lyrics_idx = rematch_end_tag.capturedStart();
          start_idx = rematch_end_tag.capturedEnd();
        }
      }
    }
    while (tags > 0 && (rematch_start_tag.hasMatch() || rematch_end_tag.hasMatch()));

    if (end_lyrics_idx != -1 && start_lyrics_idx < end_lyrics_idx) {
      if (!lyrics.isEmpty()) {
        lyrics.append(u'\n');
      }
      static const QRegularExpression regex_html_tag_a(QStringLiteral("<a [^>]*>[^<]*</a>"));
      static const QRegularExpression regex_html_tag_script(QStringLiteral("<script>[^>]*</script>"));
      static const QRegularExpression regex_html_tag_div(QStringLiteral("<div [^>]*>×</div>"));
      static const QRegularExpression regex_html_tag_br(QStringLiteral("<br[^>]*>"));
      static const QRegularExpression regex_html_tag_p_close(QStringLiteral("</p>"));
      static const QRegularExpression regex_html_tags(QStringLiteral("<[^>]*>"));
      lyrics.append(content.mid(start_lyrics_idx, end_lyrics_idx - start_lyrics_idx)
                           .remove(u'\r')
                           .remove(u'\n')
                           .remove(regex_html_tag_a)
                           .remove(regex_html_tag_script)
                           .remove(regex_html_tag_div)
                           .replace(regex_html_tag_br, QStringLiteral("\n"))
                           .replace(regex_html_tag_p_close, QStringLiteral("\n\n"))
                           .remove(regex_html_tags)
                           .trimmed());
    }
    else {
      start_idx = -1;
    }

  }
  while (start_idx > 0 && multiple);

  if (lyrics.length() > 6000 || lyrics.contains("there are no lyrics to"_L1, Qt::CaseInsensitive)) {
    return QString();
  }

  return Utilities::DecodeHtmlEntities(lyrics);

}

void HtmlLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << name_ << error;
  if (debug.isValid()) qLog(Debug) << name_ << debug;

}
