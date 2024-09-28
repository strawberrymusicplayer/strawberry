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

#ifndef BUSYINDICATOR_H
#define BUSYINDICATOR_H

#include <QObject>
#include <QWidget>
#include <QString>

class QMovie;
class QLabel;
class QShowEvent;
class QHideEvent;

class BusyIndicator : public QWidget {
  Q_OBJECT

  Q_PROPERTY(QString text READ text WRITE set_text)

 public:
  explicit BusyIndicator(const QString &text, QWidget *parent = nullptr);
  explicit BusyIndicator(QWidget *parent = nullptr);
  ~BusyIndicator() override;

  QString text() const;
  void set_text(const QString &text);

 protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

 private:
  void Init(const QString &text);

 private:
  QMovie *movie_;
  QLabel *label_;
};

#endif  // BUSYINDICATOR_H
