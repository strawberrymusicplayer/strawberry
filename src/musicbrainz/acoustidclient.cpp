/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QtGlobal>
#include <QPair>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QtAlgorithms>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/networktimeouts.h"
#include "constants/timeconstants.h"

#include "acoustidclient.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kClientId[] = "0qjUoxbowg";
constexpr char kUrl[] = "https://api.acoustid.org/v2/lookup";
constexpr int kDefaultTimeout = 5000;  // msec

QString FingerprintPreview(const QString &fingerprint) {
  constexpr int kPreviewLength = 24;
  const QString normalized = fingerprint.trimmed();
  if (normalized.size() <= kPreviewLength) {
    return normalized;
  }
  return normalized.left(kPreviewLength) + QStringLiteral("...");
}
}  // namespace

AcoustidClient::AcoustidClient(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      network_(network),
      timeouts_(new NetworkTimeouts(kDefaultTimeout, this)) {}

AcoustidClient::~AcoustidClient() {

  CancelAll();

}

void AcoustidClient::SetTimeout(const int msec) { timeouts_->SetTimeout(msec); }

void AcoustidClient::Start(const int id, const QString &fingerprint, int duration_msec) {

  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  const qint64 duration_secs = qMax(static_cast<qint64>(1), duration_msec / kMsecPerSec);
  const QString normalized_fingerprint = fingerprint.trimmed();

  // Send AcoustID lookup parameters as form data to avoid oversized query URLs.
  const ParamList params = ParamList() << Param(u"format"_s, u"json"_s)
                                       << Param(u"client"_s, QLatin1String(kClientId))
                                       << Param(u"duration"_s, QString::number(duration_secs))
                                       << Param(u"meta"_s, u"recordingids+sources"_s)
                                       << Param(u"fingerprint"_s, normalized_fingerprint);

  QUrlQuery url_query;
  url_query.setQueryItems(params);
  const QByteArray post_data = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkRequest network_request(QUrl(QString::fromLatin1(kUrl)));
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  qLog(Debug) << "AcoustID request id" << id << "duration(s)" << duration_secs << "fingerprint length" << normalized_fingerprint.size() << "preview" << FingerprintPreview(normalized_fingerprint);
  QNetworkReply *reply = network_->post(network_request, post_data);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { RequestFinished(reply, id); });
  requests_[id] = reply;

  timeouts_->AddReply(reply);

}

void AcoustidClient::Cancel(const int id) {

  if (requests_.contains(id)) delete requests_.take(id);

}

void AcoustidClient::CancelAll() {

  QList<QNetworkReply*> replies = requests_.values();
  qDeleteAll(replies);
  requests_.clear();

}

namespace {
// Struct used when extracting results in RequestFinished
struct IdSource {
  IdSource(const QString &id, const int source) : id_(id), nb_sources_(source) {}

  bool operator<(const IdSource &other) const {
    // We want the items with more sources to be at the beginning of the list
    return nb_sources_ > other.nb_sources_;
  }

  QString id_;
  int nb_sources_;
};

}  // namespace

void AcoustidClient::RequestFinished(QNetworkReply *reply, const int request_id) {

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();
  requests_.remove(request_id);

  const int http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QByteArray data = reply->readAll();

  QString api_error_message;
  QJsonObject json_object;
  // Keep only a short snippet for diagnostics to avoid dumping full responses in logs.
  const QString response_preview = QString::fromUtf8(data.left(512)).trimmed();
  if (!data.isEmpty()) {
    QJsonParseError parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error == QJsonParseError::NoError) {
      json_object = json_document.object();
      const QString status = json_object["status"_L1].toString();
      if (status != "ok"_L1) {
        if (json_object["error"_L1].isObject()) {
          const QJsonObject error_object = json_object["error"_L1].toObject();
          const int api_error_code = error_object["code"_L1].toInt();
          const QString api_error_text = error_object["message"_L1].toString();
          if (!api_error_text.isEmpty()) {
            api_error_message = QStringLiteral("%1 (%2)").arg(api_error_text).arg(api_error_code);
          }
        }
        else if (json_object["error"_L1].isString()) {
          api_error_message = json_object["error"_L1].toString();
        }
        if (api_error_message.isEmpty()) {
          api_error_message = status;
        }
      }
    }
    else if (reply->error() == QNetworkReply::NoError && http_status_code == 200) {
      Q_EMIT Finished(request_id, QStringList(), parse_error.errorString());
      return;
    }
  }

  if (reply->error() != QNetworkReply::NoError || http_status_code != 200) {
    QString error_message = api_error_message;
    if (error_message.isEmpty()) {
      if (reply->error() != QNetworkReply::NoError) {
        error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      else {
        error_message = QStringLiteral("Received HTTP code %1").arg(http_status_code);
      }
    }
    qLog(Error) << QStringLiteral("Acoustid: %1").arg(error_message);
    if (!response_preview.isEmpty()) {
      qLog(Debug) << "AcoustID non-200 response body preview:" << response_preview;
    }
    Q_EMIT Finished(request_id, QStringList(), error_message);
    return;
  }

  if (json_object.isEmpty()) {
    Q_EMIT Finished(request_id, QStringList(), QStringLiteral("Empty response from AcoustID."));
    return;
  }

  const QString status = json_object["status"_L1].toString();
  if (status != "ok"_L1) {
    const QString status_error = api_error_message.isEmpty() ? status : api_error_message;
    qLog(Warning) << "AcoustID request id" << request_id << "status not ok:" << status_error;
    if (!response_preview.isEmpty()) {
      qLog(Debug) << "AcoustID status!=ok body preview:" << response_preview;
    }
    Q_EMIT Finished(request_id, QStringList(), status_error);
    return;
  }

  // Get the results:
  // -in a first step, gather ids and their corresponding number of sources
  // -then sort results by number of sources (the results are originally
  //  unsorted but results with more sources are likely to be more accurate)
  // -keep only the ids, as sources where useful only to sort the results
  const QJsonArray json_results = json_object["results"_L1].toArray();

  // List of <id, nb of sources> pairs
  QList<IdSource> id_source_list;

  for (const QJsonValue &v : json_results) {
    const QJsonObject r = v.toObject();
    if (!r["recordings"_L1].isUndefined()) {
      const QJsonArray json_recordings = r["recordings"_L1].toArray();
      for (const QJsonValue &recording : json_recordings) {
        const QJsonObject o = recording.toObject();
        if (!o["id"_L1].isUndefined()) {
          id_source_list << IdSource(o["id"_L1].toString(), o["sources"_L1].toInt());
        }
      }
    }
  }

  std::stable_sort(id_source_list.begin(), id_source_list.end());

  QStringList id_list;
  id_list.reserve(id_source_list.count());
  for (const IdSource &is : std::as_const(id_source_list)) {
    id_list << is.id_;
  }

  qLog(Debug) << "AcoustID request id" << request_id << "returned" << id_list.count() << "recording id(s)";

  Q_EMIT Finished(request_id, id_list);

}
