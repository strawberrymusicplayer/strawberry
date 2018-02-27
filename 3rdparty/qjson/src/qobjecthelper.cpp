/* This file is part of qjson
  *
  * Copyright (C) 2009 Till Adam <adam@kde.org>
  * Copyright (C) 2009 Flavio Castelli <flavio@castelli.name>
  * Copyright (C) 2016 Anton Kudryavtsev <a.kudryavtsev@netris.ru>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License version 2.1, as published by the Free Software Foundation.
  *
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
  */


#include "qobjecthelper.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>

using namespace QJson;

class QObjectHelper::QObjectHelperPrivate {
};

QObjectHelper::QObjectHelper()
  : d (new QObjectHelperPrivate)
{
}

QObjectHelper::~QObjectHelper()
{
  delete d;
}

QVariantMap QObjectHelper::qobject2qvariant( const QObject* object,
                              const QStringList& ignoredProperties)
{
  QVariantMap result;
  const QMetaObject *metaobject = object->metaObject();
  int count = metaobject->propertyCount();
  for (int i=0; i<count; ++i) {
    QMetaProperty metaproperty = metaobject->property(i);
    const char *name = metaproperty.name();

    if (!metaproperty.isReadable() || ignoredProperties.contains(QLatin1String(name)))
      continue;

    QVariant value = object->property(name);
    result[QLatin1String(name)] = value;
 }
  return result;
}

void QObjectHelper::qvariant2qobject(const QVariantMap& variant, QObject* object)
{
  const QMetaObject *metaobject = object->metaObject();

  for (QVariantMap::const_iterator iter = variant.constBegin(),
       end = variant.constEnd(); iter != end; ++iter) {
    int pIdx = metaobject->indexOfProperty( iter.key().toLatin1() );

    if ( pIdx < 0 ) {
      continue;
    }

    QMetaProperty metaproperty = metaobject->property( pIdx );
    QVariant::Type type = metaproperty.type();
    QVariant v( iter.value() );
    if ( v.canConvert( type ) ) {
      v.convert( type );
      metaproperty.write( object, v );
    } else if (QLatin1String("QVariant") == QLatin1String(metaproperty.typeName())) {
     metaproperty.write( object, v );
    }
  }
}
