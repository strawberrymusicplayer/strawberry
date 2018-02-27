/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VLCENGINE_H
#define VLCENGINE_H

#include "config.h"

#include "enginebase.h"

#include <vlc/vlc.h>
#include <boost/circular_buffer.hpp>

#include <QMutex>

class QTimer;
class TaskManager;

class VLCEngine : public Engine::Base {
  Q_OBJECT

 public:
  VLCEngine(TaskManager *task_manager);
  ~VLCEngine();

  bool Init();
  
  virtual qint64 position_nanosec() const;
  virtual qint64 length_nanosec() const;

  bool CanDecode( const QUrl &url );

  bool Load(const QUrl &url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec);
  bool Play(quint64 offset_nanosec);
  void Stop(bool stop_after = false);
  void Pause();
  void Unpause();

  Engine::State state() const { return state_; }
  uint position() const;
  uint length() const;

  void Seek(quint64 offset_nanosec);

  static void SetScopeData(float* data, int size);
  const Engine::Scope& Scope();

 protected:
  void SetVolumeSW(uint percent); 

 private:
  void HandleErrors() const;
  void AttachCallback(libvlc_event_manager_t* em, libvlc_event_type_t type, libvlc_callback_t callback);
  static void StateChangedCallback(const libvlc_event_t* e, void* data);

 private:
  // The callbacks need access to this
  static VLCEngine *sInstance;

  // VLC bits and pieces
  //libvlc_exception_t exception_;
  libvlc_instance_t *instance_;
  libvlc_media_player_t *player_;

  // Our clementine_scope VLC plugin puts data in here
  QMutex scope_mutex_;
  boost::circular_buffer<float> scope_data_;

  Engine::State state_;
};

#endif // VLCENGINE_H
