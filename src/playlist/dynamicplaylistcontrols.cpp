/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>
#include <QIODevice>
#include <QFile>
#include <QPalette>

#include "core/logging.h"

#include "dynamicplaylistcontrols.h"
#include "ui_dynamicplaylistcontrols.h"

using namespace Qt::Literals::StringLiterals;

DynamicPlaylistControls::DynamicPlaylistControls(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_DynamicPlaylistControls) {

  ui_->setupUi(this);

  QObject::connect(ui_->expand, &QPushButton::clicked, this, &DynamicPlaylistControls::Expand);
  QObject::connect(ui_->repopulate, &QPushButton::clicked, this, &DynamicPlaylistControls::Repopulate);
  QObject::connect(ui_->off, &QPushButton::clicked, this, &DynamicPlaylistControls::TurnOff);

  QFile stylesheet_file(u":/style/dynamicplaylistcontrols.css"_s);
  if (stylesheet_file.open(QIODevice::ReadOnly)) {
    QString stylesheet = QString::fromLatin1(stylesheet_file.readAll());
    stylesheet_file.close();
    QColor color = palette().color(QPalette::AlternateBase).lighter(80);
    color.setAlpha(50);
    stylesheet.replace("%background"_L1, QStringLiteral("rgba(%1, %2, %3, %4%5)").arg(QString::number(color.red()), QString::number(color.green()), QString::number(color.blue()), QString::number(color.alpha()), u"%"_s));
    setStyleSheet(stylesheet);
  }

}

DynamicPlaylistControls::~DynamicPlaylistControls() { delete ui_; }
