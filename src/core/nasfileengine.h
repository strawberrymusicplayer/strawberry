// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NASFILEENGINE_H
#define NASFILEENGINE_H

#include <QList>
#include <QString>

namespace Nas {

struct Location {
  QString id;
  QString server;
  QString share;
  QString folder;
  QString user;
  QString password;
  QString domain;
  QString path() const;
  QString label() const;
};

void Install();
bool IsPath(const QString &path);
quint64 FailureSerial();
QString Validate(const Location &location);
QString Check(const Location &location);
QString DisplayName(const QString &path);
void SetLocations(const QList<Location> &locations);
QList<Location> Locations();

#ifdef Q_OS_ANDROID
bool Load(QString *error);
bool Save(const QList<Location> &locations, QString *error);
#endif

}  // namespace Nas
#endif
