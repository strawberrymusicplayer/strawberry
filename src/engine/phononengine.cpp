/*
 * Strawberry Music Player
 * This file was part of Clementine
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2017-2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QtGlobal>
#include <QString>
#include <QUrl>
#include <QTimer>

#include "phononengine.h"

#include "core/timeconstants.h"
#include "core/taskmanager.h"
#include "core/logging.h"

PhononEngine::PhononEngine(TaskManager *task_manager)
  : EngineBase(),
    media_object_(new Phonon::MediaObject(this)),
    audio_output_(new Phonon::AudioOutput(Phonon::MusicCategory, this)),
    state_timer_(new QTimer(this)),
    seek_offset_(-1) {

  type_ = Engine::Phonon;

  Phonon::createPath(media_object_, audio_output_);

  connect(media_object_, SIGNAL(finished()), SLOT(PhononFinished()));
  connect(media_object_, SIGNAL(stateChanged(Phonon::State,Phonon::State)), SLOT(PhononStateChanged(Phonon::State)));

  state_timer_->setSingleShot(true);
  connect(state_timer_, SIGNAL(timeout()), SLOT(StateTimeoutExpired()));

}

PhononEngine::~PhononEngine() {
  delete media_object_;
  delete audio_output_;
}

bool PhononEngine::Init() {
  return true;
}

bool PhononEngine::CanDecode(const QUrl &url) {
  // TODO
  return true;
}

bool PhononEngine::Load(const QUrl &stream_url, const QUrl &original_url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) {
  media_object_->setCurrentSource(Phonon::MediaSource(stream_url));
  return true;
}

bool PhononEngine::Play(quint64 offset_nanosec) {

  // The seek happens in PhononStateChanged - phonon doesn't seem to change currentTime() if we seek before we start playing :S
  seek_offset_ = (offset_nanosec / kNsecPerMsec);

  media_object_->play();
  return true;

}

void PhononEngine::Stop(bool stop_after) {
  media_object_->stop();
}

void PhononEngine::Pause() {
  media_object_->pause();
}

void PhononEngine::Unpause() {
  media_object_->play();
}

Engine::State PhononEngine::state() const {

  switch (media_object_->state()) {
    case Phonon::LoadingState:
    case Phonon::PlayingState:
    case Phonon::BufferingState:
      return Engine::Playing;

    case Phonon::PausedState:
      return Engine::Paused;

    case Phonon::StoppedState:
    case Phonon::ErrorState:
    default:
      return Engine::Empty;
  }

}

uint PhononEngine::position() const {
  return media_object_->currentTime();
}

uint PhononEngine::length() const {
  return media_object_->totalTime();
}

void PhononEngine::Seek(quint64 offset_nanosec) {
  int offset = (offset_nanosec / kNsecPerMsec);
  media_object_->seek(offset);
}

void PhononEngine::SetVolumeSW(uint volume) {
  audio_output_->setVolume(volume);
}

void PhononEngine::PhononFinished() {
  emit TrackEnded();
}

void PhononEngine::PhononStateChanged(Phonon::State new_state) {

  if (new_state == Phonon::ErrorState) {
    emit Error(media_object_->errorString());
  }
  if (new_state == Phonon::PlayingState && seek_offset_ != -1) {
    media_object_->seek(seek_offset_);
    seek_offset_ = -1;
  }

  // Don't emit the state change straight away
  state_timer_->start(100);

}

void PhononEngine::StateTimeoutExpired() {
  emit StateChanged(state());
}

qint64 PhononEngine::position_nanosec() const {
  if (state() == Engine::Empty) return 0;
  const qint64 result = (position() * kNsecPerMsec);
  return qint64(qMax(0ll, result));

}

qint64 PhononEngine::length_nanosec() const {
  if (state() == Engine::Empty) return 0;
  const qint64 result = end_nanosec_ - beginning_nanosec_;
  if (result > 0) {
    return result;
  }
  else {
    // Get the length from the pipeline if we don't know.
    return (length() * kNsecPerMsec);
  }
}

EngineBase::OutputDetailsList PhononEngine::GetOutputsList() const {
  OutputDetailsList ret;
  OutputDetails output;
  output.name = "none";
  output.description = "Configured by the system";
  output.iconname = "soundcard";
  ret << output;
  return ret;
}

bool PhononEngine::ValidOutput(const QString &output) {

  return (output == "auto" || output == "" || output == DefaultOutput());

}

bool PhononEngine::CustomDeviceSupport(const QString &output) {
  return false;
}

bool PhononEngine::ALSADeviceSupport(const QString &output) {
  return false;
}
