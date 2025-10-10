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

#ifndef HTMLLYRICSPROVIDER_H
#define HTMLLYRICSPROVIDER_H

#include "config.h"

#include <QVariant>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "lyricsprovider.h"
#include "lyricssearchrequest.h"

class QNetworkReply;

class HtmlLyricsProvider : public LyricsProvider {
  Q_OBJECT

 public:
  explicit HtmlLyricsProvider(const QString &name, const bool enabled, const QString &start_tag, const QString &end_tag, const QString &lyrics_start, const bool multiple, const SharedPtr<NetworkAccessManager> network, QObject *parent);

  virtual bool StartSearchAsync(const int id, const LyricsSearchRequest &request) override;

  static QString ParseLyricsFromHTML(const QString &content, const QRegularExpression &start_tag, const QRegularExpression &end_tag, const QRegularExpression &lyrics_start, const bool multiple, const QList<QRegularExpression> &regex_removes = {});

 protected:
  virtual QUrl Url(const LyricsSearchRequest &request) = 0;

 protected Q_SLOTS:
  virtual void StartSearch(const int id, const LyricsSearchRequest &request) override;
  virtual void HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request);

 protected:
  const QString start_tag_;
  const QString end_tag_;
  const QString lyrics_start_;
  const bool multiple_;
};

#endif  // HTMLLYRICSPROVIDER_H
