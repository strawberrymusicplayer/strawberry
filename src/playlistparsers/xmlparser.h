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

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QXmlStreamWriter>

#include "includes/shared_ptr.h"
#include "parserbase.h"

class TagReaderClient;
class CollectionBackendInterface;

class XMLParser : public ParserBase {
  Q_OBJECT

 protected:
  explicit XMLParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent);

  class StreamElement {
   public:
    StreamElement(const QString &name, QXmlStreamWriter *stream) : stream_(stream) {
      stream->writeStartElement(name);
    }

    ~StreamElement() { stream_->writeEndElement(); }

   private:
    QXmlStreamWriter *stream_;
    Q_DISABLE_COPY(StreamElement)
  };
};

#endif
