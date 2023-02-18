/*
 * Strawberry Music Player
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>

#ifdef Q_OS_LINUX
#  include <unistd.h>
#  include <sys/syscall.h>
#endif

#ifdef Q_OS_MACOS
#  include <sys/resource.h>
#endif

#include "threadutils.h"

namespace Utilities {

long SetThreadIOPriority(const IoPriority priority) {

#ifdef Q_OS_LINUX
  return syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, GetThreadId(), 4 | static_cast<int>(priority) << IOPRIO_CLASS_SHIFT);
#elif defined(Q_OS_MACOS)
  return setpriority(PRIO_DARWIN_THREAD, 0, priority == IoPriority::IOPRIO_CLASS_IDLE ? PRIO_DARWIN_BG : 0);
#else
  Q_UNUSED(priority);
  return 0;
#endif

}

long GetThreadId() {

#ifdef Q_OS_LINUX
  return syscall(SYS_gettid);
#else
  return 0;
#endif

}

}  // namespace Utilities
