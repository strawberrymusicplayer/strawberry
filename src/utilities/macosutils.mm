/*
 * Strawberry Music Player
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <QtGlobal>

#include <limits.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>

#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

#include "macosutils.h"

#include "core/logging.h"

namespace Utilities {

qint32 GetMacOsVersion() {

  NSOperatingSystemVersion version = [ [NSProcessInfo processInfo] operatingSystemVersion];
  return version.minorVersion;

}

void IncreaseFDLimit() {

  // Raise the soft limit for the number of file descriptors from the default of 256.
  struct rlimit limit {};
  if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
    qLog(Error) << "Could not get the file descriptor limit:" << strerror(errno);
    return;
  }

  // setrlimit() fails with EINVAL when the soft limit is above OPEN_MAX, even when kern.maxfilesperproc and the hard limit are higher.
  rlim_t max_fd = OPEN_MAX;

  // getrlimit() can report an unlimited hard limit, so the per process maximum is checked too.
  int max_files_per_process = 0;
  size_t len = sizeof(max_files_per_process);
  if (sysctlbyname("kern.maxfilesperproc", &max_files_per_process, &len, nullptr, 0) == 0 && max_files_per_process > 0) {
    max_fd = std::min(max_fd, static_cast<rlim_t>(max_files_per_process));
  }
  max_fd = std::min(max_fd, limit.rlim_max);

  // Never lower the limit.
  if (limit.rlim_cur >= max_fd) {
    qLog(Debug) << "Max fd:" << static_cast<quint64>(limit.rlim_cur);
    return;
  }

  limit.rlim_cur = max_fd;
  if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
    qLog(Error) << "Could not set the file descriptor limit to" << static_cast<quint64>(max_fd) << strerror(errno);
    return;
  }

  qLog(Debug) << "Max fd:" << static_cast<quint64>(max_fd);

}

bool ProcessTranslated() {

  int value = 0;
  size_t value_size = sizeof(value);
  if (sysctlbyname("sysctl.proc_translated", &value, &value_size, nullptr, 0) != 0) {
    return false;
  }

  return value == 1;

}

}  // namespace Utilities
