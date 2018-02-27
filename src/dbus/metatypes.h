#ifndef DBUS_METATYPES_H
#define DBUS_METATYPES_H

#include <QMetaType>
#include <QDBusObjectPath>

Q_DECLARE_METATYPE(QList<QByteArray>)

typedef QMap<QString, QVariantMap> InterfacesAndProperties;
typedef QMap<QDBusObjectPath, InterfacesAndProperties> ManagedObjectList;

Q_DECLARE_METATYPE(InterfacesAndProperties)
Q_DECLARE_METATYPE(ManagedObjectList)

#endif  // DBUS_METATYPES_H_
