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

#include "config.h"

#include <QProxyStyle>
#include <QString>
#include <QPainter>
#include <QStyleOptionHeader>
#include <QFontMetrics>

#include "playlistproxystyle.h"
#include "playlist.h"

using namespace Qt::Literals::StringLiterals;

PlaylistProxyStyle::PlaylistProxyStyle(const QString &style) : QProxyStyle(style), common_style_(new QCommonStyle) {}

void PlaylistProxyStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {

  if (element == CE_HeaderLabel) {
    const QStyleOptionHeader *header_option = qstyleoption_cast<const QStyleOptionHeader*>(option);
    const QRect &rect = header_option->rect;
    const QString &text = header_option->text;
    const QFontMetrics &font_metrics = header_option->fontMetrics;

    // Spaces added to make transition less abrupt
    if (rect.width() < font_metrics.horizontalAdvance(text + u"  "_s)) {
      const Playlist::Column column = static_cast<Playlist::Column>(header_option->section);
      QStyleOptionHeader new_option(*header_option);
      new_option.text = Playlist::abbreviated_column_name(column);
      QProxyStyle::drawControl(element, &new_option, painter, widget);
      return;
    }
  }

  if (element == CE_ItemViewItem) {
    common_style_->drawControl(element, option, painter, widget);
  }
  else {
    QProxyStyle::drawControl(element, option, painter, widget);
  }

}

void PlaylistProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {

  if (element == QStyle::PE_PanelItemViewRow || element == QStyle::PE_PanelItemViewItem) {
    common_style_->drawPrimitive(element, option, painter, widget);
  }
  else {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
  }

}
