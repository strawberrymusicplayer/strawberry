/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2012, 2014, John Maguire <john.maguire@gmail.com>
 * Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageBox>
#include <QPushButton>

#include "core/networkaccessmanager.h"
#include "core/iconloader.h"
#include "podcast.h"
#include "podcastdiscoverymodel.h"
#include "itunessearchpage.h"
#include "ui_itunessearchpage.h"

const char* ITunesSearchPage::kUrlBase = "http://ax.phobos.apple.com.edgesuite.net/WebObjects/MZStoreServices.woa/wa/wsSearch?country=US&media=podcast";

ITunesSearchPage::ITunesSearchPage(Application* app, QWidget* parent)
    : AddPodcastPage(app, parent),
      ui_(new Ui_ITunesSearchPage),
      network_(new NetworkAccessManager(this)) {

  ui_->setupUi(this);

  QObject::connect(ui_->search, &QPushButton::clicked, this, &ITunesSearchPage::SearchClicked);
  setWindowIcon(IconLoader::Load("itunes"));

}

ITunesSearchPage::~ITunesSearchPage() { delete ui_; }

void ITunesSearchPage::SearchClicked() {

  emit Busy(true);

  QUrl url(QUrl::fromEncoded(kUrlBase));
  QUrlQuery url_query;
  url_query.addQueryItem("term", ui_->query->text());
  url.setQuery(url_query);

  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { SearchFinished(reply); });

}

void ITunesSearchPage::SearchFinished(QNetworkReply* reply) {

  reply->deleteLater();
  emit Busy(false);

  model()->clear();

  // Was there a network error?
  if (reply->error() != QNetworkReply::NoError) {
    QMessageBox::warning(this, tr("Failed to fetch podcasts"), reply->errorString());
    return;
  }

  QJsonParseError error;
  QJsonDocument json_document = QJsonDocument::fromJson(reply->readAll(), &error);

  if (error.error != QJsonParseError::NoError) {
    QMessageBox::warning(this, tr("Failed to fetch podcasts"), tr("There was a problem parsing the response from the iTunes Store"));
    return;
  }

  QJsonObject json_data = json_document.object();

  // Was there an error message in the JSON?
  if (json_data.contains("errorMessage")) {
    QMessageBox::warning(this, tr("Failed to fetch podcasts"), json_data["errorMessage"].toString());
    return;
  }

  QJsonArray array = json_data["results"].toArray();
  for (const QJsonValueRef &result : array) {
    if (!result.isObject()) continue;
    QJsonObject json_result = result.toObject();
    if (json_result["kind"].toString() != "podcast") {
      continue;
    }
    
    if (!json_result.contains("artistName") ||
        !json_result.contains("trackName") ||
        !json_result.contains("feedUrl") ||
        !json_result.contains("trackViewUrl") ||
        !json_result.contains("artworkUrl30") ||
        !json_result.contains("artworkUrl100")) {
      continue;
    }

    Podcast podcast;
    podcast.set_author(json_result["artistName"].toString());
    podcast.set_title(json_result["trackName"].toString());
    podcast.set_url(QUrl(json_result["feedUrl"].toString()));
    podcast.set_link(QUrl(json_result["trackViewUrl"].toString()));
    podcast.set_image_url_small(QUrl(json_result["artworkUrl30"].toString()));
    podcast.set_image_url_large(QUrl(json_result["artworkUrl100"].toString()));

    model()->appendRow(model()->CreatePodcastItem(podcast));
  }

}

void ITunesSearchPage::Show() { ui_->query->setFocus(); }
