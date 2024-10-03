#ifndef DBUS_METATYPES_H
#define DBUS_METATYPES_H

#include <QMetaType>
#include <QByteArray>
#include <QByteArrayList>
#include <QMap>
#include <QString>
#include <QDBusObjectPath>

Q_DECLARE_METATYPE(QByteArrayList)

using InterfacesAndProperties = QMap<QString, QVariantMap>;
using ManagedObjectList = QMap<QDBusObjectPath, InterfacesAndProperties>;

Q_DECLARE_METATYPE(InterfacesAndProperties)
Q_DECLARE_METATYPE(ManagedObjectList)

#endif  // DBUS_METATYPES_H
