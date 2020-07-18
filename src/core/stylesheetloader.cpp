/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QIODevice>
#include <QTextStream>
#include <QFile>
#include <QString>
#include <QColor>
#include <QPalette>
#include <QEvent>
#include <QtDebug>

#include "core/logging.h"
#include "stylesheetloader.h"

StyleSheetLoader::StyleSheetLoader(QObject *parent) : QObject(parent), timer_reset_counter_(new QTimer(this)) {

  timer_reset_counter_->setSingleShot(true);
  timer_reset_counter_->setInterval(1000);

  connect(timer_reset_counter_, SIGNAL(timeout()), this, SLOT(ResetCounters()));

}

void StyleSheetLoader::SetStyleSheet(QWidget *widget, const QString &filename) {

  // Load the file
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Unable to open" << filename;
    return;
  }
  QTextStream stream(&file);
  QString stylesheet;
  forever {
    QString line = stream.readLine();
    stylesheet.append(line);
    if (stream.atEnd()) break;
  }
  file.close();

  StyleSheetData styledata;
  styledata.filename_ = filename;
  styledata.stylesheet_template_ = stylesheet;
  styledata.stylesheet_current_ = widget->styleSheet();
  styledata_[widget] = styledata;

  widget->installEventFilter(this);
  UpdateStyleSheet(widget, styledata);

}

void StyleSheetLoader::UpdateStyleSheet(QWidget *widget, StyleSheetData styledata) {

  QString stylesheet = styledata.stylesheet_template_;

  // Replace %palette-role with actual colours
  QPalette p(widget->palette());

  QColor alt = p.color(QPalette::AlternateBase);
  alt.setAlpha(50);
  stylesheet.replace("%palette-alternate-base", QString("rgba(%1,%2,%3,%4%)")
                                                  .arg(alt.red())
                                                  .arg(alt.green())
                                                  .arg(alt.blue())
                                                  .arg(alt.alpha()));

  ReplaceColor(&stylesheet, "Window", p, QPalette::Window);
  ReplaceColor(&stylesheet, "Background", p, QPalette::Window);
  ReplaceColor(&stylesheet, "WindowText", p, QPalette::WindowText);
  ReplaceColor(&stylesheet, "Base", p, QPalette::Base);
  ReplaceColor(&stylesheet, "AlternateBase", p, QPalette::AlternateBase);
  ReplaceColor(&stylesheet, "ToolTipBase", p, QPalette::ToolTipBase);
  ReplaceColor(&stylesheet, "ToolTipText", p, QPalette::ToolTipText);
  ReplaceColor(&stylesheet, "Text", p, QPalette::Text);
  ReplaceColor(&stylesheet, "Button", p, QPalette::Button);
  ReplaceColor(&stylesheet, "ButtonText", p, QPalette::ButtonText);
  ReplaceColor(&stylesheet, "BrightText", p, QPalette::BrightText);
  ReplaceColor(&stylesheet, "Light", p, QPalette::Light);
  ReplaceColor(&stylesheet, "Midlight", p, QPalette::Midlight);
  ReplaceColor(&stylesheet, "Dark", p, QPalette::Dark);
  ReplaceColor(&stylesheet, "Mid", p, QPalette::Mid);
  ReplaceColor(&stylesheet, "Shadow", p, QPalette::Shadow);
  ReplaceColor(&stylesheet, "Highlight", p, QPalette::Highlight);
  ReplaceColor(&stylesheet, "HighlightedText", p, QPalette::HighlightedText);
  ReplaceColor(&stylesheet, "Link", p, QPalette::Link);
  ReplaceColor(&stylesheet, "LinkVisited", p, QPalette::LinkVisited);

#ifdef Q_OS_MACOS
  stylesheet.replace("macos", "*");
#endif

  if (stylesheet != styledata.stylesheet_current_) {
    widget->setStyleSheet(stylesheet);
    styledata.stylesheet_current_ = widget->styleSheet();
    styledata_[widget] = styledata;
  }

}

void StyleSheetLoader::ReplaceColor(QString *css, const QString name, const QPalette &palette, QPalette::ColorRole role) const {

  css->replace("%palette-" + name + "-lighter", palette.color(role).lighter().name(), Qt::CaseInsensitive);
  css->replace("%palette-" + name + "-darker", palette.color(role).darker().name(), Qt::CaseInsensitive);
  css->replace("%palette-" + name, palette.color(role).name(), Qt::CaseInsensitive);

}

bool StyleSheetLoader::eventFilter(QObject *obj, QEvent *event) {

  if (event->type() != QEvent::PaletteChange) return false;

  QWidget *widget = qobject_cast<QWidget*>(obj);
  if (!widget || !styledata_.contains(widget)) return false;

  StyleSheetData styledata = styledata_[widget];
  ++styledata.count_;
  styledata_[widget] = styledata;
  timer_reset_counter_->start();
  if (styledata.count_ < 5) {
    UpdateStyleSheet(widget, styledata);
  }
  return false;

}

void StyleSheetLoader::ResetCounters() {

  for (QMap<QWidget*, StyleSheetData>::iterator i = styledata_.begin() ; i != styledata_.end() ; ++i) {
    i.value().count_ = 0;
  }

}
