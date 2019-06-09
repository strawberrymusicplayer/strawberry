/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QFile>
#include <QDir>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QJsonValue>
#include <QJsonObject>
#include <QXmlStreamReader>

#include "core/logging.h"
#include "core/network.h"
#include "core/song.h"
#include "settings/tidalsettingspage.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "tidalstreamurlrequest.h"

TidalStreamURLRequest::TidalStreamURLRequest(TidalService *service, NetworkAccessManager *network, const QUrl &original_url, QObject *parent)
    : TidalBaseRequest(service, network, parent),
    service_(service),
    reply_(nullptr),
    original_url_(original_url),
    song_id_(original_url.path().toInt()),
    tries_(0),
    need_login_(false) {}

TidalStreamURLRequest::~TidalStreamURLRequest() {

  if (reply_) {
    disconnect(reply_, 0, nullptr, 0);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

}

void TidalStreamURLRequest::LoginComplete(bool success, QString error) {

  if (!need_login_) return;
  need_login_ = false;

  if (!success) {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  Process();

}

void TidalStreamURLRequest::Process() {

  if (!authenticated()) {
    if (oauth()) {
      emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, tr("Not authenticated."));
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
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, tr("Cancelled."));
  }

}

void TidalStreamURLRequest::GetStreamURL() {

  ++tries_;

  if (reply_) {
    disconnect(reply_, 0, nullptr, 0);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }

  ParamList params;

  switch (stream_url_method()) {
    case TidalSettingsPage::StreamUrlMethod_StreamUrl:
      params << Param("soundQuality", quality());
      reply_ = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id_), params);
      connect(reply_, SIGNAL(finished()), this, SLOT(StreamURLReceived()));
      break;
    case TidalSettingsPage::StreamUrlMethod_UrlPostPaywall:
      params << Param("audioquality", quality());
      params << Param("playbackmode", "STREAM");
      params << Param("assetpresentation", "FULL");
      params << Param("urlusagemode", "STREAM");
      reply_ = CreateRequest(QString("tracks/%1/urlpostpaywall").arg(song_id_), params);
      connect(reply_, SIGNAL(finished()), this, SLOT(StreamURLReceived()));
      break;
    case TidalSettingsPage::StreamUrlMethod_PlaybackInfoPostPaywall:
      params << Param("audioquality", quality());
      params << Param("playbackmode", "STREAM");
      params << Param("assetpresentation", "FULL");
      reply_ = CreateRequest(QString("tracks/%1/playbackinfopostpaywall").arg(song_id_), params);
      connect(reply_, SIGNAL(finished()), this, SLOT(StreamURLReceived()));
      break;
  }

}

void TidalStreamURLRequest::StreamURLReceived() {

  if (!reply_) return;
  disconnect(reply_, 0, nullptr, 0);
  reply_->deleteLater();

  QString error;

  QByteArray data = GetReplyData(reply_, error, true);
  if (data.isEmpty()) {
    reply_ = nullptr;
    if (!authenticated() && login_sent() && tries_ <= 1) {
      need_login_ = true;
      return;
    }
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }
  reply_ = nullptr;

  qLog(Debug) << "Tidal:" << data;

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  if (!json_obj.contains("trackId")) {
    error = Error("Invalid Json reply, stream missing trackId.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }
  int track_id(json_obj["trackId"].toInt());
  if (track_id != song_id_) {
    error = Error("Incorrect track ID returned.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  Song::FileType filetype(Song::FileType_Unknown);

  if (json_obj.contains("codec") || json_obj.contains("codecs")) {
    QString codec;
    if (json_obj.contains("codec")) codec = json_obj["codec"].toString().toLower();
    if (json_obj.contains("codecs")) codec = json_obj["codecs"].toString().toLower();
    filetype = Song::FiletypeByExtension(codec);
    if (filetype == Song::FileType_Unknown) {
      qLog(Debug) << "Tidal: Unknown codec" << codec;
      filetype = Song::FileType_Stream;
    }
  }

  QList<QUrl> urls;

  if (json_obj.contains("manifest")) {

    QString manifest(json_obj["manifest"].toString());
    QByteArray data_manifest = QByteArray::fromBase64(manifest.toUtf8());

    qLog(Debug) << "Tidal:" << data_manifest;

    QXmlStreamReader xml_reader(data_manifest);
    if (!xml_reader.hasError()) {

      QString filepath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/tidalstreams";
      QString filename = "tidal-" + QString::number(song_id_) + ".xml";
      if (!QDir().mkpath(filepath)) {
        error = Error(QString("Failed to create directory %1.").arg(filepath), json_obj);
        emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
        return;
      }
      QUrl url("file://" + filepath + "/" + filename);
      QFile file(url.toLocalFile());
      if (file.exists())
       file.remove();
      if (!file.open(QIODevice::WriteOnly)) {
        error = Error(QString("Failed to open file %1 for writing.").arg(url.toLocalFile()), json_obj);
        emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
        return;
      }
      file.write(data_manifest);
      file.close();

      urls << url;

    }

    else {

      json_obj = ExtractJsonObj(data_manifest, error);
      if (json_obj.isEmpty()) {
        emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
        return;
      }

      if (!json_obj.contains("mimeType")) {
        error = Error("Invalid Json reply, stream url reply manifest is missing mimeType.", json_obj);
        emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
        return;
      }

      QString mimetype = json_obj["mimeType"].toString();
      QMimeDatabase mimedb;
      for (QString suffix : mimedb.mimeTypeForName(mimetype.toUtf8()).suffixes()) {
        filetype = Song::FiletypeByExtension(suffix);
        if (filetype != Song::FileType_Unknown) break;
      }
      if (filetype == Song::FileType_Unknown) {
        qLog(Debug) << "Tidal: Unknown mimetype" << mimetype;
        filetype = Song::FileType_Stream;
      }
    }

  }

  if (json_obj.contains("urls")) {
    QJsonValue json_urls = json_obj["urls"];
    if (!json_urls.isArray()) {
      error = Error("Invalid Json reply, urls is not an array.", json_urls);
      emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
      return;
    }
    QJsonArray json_array_urls = json_urls.toArray();
    for (const QJsonValue &value : json_array_urls) {
      urls << QUrl(value.toString());
    }
  }
  else if (json_obj.contains("url")) {
    QUrl new_url(json_obj["url"].toString());
    urls << new_url;
  }

  if (urls.isEmpty()) {
    error = Error("Missing stream urls.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, filetype);
    return;
  }

  emit StreamURLFinished(original_url_, urls.first(), filetype);

}
