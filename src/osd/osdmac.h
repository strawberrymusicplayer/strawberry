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

#ifndef OSDMAC_H
#define OSDMAC_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QImage>

#include "osdbase.h"

class OSDMac : public OSDBase {
  Q_OBJECT

 public:
  explicit OSDMac(const SharedPtr<SystemTrayIcon> tray_icon, QObject *parent = nullptr);
  ~OSDMac() override;

  bool SupportsNativeNotifications() const override;
  bool SupportsTrayPopups() const override;

 private:
  void ShowMessageNative(const QString &summary, const QString &message, const QString &icon, const QImage &image) override;
};

#endif  // OSDMAC_H
