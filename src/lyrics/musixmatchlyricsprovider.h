/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MUSIXMATCHLYRICSPROVIDER_H
#define MUSIXMATCHLYRICSPROVIDER_H

#include "config.h"

#include <QList>
#include <QJsonObject>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QNetworkReply;
class NetworkAccessManager;

class MusixmatchLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit MusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  bool has_compiled_api_credentials() const override;
  bool supports_custom_api_credentials() const override { return true; }
  bool api_credentials_use_secret() const override { return false; }
  QString api_credentials_id_label() const override { return tr("API key"); }
  QString api_credentials_settings_group() const override;
  QString api_credentials_use_custom_key() const override;
  QString api_credentials_id_key() const override;

  void ReloadSettings() override;

 private:
  struct LyricsSearchContext {
    explicit LyricsSearchContext() : id(-1), commontrack_id(0) {}
    int id;
    LyricsSearchRequest request;
    LyricsSearchResults results;
    // The track Musixmatch matched for the request, which can be a different track than requested, the lyrics are shared by all the tracks with the same common track ID.
    qint64 commontrack_id;
    QString artist;
    QString album;
    QString title;
  };

  using LyricsSearchContextPtr = SharedPtr<LyricsSearchContext>;

  QNetworkReply *CreateApiRequest(const QString &endpoint, const ParamList &params);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void SendTrackRequest(LyricsSearchContextPtr search);
  void SendLyricsRequest(LyricsSearchContextPtr search);
  // Returns true with the body of the message of the reply, false when there's nothing more for the search.
  bool ParseReply(QNetworkReply *reply, QJsonObject &object_body);
  void EndSearch(LyricsSearchContextPtr search);

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleTrackReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search);
  void HandleLyricsReply(QNetworkReply *reply, MusixmatchLyricsProvider::LyricsSearchContextPtr search);

 private:
  QList<LyricsSearchContextPtr> requests_search_;
  QString api_key_;

  Q_DISABLE_COPY_MOVE(MusixmatchLyricsProvider)
};

#endif  // MUSIXMATCHLYRICSPROVIDER_H
