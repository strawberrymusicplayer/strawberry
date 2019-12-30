/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2011, David Sansome <me@davidsansome.com>
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

#ifndef TAGREADERCLIENT_H
#define TAGREADERCLIENT_H

#include "config.h"


#include <QObject>
#include <QList>
#include <QString>
#include <QImage>

#include "core/messagehandler.h"
#include "core/workerpool.h"

#include "song.h"
#include "tagreadermessages.pb.h"

class QThread;
class Song;
template <typename HandlerType> class WorkerPool;

class TagReaderClient : public QObject {
  Q_OBJECT

 public:
  explicit TagReaderClient(QObject *parent = nullptr);

  typedef AbstractMessageHandler<pb::tagreader::Message> HandlerType;
  typedef HandlerType::ReplyType ReplyType;

  static const char *kWorkerExecutableName;

  void Start();
  void ExitAsync();

  ReplyType *ReadFile(const QString &filename);
  ReplyType *SaveFile(const QString &filename, const Song &metadata);
  ReplyType *IsMediaFile(const QString &filename);
  ReplyType *LoadEmbeddedArt(const QString &filename);

  // Convenience functions that call the above functions and wait for a response.
  // These block the calling thread with a semaphore, and must NOT be called from the TagReaderClient's thread.
  void ReadFileBlocking(const QString &filename, Song *song);
  bool SaveFileBlocking(const QString &filename, const Song &metadata);
  bool IsMediaFileBlocking(const QString &filename);
  QImage LoadEmbeddedArtBlocking(const QString &filename);

  // TODO: Make this not a singleton
  static TagReaderClient *Instance() { return sInstance; }

 signals:
  void ExitFinished();

 private slots:
  void Exit();
  void WorkerFailedToStart();

 private:
  static TagReaderClient *sInstance;

  WorkerPool<HandlerType> *worker_pool_;
  QList<pb::tagreader::Message> message_queue_;
  QThread *original_thread_;
};

typedef TagReaderClient::ReplyType TagReaderReply;

#endif  // TAGREADERCLIENT_H
