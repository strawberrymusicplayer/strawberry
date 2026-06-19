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
#include <QList>
#include <QStringList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "constants/opentidalsettings.h"
#include "opentidalservice.h"
#include "opentidalbaserequest.h"
#include "opentidalstreamurlrequest.h"

using namespace Qt::Literals::StringLiterals;

OpenTidalStreamURLRequest::OpenTidalStreamURLRequest(OpenTidalService *service, const SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent)
    : OpenTidalBaseRequest(service, network, parent),
      service_(service),
      reply_(nullptr),
      media_url_(media_url),
      id_(id),
      song_id_(media_url.path()) {}

OpenTidalStreamURLRequest::~OpenTidalStreamURLRequest() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

OpenTidalSettings::ManifestType OpenTidalStreamURLRequest::manifest_type() const {

  return service_->manifest_type();

}

QUrl OpenTidalStreamURLRequest::media_url() const {

  return media_url_;

}

QString OpenTidalStreamURLRequest::song_id() const {

  return song_id_;

}

void OpenTidalStreamURLRequest::Process() {

  if (!authenticated()) {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Not authenticated with Open Tidal."));
    return;
  }

  GetStreamURL();

}

void OpenTidalStreamURLRequest::Cancel() {

  if (reply_ && reply_->isRunning()) {
    reply_->abort();
  }
  else {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Cancelled."));
  }

}

void OpenTidalStreamURLRequest::GetStreamURL() {

  if (reply_) {
    QObject::disconnect(reply_, nullptr, this, nullptr);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  const QString manifest_type_str = manifest_type() == OpenTidalSettings::ManifestType::HLS ? u"HLS"_s : u"MPEG_DASH"_s;
  const QString uri_scheme_str = service_->uri_scheme() == OpenTidalSettings::UriScheme::HTTPS ? u"HTTPS"_s : u"DATA"_s;

  QStringList formats;
  formats << "FLAC_HIRES"_L1 << "FLAC"_L1;

  const ParamList params = ParamList() << Param(u"manifestType"_s, manifest_type_str)
                                       << Param(u"formats"_s, formats.join(u','))
                                       << Param(u"uriScheme"_s, uri_scheme_str)
                                       << Param(u"usage"_s, u"PLAYBACK"_s)
                                       << Param(u"adaptive"_s, u"false"_s);

  reply_ = CreateRequest(QStringLiteral("trackManifests/%1").arg(song_id_), params);
  QObject::connect(reply_, &QNetworkReply::finished, this, &OpenTidalStreamURLRequest::StreamURLReceived);

}

void OpenTidalStreamURLRequest::StreamURLReceived() {

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

  qLog(Debug) << json_object;

  if (!json_object.contains("data"_L1) || !json_object["data"_L1].isObject()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid Json reply, stream missing data."_s);
    return;
  }
  const QJsonObject object_data = json_object["data"_L1].toObject();

  if (!object_data.contains("attributes"_L1) || !object_data["attributes"_L1].isObject()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid Json reply, stream missing attributes."_s);
    return;
  }
  const QJsonObject object_attributes = object_data["attributes"_L1].toObject();

  // The Open API serves a preview clip instead of the full track when the user is not entitled to play it.
  if (object_attributes.contains("trackPresentation"_L1) && object_attributes["trackPresentation"_L1].toString() == "PREVIEW"_L1) {
    const QString reason = object_attributes["previewReason"_L1].toString();
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Open Tidal only returned a preview of this track (%1).").arg(reason.isEmpty() ? tr("subscription required") : reason));
    return;
  }

  if (object_attributes.contains("drmData"_L1) && object_attributes["drmData"_L1].isObject() && !object_attributes["drmData"_L1].toObject().isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, tr("Received URL with encrypted stream from Open Tidal. Strawberry does not currently support encrypted streams."));
    return;
  }

  const QString uri = object_attributes["uri"_L1].toString();
  if (uri.isEmpty()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Missing stream uri."_s);
    return;
  }

  const QUrl url(uri);
  if (!url.isValid()) {
    Q_EMIT StreamURLFailure(id_, media_url_, u"Invalid stream uri."_s);
    return;
  }

  Song::FileType filetype = Song::FileType::Stream;
  const QJsonArray array_formats = object_attributes["formats"_L1].toArray();
  for (const QJsonValue &value_format : array_formats) {
    const QString format = value_format.toString();
    if (format.startsWith("FLAC"_L1)) {
      filetype = Song::FileType::FLAC;
      break;
    }
  }

  Q_EMIT StreamURLSuccess(id_, media_url_, url, filetype);

}
