/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "constants/tidalsettings.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "tidalstreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;

TidalStreamURLRequest::TidalStreamURLRequest(TidalService *service, const SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      reply_(nullptr),
      media_url_(media_url),
      id_(id),
      song_id_(media_url.path().toInt()) {}

TidalStreamURLRequest::~TidalStreamURLRequest() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

bool TidalStreamURLRequest::oauth() const {

  return service_->oauth();

}
TidalSettings::StreamUrlMethod TidalStreamURLRequest::stream_url_method() const {

  return service_->stream_url_method();

}

QUrl TidalStreamURLRequest::media_url() const {

  return media_url_;

}

int TidalStreamURLRequest::song_id() const {

  return song_id_;

}

void TidalStreamURLRequest::Process() {

  if (!authenticated()) {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Not authenticated with Tidal."));
    return;
  }

  GetStreamURL();

}

void TidalStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }
  else {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Cancelled."));
  }

}

void TidalStreamURLRequest::GetStreamURL() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  ParamList params;

  switch (stream_url_method()) {
    case TidalSettings::StreamUrlMethod::StreamUrl:
      params << Param(u"soundQuality"_s, service_->quality());
      reply_ = CreateRequest(QStringLiteral("tracks/%1/streamUrl").arg(song_id_), params);
      QObject::connect(reply_, &QNetworkReply::finished, this, &TidalStreamURLRequest::StreamURLReceived);
      break;
    case TidalSettings::StreamUrlMethod::UrlPostPaywall:
      params << Param(u"audioquality"_s, service_->quality());
      params << Param(u"playbackmode"_s, u"STREAM"_s);
      params << Param(u"assetpresentation"_s, u"FULL"_s);
      params << Param(u"urlusagemode"_s, u"STREAM"_s);
      reply_ = CreateRequest(QStringLiteral("tracks/%1/urlpostpaywall").arg(song_id_), params);
      QObject::connect(reply_, &QNetworkReply::finished, this, &TidalStreamURLRequest::StreamURLReceived);
      break;
    case TidalSettings::StreamUrlMethod::PlaybackInfoPostPaywall:
      params << Param(u"audioquality"_s, service_->quality());
      params << Param(u"playbackmode"_s, u"STREAM"_s);
      params << Param(u"assetpresentation"_s, u"FULL"_s);
      reply_ = CreateRequest(QStringLiteral("tracks/%1/playbackinfopostpaywall").arg(song_id_), params);
      QObject::connect(reply_, &QNetworkReply::finished, this, &TidalStreamURLRequest::StreamURLReceived);
      break;
  }

}

QList<QUrl> TidalStreamURLRequest::ParseUrls(const QJsonObject &json_object) {

  const QJsonValue json_urls = json_object["urls"_L1];
  if (!json_urls.isArray()) {
    return QList<QUrl>();
  }
  const QJsonArray json_array_urls = json_urls.toArray();
  QList<QUrl> urls;
  urls.reserve(json_array_urls.count());
  for (const QJsonValue &value : json_array_urls) {
    urls << QUrl(value.toString());
  }

  return urls;

}

void TidalStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;

  Q_ASSERT(replies_.contains(reply_));
  replies_.removeAll(reply_);

  const JsonObjectResult json_object_result = ParseJsonObject(reply_);

  QObject::disconnect(reply_, nullptr, this, nullptr);
  reply_->deleteLater();
  reply_ = nullptr;

  if (!json_object_result.success()) {
    Q_EMIT StreamURLFailure(id_, media_url_, json_object_result.error_message);
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;

  if (!json_object.contains("trackId"_L1)) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid Json reply, stream missing trackId."_s);
    return;
  }
  const int track_id = json_object["trackId"_L1].toInt();
  if (track_id != song_id_) {
    qLog(Debug) << "Tidal returned track ID" << track_id << "for" << media_url_;
  }

  Song::FileType filetype = Song::FileType::Stream;

  if (json_object.contains("codec"_L1) || json_object.contains("codecs"_L1)) {
    QString codec;
    if (json_object.contains("codec"_L1)) codec = json_object["codec"_L1].toString().toLower();
    if (json_object.contains("codecs"_L1)) codec = json_object["codecs"_L1].toString().toLower();
    filetype = Song::FiletypeByExtension(codec);
    if (filetype == Song::FileType::Unknown) {
      qLog(Debug) << "Tidal: Unknown codec" << codec;
      filetype = Song::FileType::Stream;
    }
  }

  int samplerate = -1;
  if (json_object.contains("sampleRate"_L1)) {
    samplerate = json_object["sampleRate"_L1].toInt();
  }

  int bit_depth = -1;
  if (json_object.contains("bitDepth"_L1)) {
    bit_depth = json_object["bitDepth"_L1].toInt();
  }

  QList<QUrl> urls;

  if (json_object.contains("manifest"_L1)) {

    const QString manifest = json_object["manifest"_L1].toString();
    const QByteArray data_manifest = QByteArray::fromBase64(manifest.toUtf8());

    QXmlStreamReader xml_reader(data_manifest);
    if (xml_reader.readNextStartElement()) {
      QUrl url;
      url.setScheme(u"data"_s);
      url.setPath(QStringLiteral("application/dash+xml;base64,%1").arg(manifest));
      urls << url;
    }

    else {

      const JsonObjectResult json_object_result_manifest = GetJsonObject(data_manifest);
      if (!json_object_result_manifest.success()) {
        Q_EMIT StreamURLFailure(id_, media_url_, json_object_result_manifest.error_message);
        return;
      }
      const QJsonObject &object_manifest = json_object_result_manifest.json_object;

      if (object_manifest.contains("encryptionType"_L1)) {
        const QString encryption_type = object_manifest["encryptionType"_L1].toString();
        if (!encryption_type.isEmpty() && encryption_type != "NONE"_L1) {
          Q_EMIT StreamURLFailure(id_, media_url_, tr("Received URL with %1 encrypted stream from Tidal. Strawberry does not currently support encrypted streams.").arg(encryption_type));
          return;
        }
      }

      if (object_manifest.contains("urls"_L1)) {
        urls << ParseUrls(object_manifest);
      }

      if (object_manifest.contains("mimeType_"_L1)) {
        const QString mimetype = object_manifest["mimeType"_L1].toString();
        QMimeDatabase mimedb;
        const QStringList suffixes = mimedb.mimeTypeForName(mimetype).suffixes();
        for (const QString &suffix : suffixes) {
          filetype = Song::FiletypeByExtension(suffix);
          if (filetype != Song::FileType::Unknown) break;
        }
        if (filetype == Song::FileType::Unknown) {
          qLog(Debug) << "Tidal: Unknown mimetype" << mimetype;
          filetype = Song::FileType::Stream;
        }
      }
      else if (object_manifest.contains("codecs"_L1)) {
        const QString codec = object_manifest["codecs"_L1].toString().toLower();
        filetype = Song::FiletypeByExtension(codec);
        if (filetype == Song::FileType::Unknown) {
          qLog(Debug) << "Tidal: Unknown codec" << codec;
          filetype = Song::FileType::Stream;
        }
      }

    }

  }

  if (json_object.contains("urls"_L1)) {
    urls << ParseUrls(json_object);
  }
  else if (json_object.contains("url"_L1)) {
    const QUrl new_url(json_object["url"_L1].toString());
    urls << new_url;
    if (filetype == Song::FileType::Stream) {
      // Guess filetype by filename extension in URL.
      filetype = Song::FiletypeByExtension(QFileInfo(new_url.path()).suffix());
      if (filetype == Song::FileType::Unknown) filetype = Song::FileType::Stream;
    }
  }

  if (json_object.contains("encryptionKey"_L1)) {
    const QString encryption_key = json_object["encryptionKey"_L1].toString();
    if (!encryption_key.isEmpty()) {
      Q_EMIT StreamURLFailure(id_, media_url_, tr("Received URL with encrypted stream from Tidal. Strawberry does not currently support encrypted streams."));
      return;
    }
  }

  if (json_object.contains("securityType"_L1) && json_object.contains("securityToken"_L1)) {
    const QString security_type = json_object["securityType"_L1].toString();
    const QString security_token = json_object["securityToken"_L1].toString();
    if (!security_type.isEmpty() && !security_token.isEmpty()) {
      Q_EMIT StreamURLFailure(id_, media_url_, tr("Received URL with encrypted stream from Tidal. Strawberry does not currently support encrypted streams."));
      return;
    }
  }

  if (urls.isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Missing stream urls."_s);
    return;
  }

  Q_EMIT StreamURLSuccess(id_, media_url_, urls.first(), filetype, samplerate, bit_depth);

}
