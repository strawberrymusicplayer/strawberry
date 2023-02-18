/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "settings/tidalsettingspage.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "tidalstreamurlrequest.h"

TidalStreamURLRequest::TidalStreamURLRequest(TidalService *service, NetworkAccessManager *network, const QUrl &original_url, const uint id, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      reply_(nullptr),
      original_url_(original_url),
      id_(id),
      song_id_(original_url.path().toInt()),
      tries_(0),
      need_login_(false) {}

TidalStreamURLRequest::~TidalStreamURLRequest() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

void TidalStreamURLRequest::LoginComplete(const bool success, const QString &error) {

  if (!need_login_) return;
  need_login_ = false;

  if (!success) {
    emit StreamURLFailure(id_, original_url_, error);
    return;
  }

  Process();

}

void TidalStreamURLRequest::Process() {

  if (!authenticated()) {
    if (oauth()) {
      emit StreamURLFailure(id_, original_url_, tr("Not authenticated with Tidal."));
      return;
    }
    else if (api_token().isEmpty() || username().isEmpty() || password().isEmpty()) {
      emit StreamURLFailure(id_, original_url_, tr("Missing Tidal API token, username or password."));
      return;
    }
    need_login_ = true;
    emit TryLogin();
    return;
  }

  GetStreamURL();

}

void TidalStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }
  else {
    emit StreamURLFailure(id_, original_url_, tr("Cancelled."));
  }

}

void TidalStreamURLRequest::GetStreamURL() {

  ++tries_;

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  ParamList params;

  switch (stream_url_method()) {
    case TidalSettingsPage::StreamUrlMethod::StreamUrl:
      params << Param("soundQuality", quality());
      reply_ = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id_), params);
      QObject::connect(reply_, &QNetworkReply::finished, this, &TidalStreamURLRequest::StreamURLReceived);
      break;
    case TidalSettingsPage::StreamUrlMethod::UrlPostPaywall:
      params << Param("audioquality", quality());
      params << Param("playbackmode", "STREAM");
      params << Param("assetpresentation", "FULL");
      params << Param("urlusagemode", "STREAM");
      reply_ = CreateRequest(QString("tracks/%1/urlpostpaywall").arg(song_id_), params);
      QObject::connect(reply_, &QNetworkReply::finished, this, &TidalStreamURLRequest::StreamURLReceived);
      break;
    case TidalSettingsPage::StreamUrlMethod::PlaybackInfoPostPaywall:
      params << Param("audioquality", quality());
      params << Param("playbackmode", "STREAM");
      params << Param("assetpresentation", "FULL");
      reply_ = CreateRequest(QString("tracks/%1/playbackinfopostpaywall").arg(song_id_), params);
      QObject::connect(reply_, &QNetworkReply::finished, this, &TidalStreamURLRequest::StreamURLReceived);
      break;
  }

}

void TidalStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;

  QByteArray data = GetReplyData(reply_, true);

  QObject::disconnect(reply_, nullptr, this, nullptr);
  reply_->deleteLater();
  reply_ = nullptr;

  if (data.isEmpty()) {
    if (!authenticated() && login_sent() && tries_ <= 1) {
      need_login_ = true;
      return;
    }
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  if (!json_obj.contains("trackId")) {
    Error("Invalid Json reply, stream missing trackId.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }
  int track_id(json_obj["trackId"].toInt());
  if (track_id != song_id_) {
    Error("Incorrect track ID returned.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  Song::FileType filetype(Song::FileType::Stream);

  if (json_obj.contains("codec") || json_obj.contains("codecs")) {
    QString codec;
    if (json_obj.contains("codec")) codec = json_obj["codec"].toString().toLower();
    if (json_obj.contains("codecs")) codec = json_obj["codecs"].toString().toLower();
    filetype = Song::FiletypeByExtension(codec);
    if (filetype == Song::FileType::Unknown) {
      qLog(Debug) << "Tidal: Unknown codec" << codec;
      filetype = Song::FileType::Stream;
    }
  }

  QList<QUrl> urls;

  if (json_obj.contains("manifest")) {

    QString manifest(json_obj["manifest"].toString());
    QByteArray data_manifest = QByteArray::fromBase64(manifest.toUtf8());

    QXmlStreamReader xml_reader(data_manifest);
    if (xml_reader.readNextStartElement()) {
      QUrl url;
      url.setScheme("data");
      url.setPath(QString("application/dash+xml;base64,%1").arg(manifest));
      urls << url;
    }

    else {

      json_obj = ExtractJsonObj(data_manifest);
      if (json_obj.isEmpty()) {
        emit StreamURLFailure(id_, original_url_, errors_.first());
        return;
      }

      if (json_obj.contains("encryptionType") && json_obj.contains("keyId")) {
        QString encryption_type = json_obj["encryptionType"].toString();
        QString key_id = json_obj["keyId"].toString();
        if (!encryption_type.isEmpty() && !key_id.isEmpty()) {
          Error(tr("Received URL with %1 encrypted stream from Tidal. Strawberry does not currently support encrypted streams.").arg(encryption_type));
          emit StreamURLFailure(id_, original_url_, errors_.first());
          return;
        }
      }

      if (!json_obj.contains("mimeType")) {
        Error("Invalid Json reply, stream url reply manifest is missing mimeType.", json_obj);
        emit StreamURLFailure(id_, original_url_, errors_.first());
        return;
      }

      QString mimetype = json_obj["mimeType"].toString();
      QMimeDatabase mimedb;
      QStringList suffixes = mimedb.mimeTypeForName(mimetype.toUtf8()).suffixes();
      for (const QString &suffix : suffixes) {
        filetype = Song::FiletypeByExtension(suffix);
        if (filetype != Song::FileType::Unknown) break;
      }
      if (filetype == Song::FileType::Unknown) {
        qLog(Debug) << "Tidal: Unknown mimetype" << mimetype;
        filetype = Song::FileType::Stream;
      }
    }

  }

  if (json_obj.contains("urls")) {
    QJsonValue json_urls = json_obj["urls"];
    if (!json_urls.isArray()) {
      Error("Invalid Json reply, urls is not an array.", json_urls);
      emit StreamURLFailure(id_, original_url_, errors_.first());
      return;
    }
    QJsonArray json_array_urls = json_urls.toArray();
    urls.reserve(json_array_urls.count());
    for (const QJsonValueRef value : json_array_urls) {
      urls << QUrl(value.toString());
    }
  }
  else if (json_obj.contains("url")) {
    QUrl new_url(json_obj["url"].toString());
    urls << new_url;
    if (filetype == Song::FileType::Stream) {
      // Guess filetype by filename extension in URL.
      filetype = Song::FiletypeByExtension(QFileInfo(new_url.path()).suffix());
      if (filetype == Song::FileType::Unknown) filetype = Song::FileType::Stream;
    }
  }

  if (json_obj.contains("encryptionKey")) {
    QString encryption_key = json_obj["encryptionKey"].toString();
    if (!encryption_key.isEmpty()) {
      Error(tr("Received URL with encrypted stream from Tidal. Strawberry does not currently support encrypted streams."));
      emit StreamURLFailure(id_, original_url_, errors_.first());
      return;
    }
  }

  if (json_obj.contains("securityType") && json_obj.contains("securityToken")) {
    QString security_type = json_obj["securityType"].toString();
    QString security_token = json_obj["securityToken"].toString();
    if (!security_type.isEmpty() && !security_token.isEmpty()) {
      Error(tr("Received URL with encrypted stream from Tidal. Strawberry does not currently support encrypted streams."));
      emit StreamURLFailure(id_, original_url_, errors_.first());
      return;
    }
  }

  if (urls.isEmpty()) {
    Error("Missing stream urls.", json_obj);
    emit StreamURLFailure(id_, original_url_, errors_.first());
    return;
  }

  emit StreamURLSuccess(id_, original_url_, urls.first(), filetype);

}

void TidalStreamURLRequest::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  if (!error.isEmpty()) {
    errors_ << error;
  }

}
