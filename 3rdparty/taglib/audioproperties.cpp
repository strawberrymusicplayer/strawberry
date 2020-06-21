/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <tbytevector.h>

#include "aiffproperties.h"
#include "apeproperties.h"
#include "asfproperties.h"
#include "flacproperties.h"
#include "mp4properties.h"
#include "mpcproperties.h"
#include "mpegproperties.h"
#include "opusproperties.h"
#include "speexproperties.h"
#include "trueaudioproperties.h"
#include "vorbisproperties.h"
#include "wavproperties.h"
#include "wavpackproperties.h"
#include "dsfproperties.h"
#include "dsdiffproperties.h"

#include "audioproperties.h"

using namespace Strawberry_TagLib::TagLib;

// This macro is a workaround for the fact that we can't add virtual functions.
// Should be true virtual functions in taglib2.

#define VIRTUAL_FUNCTION_WORKAROUND(function_name, default_value)              \
  if (dynamic_cast<const APE::AudioProperties*>(this))                              \
    return dynamic_cast<const APE::AudioProperties*>(this)->function_name();        \
  else if (dynamic_cast<const ASF::AudioProperties*>(this))                         \
    return dynamic_cast<const ASF::AudioProperties*>(this)->function_name();        \
  else if (dynamic_cast<const FLAC::AudioProperties*>(this))                        \
    return dynamic_cast<const FLAC::AudioProperties*>(this)->function_name();       \
  else if (dynamic_cast<const MP4::AudioProperties*>(this))                         \
    return dynamic_cast<const MP4::AudioProperties*>(this)->function_name();        \
  else if (dynamic_cast<const MPC::AudioProperties*>(this))                         \
    return dynamic_cast<const MPC::AudioProperties*>(this)->function_name();        \
  else if (dynamic_cast<const MPEG::AudioProperties*>(this))                        \
    return dynamic_cast<const MPEG::AudioProperties*>(this)->function_name();       \
  else if (dynamic_cast<const Ogg::Opus::AudioProperties*>(this))                   \
    return dynamic_cast<const Ogg::Opus::AudioProperties*>(this)->function_name();  \
  else if (dynamic_cast<const Ogg::Speex::AudioProperties*>(this))                  \
    return dynamic_cast<const Ogg::Speex::AudioProperties*>(this)->function_name(); \
  else if (dynamic_cast<const TrueAudio::AudioProperties*>(this))                   \
    return dynamic_cast<const TrueAudio::AudioProperties*>(this)->function_name();  \
  else if (dynamic_cast<const RIFF::AIFF::AudioProperties*>(this))                  \
    return dynamic_cast<const RIFF::AIFF::AudioProperties*>(this)->function_name(); \
  else if (dynamic_cast<const RIFF::WAV::AudioProperties*>(this))                   \
    return dynamic_cast<const RIFF::WAV::AudioProperties*>(this)->function_name();  \
  else if (dynamic_cast<const Vorbis::AudioProperties*>(this))                      \
    return dynamic_cast<const Vorbis::AudioProperties*>(this)->function_name();     \
  else if (dynamic_cast<const WavPack::AudioProperties*>(this))                     \
    return dynamic_cast<const WavPack::AudioProperties*>(this)->function_name();    \
  else if (dynamic_cast<const DSF::AudioProperties*>(this))                         \
    return dynamic_cast<const DSF::AudioProperties*>(this)->function_name();        \
  else if (dynamic_cast<const DSDIFF::AudioProperties*>(this))                      \
    return dynamic_cast<const DSDIFF::AudioProperties*>(this)->function_name();     \
  else                                                                         \
    return (default_value);

class AudioProperties::AudioPropertiesPrivate {};

////////////////////////////////////////////////////////////////////////////////
// public methods
////////////////////////////////////////////////////////////////////////////////

AudioProperties::~AudioProperties() {}

int AudioProperties::lengthInSeconds() const {
  VIRTUAL_FUNCTION_WORKAROUND(lengthInSeconds, 0)
}

int AudioProperties::lengthInMilliseconds() const {
  VIRTUAL_FUNCTION_WORKAROUND(lengthInMilliseconds, 0)
}

////////////////////////////////////////////////////////////////////////////////
// protected methods
////////////////////////////////////////////////////////////////////////////////

AudioProperties::AudioProperties(ReadStyle) : d(nullptr) {}
