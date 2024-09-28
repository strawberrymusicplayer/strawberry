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

#ifndef FREESPACEBAR_H
#define FREESPACEBAR_H

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QColor>
#include <QRgb>
#include <QRect>
#include <QSize>

class QPainter;
class QPaintEvent;

class FreeSpaceBar : public QWidget {
  Q_OBJECT

 public:
  explicit FreeSpaceBar(QWidget *parent = nullptr);

  void set_free_bytes(const quint64 bytes) { free_ = bytes; update(); }
  void set_additional_bytes(const quint64 bytes) { additional_ = bytes; update(); }
  void set_total_bytes(const quint64 bytes) { total_ = bytes; update(); }

  void set_free_text(const QString &text) { free_text_ = text; update(); }
  void set_additional_text(const QString &text) { additional_text_ = text; update(); }
  void set_used_text(const QString &text) { used_text_ = text; update(); }

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent *e) override;

 private:
  struct Label {
    explicit Label(const QString &t, const QColor &c) : text(t), color(c) {}

    QString text;
    QColor color;
  };

  static QString TextForSize(const QString &prefix, const quint64 size);

  void DrawBar(QPainter *p, const QRect r);
  void DrawText(QPainter *p, const QRect r);

 private:
  quint64 free_;
  quint64 additional_;
  quint64 total_;

  QString free_text_;
  QString additional_text_;
  QString exceeded_text_;
  QString used_text_;
};

#endif  // FREESPACEBAR_H
