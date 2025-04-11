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

#ifndef DISCORD_RPC_CONNECTION_H
#define DISCORD_RPC_CONNECTION_H

#include "discord_connection.h"
#include "discord_serialization.h"

namespace discord_rpc {

// I took this from the buffer size libuv uses for named pipes; I suspect ours would usually be much smaller.
constexpr size_t MaxRpcFrameSize = 64 * 1024;

struct RpcConnection {
  enum class ErrorCode : int {
    Success = 0,
    PipeClosed = 1,
    ReadCorrupt = 2,
  };

  enum class Opcode : uint32_t {
    Handshake = 0,
    Frame = 1,
    Close = 2,
    Ping = 3,
    Pong = 4,
  };

  struct MessageFrameHeader {
    Opcode opcode;
    uint32_t length;
  };

  struct MessageFrame : public MessageFrameHeader {
    char message[MaxRpcFrameSize - sizeof(MessageFrameHeader)];
  };

  enum class State : uint32_t {
    Disconnected,
    SentHandshake,
    AwaitingResponse,
    Connected,
  };

  BaseConnection *connection { nullptr };
  State state { State::Disconnected };
  void (*onConnect)(JsonDocument &message) { nullptr };
  void (*onDisconnect)(int errorCode, const char *message) { nullptr };
  char appId[64] {};
  int lastErrorCode { 0 };
  char lastErrorMessage[256] {};
  RpcConnection::MessageFrame sendFrame;

  static RpcConnection *Create(const char *applicationId);
  static void Destroy(RpcConnection *&);

  inline bool IsOpen() const { return state == State::Connected; }

  void Open();
  void Close();
  bool Write(const void *data, size_t length);
  bool Read(JsonDocument &message);
};

}  // namespace discord_rpc

#endif  // DISCORD_RPC_CONNECTION_H
