/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QString>
#include <QMovie>
#include <QLabel>
#include <QSizePolicy>
#include <QBoxLayout>

#include "busyindicator.h"

using namespace Qt::Literals::StringLiterals;

class QHideEvent;
class QShowEvent;

BusyIndicator::BusyIndicator(const QString &text, QWidget *parent)
    : QWidget(parent),
      movie_(nullptr),
      label_(nullptr) {

  Init(text);
}

BusyIndicator::BusyIndicator(QWidget *parent)
    : QWidget(parent),
      movie_(nullptr),
      label_(nullptr) {

  Init(QString());
}

void BusyIndicator::Init(const QString &text) {

  movie_ = new QMovie(u":/pictures/spinner.gif"_s),
  label_ = new QLabel;

  QLabel *icon = new QLabel;
  icon->setMovie(movie_);
  icon->setMinimumSize(16, 16);

  label_->setWordWrap(true);
  label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(icon);
  layout->addSpacing(6);
  layout->addWidget(label_);

  set_text(text);

}

BusyIndicator::~BusyIndicator() {
  delete movie_;
}

void BusyIndicator::showEvent(QShowEvent *e) {
  Q_UNUSED(e)
  movie_->start();
}

void BusyIndicator::hideEvent(QHideEvent *e) {
  Q_UNUSED(e)
  movie_->stop();
}

void BusyIndicator::set_text(const QString &text) {
  label_->setText(text);
  label_->setVisible(!text.isEmpty());
}

QString BusyIndicator::text() const {
  return label_->text();
}
