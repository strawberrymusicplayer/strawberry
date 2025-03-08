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

  const ParamList params = ParamList() << Param(u"format"_s, u"json"_s)
                                       << Param(u"client"_s, QLatin1String(kClientId))
                                       << Param(u"duration"_s, QString::number(duration_msec / kMsecPerSec))
                                       << Param(u"meta"_s, u"recordingids+sources"_s)
                                       << Param(u"fingerprint"_s, fingerprint);

  QUrlQuery url_query;
  url_query.setQueryItems(params);
  QUrl url(QString::fromLatin1(kUrl));
  url.setQuery(url_query);

  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(network_request);
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

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError) {
      qLog(Error) << QStringLiteral("Acoustid: %1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else {
      qLog(Error) << QStringLiteral("Acoustid: Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    }
    Q_EMIT Finished(request_id, QStringList());
    return;
  }

  QJsonParseError error;
  const QJsonDocument json_document = QJsonDocument::fromJson(reply->readAll(), &error);

  if (error.error != QJsonParseError::NoError) {
    Q_EMIT Finished(request_id, QStringList(), error.errorString());
    return;
  }

  const QJsonObject json_object = json_document.object();

  const QString status = json_object["status"_L1].toString();
  if (status != "ok"_L1) {
    Q_EMIT Finished(request_id, QStringList(), status);
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

  Q_EMIT Finished(request_id, id_list);

}
