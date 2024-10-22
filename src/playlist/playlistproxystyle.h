/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTPROXYSTYLE_H
#define PLAYLISTPROXYSTYLE_H

#include "config.h"

#include <QProxyStyle>
#include <QCommonStyle>
#include <QString>

#include "includes/scoped_ptr.h"

class QPainter;

// This proxy style works around a bug/feature introduced in Qt 4.7's QGtkStyle
// that uses Gtk to paint row backgrounds, ignoring any custom brush or palette the caller set in the QStyleOption.
// That breaks our currently playing track animation, which relies on the background painted by Qt to be transparent.
// This proxy style uses QCommonStyle to paint the affected elements.

class PlaylistProxyStyle : public QProxyStyle {
  Q_OBJECT

 public:
  explicit PlaylistProxyStyle(const QString &style);

  void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override;
  void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override;

 private:
  ScopedPtr<QCommonStyle> common_style_;
};

#endif  // PLAYLISTPROXYSTYLE_H
