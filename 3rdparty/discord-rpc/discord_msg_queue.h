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

#ifndef DISCORD_MSG_QUEUE_H
#define DISCORD_MSG_QUEUE_H

#include <atomic>

// A simple queue. No locks, but only works with a single thread as producer and a single thread as
// a consumer. Mutex up as needed.

namespace discord_rpc {

template<typename ElementType, std::size_t QueueSize>
class MsgQueue {
  ElementType queue_[QueueSize];
  std::atomic_uint nextAdd_ { 0 };
  std::atomic_uint nextSend_ { 0 };
  std::atomic_uint pendingSends_ { 0 };

 public:
  MsgQueue() {}

  ElementType *GetNextAddMessage() {
    // if we are falling behind, bail
    if (pendingSends_.load() >= QueueSize) {
      return nullptr;
    }
    auto index = (nextAdd_++) % QueueSize;
    return &queue_[index];
  }
  void CommitAdd() { ++pendingSends_; }

  bool HavePendingSends() const { return pendingSends_.load() != 0; }
  ElementType *GetNextSendMessage() {
    auto index = (nextSend_++) % QueueSize;
    return &queue_[index];
  }
  void CommitSend() { --pendingSends_; }
};

}  // namespace discord_rpc

#endif  // DISCORD_MSG_QUEUE_H
