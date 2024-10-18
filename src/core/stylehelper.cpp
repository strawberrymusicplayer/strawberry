/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "stylehelper.h"

#include <QPixmapCache>
#include <QPainter>
#include <QApplication>
#include <QFileInfo>
#include <QCommonStyle>
#include <QStyleOption>
#include <QWindow>
#include <qmath.h>

using namespace Qt::Literals::StringLiterals;

// Clamps float color values within (0, 255)
static int clamp(float x) {
  const int val = x > 255 ? 255 : static_cast<int>(x);
  return val < 0 ? 0 : val;
}

// Clamps float color values within (0, 255)
/*
static int range(float x, int min, int max) {
    int val = x > max ? max : x;
    return val < min ? min : val;
}
*/

namespace Utils {

QColor StyleHelper::m_baseColor;
QColor StyleHelper::m_requestedBaseColor;
QColor StyleHelper::m_IconsBaseColor = 0xffdcdcdc;
QColor StyleHelper::m_IconsDisabledColor = 0x88a0a0a0;
QColor StyleHelper::m_ProgressBarTitleColor = 0xffffffff;

QColor StyleHelper::mergedColors(const QColor &colorA, const QColor &colorB, int factor) {

  const int maxFactor = 100;
  QColor tmp = colorA;
  tmp.setRed((tmp.red() * factor) / maxFactor + (colorB.red() * (maxFactor - factor)) / maxFactor);
  tmp.setGreen((tmp.green() * factor) / maxFactor + (colorB.green() * (maxFactor - factor)) / maxFactor);
  tmp.setBlue((tmp.blue() * factor) / maxFactor + (colorB.blue() * (maxFactor - factor)) / maxFactor);
  return tmp;

}

QColor StyleHelper::alphaBlendedColors(const QColor &colorA, const QColor &colorB) {

  const int alpha = colorB.alpha();
  const int antiAlpha = 255 - alpha;

  return QColor(
               (colorA.red() * antiAlpha + colorB.red() * alpha) / 255,
               (colorA.green() * antiAlpha + colorB.green() * alpha) / 255,
               (colorA.blue() * antiAlpha + colorB.blue() * alpha) / 255
               );
}

qreal StyleHelper::sidebarFontSize() {
#ifdef Q_OS_MACOS
  return 10;
#else
  return 7.5;
#endif
}

QColor StyleHelper::notTooBrightHighlightColor() {

  QColor highlightColor = QApplication::palette().highlight().color();
  if (0.5 * highlightColor.saturationF() + 0.75 - highlightColor.valueF() < 0) {
    highlightColor.setHsvF(highlightColor.hsvHueF(), 0.1F + highlightColor.saturationF() * 2.0F, highlightColor.valueF());
  }
  return highlightColor;

}

QPalette StyleHelper::sidebarFontPalette(const QPalette &original) {

  QPalette palette = original;
  const QColor textColor = m_ProgressBarTitleColor;
  palette.setColor(QPalette::WindowText, textColor);
  palette.setColor(QPalette::Text, textColor);
  return palette;

}

QColor StyleHelper::panelTextColor(bool lightColored) {

  return lightColored ? Qt::black : Qt::white;

}

QColor StyleHelper::baseColor(bool lightColored) {

  return lightColored ? m_baseColor.lighter(230) : m_baseColor;

}

QColor StyleHelper::highlightColor(bool lightColored) {

  QColor result = baseColor(lightColored);
  if (lightColored) {
    result.setHsv(result.hue(), clamp(static_cast<float>(result.saturation())), clamp(static_cast<float>(result.value()) * 1.06F));
  }
  else {
    result.setHsv(result.hue(), clamp(static_cast<float>(result.saturation())), clamp(static_cast<float>(result.value()) * 1.16F));
  }

  return result;

}

QColor StyleHelper::shadowColor(bool lightColored) {

  QColor result = baseColor(lightColored);
  result.setHsv(result.hue(), clamp(static_cast<float>(result.saturation()) * 1.1F), clamp(static_cast<float>(result.value()) * 0.70F));
  return result;

}

QColor StyleHelper::borderColor(bool lightColored) {

  QColor result = baseColor(lightColored);
  result.setHsv(result.hue(), result.saturation(), result.value() / 2);
  return result;

}

QColor StyleHelper::toolBarBorderColor() {

  const QColor base = baseColor();
  return QColor::fromHsv(base.hue(), base.saturation(), clamp(static_cast<float>(base.value()) * 0.80F));

}

// We try to ensure that the actual color used are within reasonalbe bounds while generating the actual baseColor from the users request.
void StyleHelper::setBaseColor(const QColor &newcolor) {

  m_requestedBaseColor = newcolor;

  QColor color;
  color.setHsv(newcolor.hue(), static_cast<int>(static_cast<float>(newcolor.saturation()) * 0.7F), 64 + newcolor.value() / 3);

  if (color.isValid() && color != m_baseColor) {
    m_baseColor = color;
    const QList<QWidget*> widgets = QApplication::topLevelWidgets();
    for (QWidget *w : widgets) {
      w->update();
    }
  }

}

static void verticalGradientHelper(QPainter *p, const QRect spanRect, const QRect rect, bool lightColored) {

  QColor highlight = StyleHelper::highlightColor(lightColored);
  QColor shadow = StyleHelper::shadowColor(lightColored);
  QLinearGradient grad(spanRect.topRight(), spanRect.topLeft());
  grad.setColorAt(0, highlight.lighter(117));
  grad.setColorAt(1, shadow.darker(109));
  p->fillRect(rect, grad);

  QColor light(255, 255, 255, 80);
  p->setPen(light);
  p->drawLine(rect.topRight() - QPoint(1, 0), rect.bottomRight() - QPoint(1, 0));
  QColor dark(0, 0, 0, 90);
  p->setPen(dark);
  p->drawLine(rect.topLeft(), rect.bottomLeft());

}

void StyleHelper::verticalGradient(QPainter *painter, const QRect spanRect, const QRect clipRect, bool lightColored) {

  if (StyleHelper::usePixmapCache()) {
    QColor keyColor = baseColor(lightColored);
    QString key = QString::asprintf("mh_vertical %d %d %d %d %d", spanRect.width(), spanRect.height(), clipRect.width(), clipRect.height(), keyColor.rgb());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap(clipRect.size());
      QPainter p(&pixmap);
      QRect rect(0, 0, clipRect.width(), clipRect.height());
      verticalGradientHelper(&p, spanRect, rect, lightColored);
      p.end();
      QPixmapCache::insert(key, pixmap);
    }
    painter->drawPixmap(clipRect.topLeft(), pixmap);
  }
  else {
    verticalGradientHelper(painter, spanRect, clipRect, lightColored);
  }

}

static void horizontalGradientHelper(QPainter *p, const QRect spanRect, const QRect rect, bool lightColored) {

  if (lightColored) {
    QLinearGradient shadowGradient(rect.topLeft(), rect.bottomLeft());
    shadowGradient.setColorAt(0, 0xf0f0f0);
    shadowGradient.setColorAt(1, 0xcfcfcf);
    p->fillRect(rect, shadowGradient);
    return;
  }

  QColor base = StyleHelper::baseColor(lightColored);
  QColor highlight = StyleHelper::highlightColor(lightColored);
  QColor shadow = StyleHelper::shadowColor(lightColored);
  QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
  grad.setColorAt(0, highlight.lighter(120));
  if (rect.height() == StyleHelper::navigationWidgetHeight()) {
    grad.setColorAt(0.4, highlight);
    grad.setColorAt(0.401, base);
  }
  grad.setColorAt(1, shadow);
  p->fillRect(rect, grad);

  QLinearGradient shadowGradient(spanRect.topLeft(), spanRect.topRight());
  shadowGradient.setColorAt(0, QColor(0, 0, 0, 30));
  QColor lighterHighlight;
  lighterHighlight = highlight.lighter(130);
  lighterHighlight.setAlpha(100);
  shadowGradient.setColorAt(0.7, lighterHighlight);
  shadowGradient.setColorAt(1, QColor(0, 0, 0, 40));
  p->fillRect(rect, shadowGradient);

}

void StyleHelper::horizontalGradient(QPainter *painter, const QRect spanRect, const QRect clipRect, bool lightColored) {

  if (StyleHelper::usePixmapCache()) {
    QColor keyColor = baseColor(lightColored);
    QString key = QString::asprintf("mh_horizontal %d %d %d %d %d %d", spanRect.width(), spanRect.height(), clipRect.width(), clipRect.height(), keyColor.rgb(), spanRect.x());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap(clipRect.size());
      QPainter p(&pixmap);
      QRect rect = QRect(0, 0, clipRect.width(), clipRect.height());
      horizontalGradientHelper(&p, spanRect, rect, lightColored);
      p.end();
      QPixmapCache::insert(key, pixmap);
    }
    painter->drawPixmap(clipRect.topLeft(), pixmap);
  }
  else {
    horizontalGradientHelper(painter, spanRect, clipRect, lightColored);
  }

}

static void menuGradientHelper(QPainter *p, const QRect spanRect, const QRect rect) {

  QLinearGradient grad(spanRect.topLeft(), spanRect.bottomLeft());
  QColor menuColor = StyleHelper::mergedColors(StyleHelper::baseColor(), QColor(244, 244, 244), 25);
  grad.setColorAt(0, menuColor.lighter(112));
  grad.setColorAt(1, menuColor);
  p->fillRect(rect, grad);

}

void StyleHelper::drawArrow(QStyle::PrimitiveElement element, QPainter *painter, const QStyleOption *option) {

  if (option->rect.width() <= 1 || option->rect.height() <= 1) {
    return;
  }

  const qreal devicePixelRatio = painter->device()->devicePixelRatio();
  const bool enabled = option->state & QStyle::State_Enabled;
  QRect r = option->rect;
  int size = qMin(r.height(), r.width());
  QPixmap pixmap;
  QString pixmapName = QString::asprintf("StyleHelper::drawArrow-%d-%d-%d-%f", element, size, (enabled ? 1 : 0), devicePixelRatio);
  if (!QPixmapCache::find(pixmapName, &pixmap)) {
    QImage image(size * static_cast<int>(devicePixelRatio), size * static_cast<int>(devicePixelRatio), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);

    QStyleOption tweakedOption(*option);
    tweakedOption.state = QStyle::State_Enabled;

    auto drawCommonStyleArrow = [&tweakedOption, element, &p](const QRect rect, const QColor &color) -> void
    {
      static const QCommonStyle* const style = qobject_cast<QCommonStyle*>(QApplication::style());
      if (!style)
        return;
      tweakedOption.palette.setColor(QPalette::ButtonText, color.rgb());
      tweakedOption.rect = rect;
      p.setOpacity(color.alphaF());
      style->QCommonStyle::drawPrimitive(element, &tweakedOption, &p);
    };

    if (!enabled) {
      drawCommonStyleArrow(image.rect(), m_IconsDisabledColor);
    }
    else {
      drawCommonStyleArrow(image.rect().translated(0, static_cast<int>(devicePixelRatio)), toolBarDropShadowColor());
      drawCommonStyleArrow(image.rect(), m_IconsBaseColor);
    }
    p.end();
    pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    QPixmapCache::insert(pixmapName, pixmap);
  }
  int xOffset = r.x() + (r.width() - size) / 2;
  int yOffset = r.y() + (r.height() - size) / 2;
  painter->drawPixmap(xOffset, yOffset, pixmap);

}

void StyleHelper::menuGradient(QPainter *painter, const QRect spanRect, const QRect clipRect) {

  if (StyleHelper::usePixmapCache()) {
    QString key = QString::asprintf("mh_menu %d %d %d %d %d", spanRect.width(), spanRect.height(), clipRect.width(), clipRect.height(), StyleHelper::baseColor().rgb());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap(clipRect.size());
      QPainter p(&pixmap);
      QRect rect = QRect(0, 0, clipRect.width(), clipRect.height());
      menuGradientHelper(&p, spanRect, rect);
      p.end();
      QPixmapCache::insert(key, pixmap);
    }
    painter->drawPixmap(clipRect.topLeft(), pixmap);
  }
  else {
    menuGradientHelper(painter, spanRect, clipRect);
  }

}

QPixmap StyleHelper::disabledSideBarIcon(const QPixmap &enabledicon) {

  QImage im = enabledicon.toImage().convertToFormat(QImage::Format_ARGB32);

  for (int y = 0; y < im.height(); ++y) {
    auto scanLine = reinterpret_cast<QRgb*>(im.scanLine(y));
    for (int x = 0; x < im.width(); ++x) {
      QRgb pixel = *scanLine;
      char intensity = static_cast<char>(qGray(pixel));
      *scanLine = qRgba(intensity, intensity, intensity, qAlpha(pixel));
      ++scanLine;
    }
  }
  return QPixmap::fromImage(im);

}

// Draws a CSS-like border image where the defined borders are not stretched
// Unit for rect, left, top, right and bottom is user pixels
void StyleHelper::drawCornerImage(const QImage &img, QPainter *painter, const QRect rect, int left, int top, int right, int bottom) {

  // source rect for drawImage() calls needs to be specified in DIP unit of the image
  const qreal imagePixelRatio = img.devicePixelRatio();
  const qreal leftDIP = left * imagePixelRatio;
  const qreal topDIP = top * imagePixelRatio;
  const qreal rightDIP = right * imagePixelRatio;
  const qreal bottomDIP = bottom * imagePixelRatio;

  const QSize size = img.size();
  if (top > 0) {  //top
    painter->drawImage(QRectF(rect.left() + left, rect.top(), rect.width() - right - left, top), img, QRectF(leftDIP, 0, size.width() - rightDIP - leftDIP, topDIP));
    if (left > 0) {  //top-left
      painter->drawImage(QRectF(rect.left(), rect.top(), left, top), img, QRectF(0, 0, leftDIP, topDIP));
    }
    if (right > 0) {  //top-right
      painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top(), right, top), img, QRectF(size.width() - rightDIP, 0, rightDIP, topDIP));
    }
  }
  //left
  if (left > 0) {
    painter->drawImage(QRectF(rect.left(), rect.top() + top, left, rect.height() - top - bottom), img, QRectF(0, topDIP, leftDIP, size.height() - bottomDIP - topDIP));
  }
  //center
  painter->drawImage(QRectF(rect.left() + left, rect.top() + top, rect.width() - right - left, rect.height() - bottom - top), img, QRectF(leftDIP, topDIP, size.width() - rightDIP - leftDIP, size.height() - bottomDIP - topDIP));
  if (right > 0) {  //right
    painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top() + top, right, rect.height() - top - bottom), img, QRectF(size.width() - rightDIP, topDIP, rightDIP, size.height() - bottomDIP - topDIP));
  }
  if (bottom > 0) {  //bottom
    painter->drawImage(QRectF(rect.left() + left, rect.top() + rect.height() - bottom, rect.width() - right - left, bottom), img, QRectF(leftDIP, size.height() - bottomDIP, size.width() - rightDIP - leftDIP, bottomDIP));
    if (left > 0) {  //bottom-left
      painter->drawImage(QRectF(rect.left(), rect.top() + rect.height() - bottom, left, bottom), img, QRectF(0, size.height() - bottomDIP, leftDIP, bottomDIP));
    }
    if (right > 0) {  //bottom-right
      painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top() + rect.height() - bottom, right, bottom), img, QRectF(size.width() - rightDIP, size.height() - bottomDIP, rightDIP, bottomDIP));
    }
  }
}

// Tints an image with tintColor, while preserving alpha and lightness
void StyleHelper::tintImage(QImage &img, const QColor &tintColor) {

  QPainter p(&img);
  p.setCompositionMode(QPainter::CompositionMode_Screen);

  for (int x = 0; x < img.width(); ++x) {
    for (int y = 0; y < img.height(); ++y) {
      QRgb rgbColor = img.pixel(x, y);
      int alpha = qAlpha(rgbColor);
      QColor c = QColor(rgbColor);

      if (alpha > 0) {
        c.toHsl();
        float l = c.lightnessF();
        QColor newColor = QColor::fromHslF(tintColor.hslHueF(), tintColor.hslSaturationF(), l);
        newColor.setAlpha(alpha);
        img.setPixel(x, y, newColor.rgba());
      }
    }
  }

}

QLinearGradient StyleHelper::statusBarGradient(const QRect statusBarRect) {

  QLinearGradient grad(statusBarRect.topLeft(), QPoint(statusBarRect.center().x(), statusBarRect.bottom()));
  QColor startColor = shadowColor().darker(164);
  QColor endColor = baseColor().darker(130);
  grad.setColorAt(0, startColor);
  grad.setColorAt(1, endColor);
  return grad;

}

QString StyleHelper::dpiSpecificImageFile(const QString &fileName) {

  // See QIcon::addFile()
  if (qApp->devicePixelRatio() > 1.0) {
    const QString atDprfileName = imageFileWithResolution(fileName, qRound(qApp->devicePixelRatio()));
    if (QFile::exists(atDprfileName)) {
      return atDprfileName;
    }
  }
  return fileName;

}

QString StyleHelper::imageFileWithResolution(const QString &fileName, int dpr) {

  const QFileInfo fi(fileName);
  return dpr == 1 ? fileName : fi.path() + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('@') + QString::number(dpr) + "x."_L1 + fi.suffix();

}

QList<int> StyleHelper::availableImageResolutions(const QString &fileName) {

  QList<int> result;
  const int maxResolutions = qApp->devicePixelRatio();
  for (int i = 1; i <= maxResolutions; ++i) {
    if (QFile::exists(imageFileWithResolution(fileName, i))) {
      result.append(i);
    }
  }
  return result;
}

}  // namespace Utils
