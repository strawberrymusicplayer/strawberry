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

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QPalette>
#include <QLabel>

#include "core/application.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "podcastinfowidget.h"
#include "ui_podcastinfowidget.h"

PodcastInfoWidget::PodcastInfoWidget(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_PodcastInfoWidget),
      app_(nullptr),
      image_id_(0) {

  ui_->setupUi(this);

  cover_options_.desired_height_ = 180;
  ui_->image->setFixedSize(cover_options_.desired_height_, cover_options_.desired_height_);

  // Set the colour of all the labels
  const bool light = palette().color(QPalette::Base).value() > 128;
  const QColor color = palette().color(QPalette::Dark);
  QPalette label_palette(palette());
  label_palette.setColor(QPalette::WindowText, light ? color.darker(150) : color.lighter(125));

  for (QLabel* label : findChildren<QLabel*>()) {
    if (label->property("field_label").toBool()) {
      label->setPalette(label_palette);
    }
  }

}

PodcastInfoWidget::~PodcastInfoWidget() { delete ui_; }

void PodcastInfoWidget::SetApplication(Application *app) {

  app_ = app;
  connect(app_->album_cover_loader(), &AlbumCoverLoader::AlbumCoverLoaded, this, &PodcastInfoWidget::AlbumCoverLoaded);

}

namespace {
template<typename T>
void SetText(const QString& value, T* label, QLabel* buddy_label = nullptr) {

  const bool visible = !value.isEmpty();

  label->setVisible(visible);
  if (buddy_label) {
    buddy_label->setVisible(visible);
  }

  if (visible) {
    label->setText(value);
  }

}
}  // namespace

void PodcastInfoWidget::SetPodcast(const Podcast &podcast) {

  if (image_id_) {
    app_->album_cover_loader()->CancelTask(image_id_);
    image_id_ = 0;
  }

  podcast_ = podcast;

  if (podcast.ImageUrlLarge().isValid()) {
    // Start loading an image for this item.
    image_id_ = app_->album_cover_loader()->LoadImageAsync(cover_options_, podcast.ImageUrlLarge(), QUrl());
  }

  ui_->image->hide();

  SetText(podcast.title(), ui_->title);
  SetText(podcast.description(), ui_->description);
  SetText(podcast.copyright(), ui_->copyright, ui_->copyright_label);
  SetText(podcast.author(), ui_->author, ui_->author_label);
  SetText(podcast.owner_name(), ui_->owner, ui_->owner_label);
  SetText(podcast.link().toString(), ui_->website, ui_->website_label);
  SetText(podcast.extra("gpodder:subscribers").toString(), ui_->subscribers, ui_->subscribers_label);

  if (!image_id_) {
    emit LoadingFinished();
  }

}

void PodcastInfoWidget::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (id != image_id_) {
    return;
  }
  image_id_ = 0;

  if (result.success && !result.image_scaled.isNull()) {
    ui_->image->setPixmap(QPixmap::fromImage(result.image_scaled));
    ui_->image->show();
  }

  emit LoadingFinished();

}
