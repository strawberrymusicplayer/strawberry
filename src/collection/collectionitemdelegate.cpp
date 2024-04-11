/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QStyleOptionViewItem>
#include <QVariant>
#include <QString>
#include <QLocale>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QRect>
#include <QSize>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPoint>
#include <QLinearGradient>
#include <QToolTip>
#include <QWhatsThis>
#include <QtEvents>

#include "collectionitemdelegate.h"
#include "collectionmodel.h"

CollectionItemDelegate::CollectionItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void CollectionItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {

  const bool is_divider = idx.data(CollectionModel::Role_IsDivider).toBool();

  if (is_divider) {
    QString text(idx.data().toString());

    painter->save();

    QRect text_rect(opt.rect);

    // Does this item have an icon?
    QPixmap pixmap;
    QVariant decoration = idx.data(Qt::DecorationRole);
    if (!decoration.isNull()) {
      if (decoration.canConvert<QPixmap>()) {
        pixmap = decoration.value<QPixmap>();
      }
      else if (decoration.canConvert<QIcon>()) {
        pixmap = decoration.value<QIcon>().pixmap(opt.decorationSize);
      }
    }

    // Draw the icon at the left of the text rectangle
    if (!pixmap.isNull()) {
      QRect icon_rect(text_rect.topLeft(), opt.decorationSize);
      const int padding = (text_rect.height() - icon_rect.height()) / 2;
      icon_rect.adjust(padding, padding, padding, padding);
      text_rect.moveLeft(icon_rect.right() + padding + 6);

      if (pixmap.size() != opt.decorationSize) {
        pixmap = pixmap.scaled(opt.decorationSize, Qt::KeepAspectRatio);
      }

      painter->drawPixmap(icon_rect, pixmap);
    }
    else {
      text_rect.setLeft(text_rect.left() + 30);
    }

    // Draw the text
    QFont bold_font(opt.font);
    bold_font.setBold(true);

    painter->setPen(opt.palette.color(QPalette::Text));
    painter->setFont(bold_font);
    painter->drawText(text_rect, text);

    // Draw the line under the item
    QColor line_color = opt.palette.color(QPalette::Text);
    QLinearGradient grad_color(opt.rect.bottomLeft(), opt.rect.bottomRight());
    const double fade_start_end = (opt.rect.width() / 3.0) / opt.rect.width();
    line_color.setAlphaF(0.0);
    grad_color.setColorAt(0, line_color);
    line_color.setAlphaF(0.5);
    grad_color.setColorAt(fade_start_end, line_color);
    grad_color.setColorAt(1.0 - fade_start_end, line_color);
    line_color.setAlphaF(0.0);
    grad_color.setColorAt(1, line_color);
    painter->setPen(QPen(grad_color, 1));
    painter->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());

    painter->restore();
  }
  else {
    QStyledItemDelegate::paint(painter, opt, idx);
  }

}

bool CollectionItemDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &idx) {

  Q_UNUSED(option);

  if (!event || !view) return false;

  QString text = displayText(idx.data(), QLocale::system());

  if (text.isEmpty()) return false;

  switch (event->type()) {
    case QEvent::ToolTip:{

      QSize real_text = sizeHint(option, idx);
      QRect displayed_text = view->visualRect(idx);
      bool is_elided = displayed_text.width() < real_text.width();

      if (is_elided) {
        QToolTip::showText(event->globalPos(), text, view);
      }
      else if (idx.data(Qt::ToolTipRole).isValid()) {
        // If the item has a tooltip text, display it
        QString tooltip_text = idx.data(Qt::ToolTipRole).toString();
        QToolTip::showText(event->globalPos(), tooltip_text, view);
      }
      else {
        // in case that another text was previously displayed
        QToolTip::hideText();
      }
      return true;
    }

    case QEvent::QueryWhatsThis:
      return true;

    case QEvent::WhatsThis:
      QWhatsThis::showText(event->globalPos(), text, view);
      return true;

    default:
      break;
  }
  return false;

}
