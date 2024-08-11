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

#ifndef POTRANSLATOR_H
#define POTRANSLATOR_H

#include <QTranslator>
#include <QString>

// We convert from .po files to .qm files, which loses context information.
// This translator tries loading strings with an empty context if it can't find any others.

class PoTranslator : public QTranslator {
  Q_OBJECT

 public:
  explicit PoTranslator(QObject *parent = nullptr);
  QString translate(const char *context, const char *source_text, const char *disambiguation = nullptr, int n = -1) const override;
};

#endif  // POTRANSLATOR_H
