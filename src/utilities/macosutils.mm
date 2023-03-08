/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

  // Bump the soft limit for the number of file descriptors from the default of 256 to the maximum (usually 10240).
  struct rlimit limit;
  getrlimit(RLIMIT_NOFILE, &limit);

  // getrlimit() lies about the hard limit so we have to check sysctl.
  int max_fd = 0;
  size_t len = sizeof(max_fd);
  sysctlbyname("kern.maxfilesperproc", &max_fd, &len, nullptr, 0);

  limit.rlim_cur = max_fd;
  int ret = setrlimit(RLIMIT_NOFILE, &limit);

  if (ret == 0) {
    qLog(Debug) << "Max fd:" << max_fd;
  }

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
