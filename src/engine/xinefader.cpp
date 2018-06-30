/***************************************************************************
 *   Copyright (C) 2017-2018 Jonas Kvinge <jonas@jkvinge.net>              *
 *   Copyright (C) 2005 Christophe Thommeret <hftom@free.fr>               *
 *             (C) 2005 Ian Monroe <ian@monroe.nu>                         *
 *             (C) 2005-2006 Mark Kretschmann <markey@web.de>              *
 *             (C) 2004-2005 Max Howell <max.howell@methylblue.com>        *
 *             (C) 2003-2004 J. Kofler <kaffeine@gmx.net>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "config.h"

#include <xine.h>

#include <QtGlobal>
#include <QThread>

#include "core/logging.h"

#include "xineengine.h"
#include "xinefader.h"

XineFader::XineFader(XineEngine *engine, xine_t *xine, xine_stream_t *stream, xine_audio_port_t *audioport, xine_post_t *post, uint fade_length)
    : QThread(engine),
    engine_(engine),
    xine_(xine),
    stream_(stream),
    decrease_(stream),
    increase_(nullptr),
    port_(audioport),
    post_(post),
    fade_length_(fade_length),
    paused_(false),
    terminated_(false) {

  if (engine->CreateStream()) {
    increase_ = stream_;
    xine_set_param(increase_, XINE_PARAM_AUDIO_AMP_LEVEL, 0);
  }
  else {
    terminated_ = true;
  }

}

XineFader::~XineFader() {

  wait();

  xine_close(decrease_);
  xine_dispose(decrease_);
  xine_close_audio_driver(xine_, port_);
  if (post_) xine_post_dispose(xine_, post_);

  if (!engine_->stop_fader())
    engine_->SetVolume(engine_->volume());

  engine_->set_stop_fader(false);

}

void XineFader::run() {

  // Do a volume change in 100 steps (or every 10ms)
  uint stepsCount = fade_length_ < 1000 ? fade_length_ / 10 : 100;
  uint stepSizeUs = (int)(1000.0 * (float)fade_length_ / (float)stepsCount);

  float mix = 0.0;
  float elapsedUs = 0.0;
  while (mix < 1.0) {
    if (terminated_) break;
    // Sleep a constant amount of time
    QThread::usleep(stepSizeUs);

    if (paused_)
      continue;

    elapsedUs += stepSizeUs;

    // Get volume (amarok main * equalizer preamp)
    float vol = Engine::Base::MakeVolumeLogarithmic(engine_->volume()) * engine_->preamp();

    // Compute the mix factor as the percentage of time spent since fade begun
    float mix = (elapsedUs / 1000.0) / (float)fade_length_;
    if (mix > 1.0) {
      if (increase_) xine_set_param(increase_, XINE_PARAM_AUDIO_AMP_LEVEL, (uint)vol);
      break;
    }

    // Change volume of streams (using dj-like cross-fade profile)
    if (decrease_) {
      //xine_set_param(decrease_, XINE_PARAM_AUDIO_AMP_LEVEL, (uint)(vol * (1.0 - mix)));  // linear
      float v = 4.0 * (1.0 - mix) / 3.0;
      xine_set_param(decrease_, XINE_PARAM_AUDIO_AMP_LEVEL, (uint)(v < 1.0 ? vol * v : vol));
    }
    if (increase_) {
      // xine_set_param(increase_, XINE_PARAM_AUDIO_AMP_LEVEL, (uint)(vol * mix));  //linear
      float v = 4.0 * mix / 3.0;
      xine_set_param(increase_, XINE_PARAM_AUDIO_AMP_LEVEL, (uint)(v < 1.0 ? vol * v : vol));
    }
  }

  // Stop using cpu!
  xine_stop(decrease_);

}

void XineFader::pause() {
  paused_ = true;
}

void XineFader::resume() {
  paused_ = false;
}

void XineFader::finish() {
  terminated_ = true;
}

XineOutFader::XineOutFader(XineEngine *engine, uint fadeLength)
    : QThread(engine),
    engine_(engine),
    terminated_(false),
    fade_length_(fadeLength)
{
}

XineOutFader::~XineOutFader() {
  wait();
}

void XineOutFader::run() {

  engine_->FadeOut(fade_length_, &terminated_);

  xine_stop(engine_->stream());
  xine_close(engine_->stream());
  xine_set_param(engine_->stream(), XINE_PARAM_AUDIO_CLOSE_DEVICE, 1);

}

void XineOutFader::finish() {
  terminated_ = true;
}

