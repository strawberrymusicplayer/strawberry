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

#ifndef MOODBARCONTROLLER_H
#define MOODBARCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>

class Application;
class MoodbarPipeline;
class Song;

class MoodbarController : public QObject {
  Q_OBJECT

 public:
  MoodbarController(Application* app, QObject* parent = nullptr);

 signals:
  void CurrentMoodbarDataChanged(const QByteArray& data);

 private slots:
  void CurrentSongChanged(const Song& song);
  void PlaybackStopped();
  void AsyncLoadComplete(MoodbarPipeline* pipeline, const QUrl& url);

 private:
  Application* app_;
};

#endif  // MOODBARCONTROLLER_H
