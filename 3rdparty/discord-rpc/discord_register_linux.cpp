/*
 * Copyright 2017 Discord, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "discord_rpc.h"
#include "discord_register.h"

#include <cstdio>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

static bool Mkdir(const char *path) {
  int result = mkdir(path, 0755);
  if (result == 0) {
    return true;
  }
  if (errno == EEXIST) {
    return true;
  }
  return false;
}

}  // namespace

// We want to register games so we can run them from Discord client as discord-<appid>://
extern "C" void Discord_Register(const char *applicationId, const char *command) {

  // Add a desktop file and update some mime handlers so that xdg-open does the right thing.

  const char *home = getenv("HOME");
  if (!home) {
    return;
  }

  char exePath[1024]{};
  if (!command || !command[0]) {
    const ssize_t size = readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (size <= 0 || size >= static_cast<ssize_t>(sizeof(exePath))) {
      return;
    }
    exePath[size] = '\0';
    command = exePath;
  }

  constexpr char desktopFileFormat[] = "[Desktop Entry]\n"
                                       "Name=Game %s\n"
                                       "Exec=%s %%u\n"  // note: it really wants that %u in there
                                       "Type=Application\n"
                                       "NoDisplay=true\n"
                                       "Categories=Discord;Games;\n"
                                       "MimeType=x-scheme-handler/discord-%s;\n";
  char desktopFile[2048]{};
  int fileLen = snprintf(desktopFile, sizeof(desktopFile), desktopFileFormat, applicationId, command, applicationId);
  if (fileLen <= 0) {
    return;
  }

  char desktopFilename[256]{};
  (void)snprintf(desktopFilename, sizeof(desktopFilename), "/discord-%s.desktop", applicationId);

  char desktopFilePath[1024]{};
  (void)snprintf(desktopFilePath, sizeof(desktopFilePath), "%s/.local", home);
  if (!Mkdir(desktopFilePath)) {
    return;
  }
  strcat(desktopFilePath, "/share");
  if (!Mkdir(desktopFilePath)) {
    return;
  }
  strcat(desktopFilePath, "/applications");
  if (!Mkdir(desktopFilePath)) {
    return;
  }
  strcat(desktopFilePath, desktopFilename);

  FILE *fp = fopen(desktopFilePath, "w");
  if (fp) {
    fwrite(desktopFile, 1, fileLen, fp);
    fclose(fp);
  }
  else {
    return;
  }

  char xdgMimeCommand[1024]{};
  snprintf(xdgMimeCommand,
           sizeof(xdgMimeCommand),
           "xdg-mime default discord-%s.desktop x-scheme-handler/discord-%s",
           applicationId,
           applicationId);
  if (system(xdgMimeCommand) < 0) {
    fprintf(stderr, "Failed to register mime handler\n");
  }

}
