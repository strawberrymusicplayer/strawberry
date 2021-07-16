/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2014, John Maguire <john.maguire@gmail.com>
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
#include <QString>
#include <QUrl>
#include <QClipboard>
#include <QMessageBox>

#include "core/iconloader.h"
#include "podcastdiscoverymodel.h"
#include "podcasturlloader.h"
#include "addpodcastbyurl.h"
#include "ui_addpodcastbyurl.h"

AddPodcastByUrl::AddPodcastByUrl(Application *app, QWidget *parent)
    : AddPodcastPage(app, parent),
      ui_(new Ui_AddPodcastByUrl),
      loader_(new PodcastUrlLoader(this)) {

  ui_->setupUi(this);
  QObject::connect(ui_->go, &QPushButton::clicked, this, &AddPodcastByUrl::GoClicked);
  setWindowIcon(IconLoader::Load("podcast"));

}

AddPodcastByUrl::~AddPodcastByUrl() { delete ui_; }

void AddPodcastByUrl::SetUrlAndGo(const QUrl &url) {

  ui_->url->setText(url.toString());
  GoClicked();

}

void AddPodcastByUrl::SetOpml(const OpmlContainer &opml) {

  ui_->url->setText(opml.url.toString());
  model()->clear();
  model()->CreateOpmlContainerItems(opml, model()->invisibleRootItem());

}

void AddPodcastByUrl::GoClicked() {

  emit Busy(true);
  model()->clear();

  PodcastUrlLoaderReply* reply = loader_->Load(ui_->url->text());
  ui_->url->setText(reply->url().toString());

  QObject::connect(reply, &PodcastUrlLoaderReply::Finished, this, [this, reply]() { RequestFinished(reply); });

}

void AddPodcastByUrl::RequestFinished(PodcastUrlLoaderReply *reply) {

  reply->deleteLater();

  emit Busy(false);

  if (!reply->is_success()) {
    QMessageBox::warning(this, tr("Failed to load podcast"), reply->error_text(), QMessageBox::Close);
    return;
  }

  switch (reply->result_type()) {
    case PodcastUrlLoaderReply::Type_Podcast:
      for (const Podcast& podcast : reply->podcast_results()) {
        model()->appendRow(model()->CreatePodcastItem(podcast));
      }
      break;

    case PodcastUrlLoaderReply::Type_Opml:
      model()->CreateOpmlContainerItems(reply->opml_results(), model()->invisibleRootItem());
      break;
  }

}

void AddPodcastByUrl::Show() {

  ui_->url->setFocus();

  const QClipboard *clipboard = QApplication::clipboard();
  QStringList contents;
  contents << clipboard->text(QClipboard::Selection) << clipboard->text(QClipboard::Clipboard);

  for (const QString &content : contents) {
    if (content.contains("://")) {
      ui_->url->setText(content);
      return;
    }
  }

}
