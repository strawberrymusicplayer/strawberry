/*
 * Strawberry Music Player
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#include <QString>

#include "enginetype.h"

namespace Engine {

Engine::EngineType EngineTypeFromName(QString enginename) {
  QString lower = enginename.toLower();
  if (lower == "gstreamer")     return Engine::GStreamer;
  else if (lower == "xine")     return Engine::Xine;
  else if (lower == "vlc")      return Engine::VLC;
  else if (lower == "phonon")   return Engine::Phonon;
  else                          return Engine::None;
}

QString EngineName(Engine::EngineType enginetype) {
  switch (enginetype) {
    case Engine::GStreamer:     return QString("gstreamer");
    case Engine::Xine:          return QString("xine");
    case Engine::VLC:           return QString("vlc");
    case Engine::Phonon:        return QString("phonon");
    case Engine::None:
    default:                    return QString("None");
  }
}

QString EngineDescription(Engine::EngineType enginetype) {
  switch (enginetype) {
    case Engine::GStreamer:	return QString("GStreamer");
    case Engine::Xine:		return QString("Xine");
    case Engine::VLC:		return QString("VLC");
    case Engine::Phonon:	return QString("Phonon");
    case Engine::None:
    default:			return QString("None");

  }
}

}
