/* This file was part of Clementine.
   Copyright 2012, David Sansome <me@davidsansome.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MOODBARLOADER_H
#define MOODBARLOADER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

class QThread;
class QByteArray;
class QNetworkDiskCache;
class Application;
class MoodbarPipeline;

class MoodbarLoader : public QObject {
  Q_OBJECT

 public:
  explicit MoodbarLoader(Application* app, QObject* parent = nullptr);
  ~MoodbarLoader();

  enum Result {
    // The URL isn't a local file or the moodbar plugin was not available -
    // moodbar data can never be loaded.
    CannotLoad,

    // Moodbar data was loaded and returned.
    Loaded,

    // Moodbar data will be loaded in the background, a MoodbarPipeline* was
    // was returned that you can connect to the Finished() signal on.
    WillLoadAsync
  };

  Result Load(const QUrl& url, QByteArray* data, MoodbarPipeline** async_pipeline);

 private slots:
  void ReloadSettings();

  void RequestFinished(MoodbarPipeline* request, const QUrl& filename);
  void MaybeTakeNextRequest();

 private:
  static QStringList MoodFilenames(const QString& song_filename);

 private:
  QNetworkDiskCache* cache_;
  QThread* thread_;

  const int kMaxActiveRequests;

  QMap<QUrl, MoodbarPipeline*> requests_;
  QList<QUrl> queued_requests_;
  QSet<QUrl> active_requests_;

  bool enabled_;
  bool save_;
};

#endif  // MOODBARLOADER_H
