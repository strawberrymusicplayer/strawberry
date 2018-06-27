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

#ifndef XINEFADER_H
#define XINEFADER_H

#include "config.h"

#include <QtGlobal>
#include <QThread>

class XineFader : public QThread {
    
private:

  XineEngine *engine_;
  xine_t *xine_;
  xine_stream_t *stream_;
  xine_stream_t *decrease_;
  xine_stream_t *increase_;
  xine_audio_port_t *port_;
  xine_post_t *post_;
  uint fade_length_;
  bool paused_;
  bool terminated_;
  bool stop_fader_;

  void run();

public:

  XineFader(XineEngine *engine, xine_t *xine, xine_stream_t *stream, xine_audio_port_t *audioport, xine_post_t *post, uint fadeMs);
  ~XineFader();

   void pause();
   void resume();
   void finish();

};

class XineOutFader : public QThread {
    
private:

  XineEngine *engine_;
  bool terminated_;
  uint fade_length_;

  void run();

public:
  XineOutFader(XineEngine *, uint fadeLengthMs );
  ~XineOutFader();

   void finish();
};

#endif
