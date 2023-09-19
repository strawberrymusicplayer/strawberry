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

#ifndef SONGLYRICSCOMLYRICSPROVIDER_H
#define SONGLYRICSCOMLYRICSPROVIDER_H

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "core/shared_ptr.h"
#include "lyricsprovider.h"
#include "lyricssearchrequest.h"

class QNetworkReply;
class NetworkAccessManager;

class SongLyricsComLyricsProvider : public LyricsProvider {
  Q_OBJECT

 public:
  explicit SongLyricsComLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~SongLyricsComLyricsProvider() override;

  bool StartSearch(const int id, const LyricsSearchRequest &request) override;
  void CancelSearch(const int id) override;

 private:
  void SendRequest(const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title, QUrl url = QUrl());
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  static QString StringFixup(QString string);

 private slots:
  void HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title);

 private:
  static const char *kUrl;
  QList<QNetworkReply*> replies_;
};

#endif  // SONGLYRICSCOMLYRICSPROVIDER_H
