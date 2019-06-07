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
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QJsonObject>

#include "core/logging.h"
#include "core/network.h"
#include "core/song.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "tidalstreamurlrequest.h"

TidalStreamURLRequest::TidalStreamURLRequest(TidalService *service, NetworkAccessManager *network, const QUrl &original_url, QObject *parent)
    : TidalBaseRequest(service, network, parent),
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
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, "Cancelled.");
  }

}

void TidalStreamURLRequest::GetStreamURL() {

  ++tries_;

  ParamList parameters;
  parameters << Param("soundQuality", quality());

  if (reply_) {
    disconnect(reply_, 0, nullptr, 0);
    if (reply_->isRunning()) reply_->abort();
    reply_->deleteLater();
  }
  reply_ = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id_), parameters);
  connect(reply_, SIGNAL(finished()), this, SLOT(StreamURLReceived()));

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

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  if (!json_obj.contains("url") || !json_obj.contains("codec")) {
    error = Error("Invalid Json reply, stream missing url or codec.", json_obj);
    emit StreamURLFinished(original_url_, original_url_, Song::FileType_Stream, error);
    return;
  }

  QUrl new_url(json_obj["url"].toString());
  QString codec(json_obj["codec"].toString().toLower());
  Song::FileType filetype(Song::FiletypeByExtension(codec));
  if (filetype == Song::FileType_Unknown) {
    qLog(Debug) << "Tidal: Unknown codec" << codec;
    filetype = Song::FileType_Stream;
  }

  emit StreamURLFinished(original_url_, new_url, filetype, QString());

}
