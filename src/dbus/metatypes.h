#ifndef DBUS_METATYPES_H
#define DBUS_METATYPES_H

#include <QMetaType>
#include <QList>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QDBusObjectPath>

Q_DECLARE_METATYPE(QList<QByteArray>)

using InterfacesAndProperties = QMap<QString, QVariantMap>;
using ManagedObjectList = QMap<QDBusObjectPath, InterfacesAndProperties>;

Q_DECLARE_METATYPE(InterfacesAndProperties)
Q_DECLARE_METATYPE(ManagedObjectList)

#endif  // DBUS_METATYPES_H_
