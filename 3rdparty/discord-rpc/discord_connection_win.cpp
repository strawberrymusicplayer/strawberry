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

#include "discord_connection.h"

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOIME

#include <cassert>
#include <windows.h>

namespace discord_rpc {

int GetProcessId() {
  return static_cast<int>(::GetCurrentProcessId());
}

struct BaseConnectionWin : public BaseConnection {
  HANDLE pipe { INVALID_HANDLE_VALUE };
};

static BaseConnectionWin Connection;

BaseConnection *BaseConnection::Create() {
  return &Connection;
}

void BaseConnection::Destroy(BaseConnection *&c) {

  auto self = reinterpret_cast<BaseConnectionWin*>(c);
  self->Close();
  c = nullptr;

}

bool BaseConnection::Open() {

  wchar_t pipeName[] { L"\\\\?\\pipe\\discord-ipc-0" };
  const size_t pipeDigit = sizeof(pipeName) / sizeof(wchar_t) - 2;
  pipeName[pipeDigit] = L'0';
  auto self = reinterpret_cast<BaseConnectionWin *>(this);
  for (;;) {
    self->pipe = ::CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (self->pipe != INVALID_HANDLE_VALUE) {
      self->isOpen = true;
      return true;
    }

    auto lastError = GetLastError();
    if (lastError == ERROR_FILE_NOT_FOUND) {
      if (pipeName[pipeDigit] < L'9') {
        pipeName[pipeDigit]++;
        continue;
      }
    }
    else if (lastError == ERROR_PIPE_BUSY) {
      if (!WaitNamedPipeW(pipeName, 10000)) {
        return false;
      }
      continue;
    }
    return false;
  }

}

bool BaseConnection::Close() {

  auto self = reinterpret_cast<BaseConnectionWin *>(this);
  ::CloseHandle(self->pipe);
  self->pipe = INVALID_HANDLE_VALUE;
  self->isOpen = false;

  return true;

}

bool BaseConnection::Write(const void *data, size_t length) {

  if (length == 0) {
    return true;
  }
  auto self = reinterpret_cast<BaseConnectionWin *>(this);
  assert(self);
  if (!self) {
    return false;
  }
  if (self->pipe == INVALID_HANDLE_VALUE) {
    return false;
  }
  assert(data);
  if (!data) {
    return false;
  }
  const DWORD bytesLength = static_cast<DWORD>(length);
  DWORD bytesWritten = 0;

  return ::WriteFile(self->pipe, data, bytesLength, &bytesWritten, nullptr) == TRUE && bytesWritten == bytesLength;

}

bool BaseConnection::Read(void *data, size_t length) {

  assert(data);
  if (!data) {
    return false;
  }
  auto self = reinterpret_cast<BaseConnectionWin *>(this);
  assert(self);
  if (!self) {
    return false;
  }
  if (self->pipe == INVALID_HANDLE_VALUE) {
    return false;
  }
  DWORD bytesAvailable = 0;
  if (::PeekNamedPipe(self->pipe, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
    if (bytesAvailable >= length) {
      DWORD bytesToRead = static_cast<DWORD>(length);
      DWORD bytesRead = 0;
      if (::ReadFile(self->pipe, data, bytesToRead, &bytesRead, nullptr) == TRUE) {
        assert(bytesToRead == bytesRead);
        return true;
      }
      else {
        Close();
      }
    }
  }
  else {
    Close();
  }

  return false;

}

}  // namespace discord_rpc
