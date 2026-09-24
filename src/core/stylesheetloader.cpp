/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cmath>
#include <algorithm>
#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QIODevice>
#include <QTextStream>
#include <QFile>
#include <QString>
#include <QPalette>
#include <QColor>
#include <QEvent>

#include "includes/shared_ptr.h"
#include "logging.h"
#include "utilities/colorutils.h"
#include "stylesheetloader.h"

using namespace Qt::Literals::StringLiterals;

namespace {
// Color difference between alternating rows and normal rows, for light and dark palettes.
#ifdef Q_OS_MACOS
// A smaller difference is hard to see on macOS in light mode.
constexpr float kAlternateBaseColorDifferenceLight = 0.07F;
constexpr float kAlternateBaseColorDifferenceDark = 0.017F;
// The alternate base on macOS is the same colour as the base in light mode, and an opaque grey in dark mode.
// Use the text colour instead, which has the most contrast to the base, so the alternating rows need the least opacity.
constexpr bool kUseTextColorForAlternateBase = true;
#else
constexpr float kAlternateBaseColorDifferenceLight = 0.025F;
constexpr float kAlternateBaseColorDifferenceDark = 0.025F;
constexpr bool kUseTextColorForAlternateBase = false;
#endif
}  // namespace

using std::make_shared;

StyleSheetLoader::StyleSheetLoader(QObject *parent) : QObject(parent) {}

void StyleSheetLoader::SetStyleSheet(QWidget *widget, const QString &filename) {

  // Load the file
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Could not open stylesheet file" << filename << "for reading:" << file.errorString();
    return;
  }
  QTextStream stream(&file);
  QString stylesheet;
  Q_FOREVER {
    QString line = stream.readLine();
    stylesheet.append(line);
    if (stream.atEnd()) break;
  }
  file.close();

  SharedPtr<StyleSheetData> styledata = make_shared<StyleSheetData>();
  styledata->filename_ = filename;
  styledata->stylesheet_template_ = stylesheet;
  styledata_.insert(widget, styledata);

  widget->installEventFilter(this);
  UpdateStyleSheet(widget, styledata);

}

void StyleSheetLoader::UpdateStyleSheet(QWidget *widget, SharedPtr<StyleSheetData> styledata) {

  QString stylesheet = styledata->stylesheet_template_;

  // Replace %palette-role with actual colours
  const QPalette palette = widget->palette();

  // The alternate base is drawn on top of the base, so the visible difference is the alpha multiplied by the color difference between the colours.
  // Calculate the alpha giving the same visible difference for all palettes, but never more opaque than the palette colour.
  const QColor color_base = palette.color(QPalette::Base);
  const float target_color_difference = Utilities::IsPaletteDark(palette, QPalette::Base, QPalette::Text) ? kAlternateBaseColorDifferenceDark : kAlternateBaseColorDifferenceLight;
  QColor color_altbase = palette.color(QPalette::AlternateBase);
  float color_difference = Utilities::ColorDifference(color_altbase, color_base);
  // Some palettes use an alternate base too close to the base to be visible, use the text colour instead.
  if (kUseTextColorForAlternateBase || color_difference < target_color_difference) {
    const QColor color_text = palette.color(QPalette::Text);
    const float text_color_difference = Utilities::ColorDifference(color_text, color_base);
    if (text_color_difference > color_difference) {
      color_altbase = color_text;
      color_difference = text_color_difference;
    }
  }
  // If the colors are the same, the alternating rows can't be seen at any opacity, so don't draw them.
  const float altbase_alpha = color_difference > 0.0F ? std::min(color_altbase.alphaF(), target_color_difference / color_difference) : 0.0F;
  const int altbase_alpha_percent = static_cast<int>(std::lround(altbase_alpha * 100.0F));
  stylesheet.replace("%palette-alternate-base"_L1, QStringLiteral("rgba(%1,%2,%3,%4%)").arg(color_altbase.red()).arg(color_altbase.green()).arg(color_altbase.blue()).arg(altbase_alpha_percent));

  ReplaceColor(&stylesheet, u"Window"_s, palette, QPalette::Window);
  ReplaceColor(&stylesheet, u"Background"_s, palette, QPalette::Window);
  ReplaceColor(&stylesheet, u"WindowText"_s, palette, QPalette::WindowText);
  ReplaceColor(&stylesheet, u"Base"_s, palette, QPalette::Base);
  ReplaceColor(&stylesheet, u"AlternateBase"_s, palette, QPalette::AlternateBase);
  ReplaceColor(&stylesheet, u"ToolTipBase"_s, palette, QPalette::ToolTipBase);
  ReplaceColor(&stylesheet, u"ToolTipText"_s, palette, QPalette::ToolTipText);
  ReplaceColor(&stylesheet, u"Text"_s, palette, QPalette::Text);
  ReplaceColor(&stylesheet, u"Button"_s, palette, QPalette::Button);
  ReplaceColor(&stylesheet, u"ButtonText"_s, palette, QPalette::ButtonText);
  ReplaceColor(&stylesheet, u"BrightText"_s, palette, QPalette::BrightText);
  ReplaceColor(&stylesheet, u"Light"_s, palette, QPalette::Light);
  ReplaceColor(&stylesheet, u"Midlight"_s, palette, QPalette::Midlight);
  ReplaceColor(&stylesheet, u"Dark"_s, palette, QPalette::Dark);
  ReplaceColor(&stylesheet, u"Mid"_s, palette, QPalette::Mid);
  ReplaceColor(&stylesheet, u"Shadow"_s, palette, QPalette::Shadow);
  ReplaceColor(&stylesheet, u"Highlight"_s, palette, QPalette::Highlight);
  ReplaceColor(&stylesheet, u"HighlightedText"_s, palette, QPalette::HighlightedText);
  ReplaceColor(&stylesheet, u"Link"_s, palette, QPalette::Link);
  ReplaceColor(&stylesheet, u"LinkVisited"_s, palette, QPalette::LinkVisited);

#ifdef Q_OS_MACOS
  stylesheet.replace(QLatin1String("macos"), QLatin1String("*"));
#endif

  widget->setStyleSheet(stylesheet);

}

void StyleSheetLoader::ReplaceColor(QString *css, const QString &name, const QPalette &palette, const QPalette::ColorRole role) {

  css->replace(u"%palette-"_s + name + u"-lighter"_s, palette.color(role).lighter().name(), Qt::CaseInsensitive);
  css->replace(u"%palette-"_s + name + u"-darker"_s, palette.color(role).darker().name(), Qt::CaseInsensitive);
  css->replace(u"%palette-"_s + name, palette.color(role).name(), Qt::CaseInsensitive);

}

bool StyleSheetLoader::eventFilter(QObject *obj, QEvent *event) {

  if (event->type() == QEvent::PaletteChange) {
    QWidget *widget = qobject_cast<QWidget*>(obj);
    if (widget && styledata_.contains(widget)) {
      UpdateStyleSheet(widget, styledata_[widget]);
    }
  }

  return false;

}
