/*
 * Strawberry Music Player
 * Copyright 2026, Leopold List <leo@zudiewiener.com>
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

#ifndef NETWORKREMOTEMESSAGEFRAMER_H
#define NETWORKREMOTEMESSAGEFRAMER_H

#include <QByteArray>

// Extracts length-prefixed protobuf frames from a byte stream. Deliberately
// has no dependency on QTcpSocket or any other I/O source, so it can be fed
// bytes from a real connection or directly from a test with no socket at all.
// Each frame is a 4-byte big-endian length prefix followed by that many
// bytes of serialized protobuf payload.
class NetworkRemoteMessageFramer {
 public:
  enum class Status {
    NeedMoreData,    // Not enough bytes buffered yet for the next frame.
    FrameReady,      // A complete frame was extracted into the output payload.
    OversizedFrame,  // The declared frame length exceeds kMaxMsgLen.
  };

  // Appends newly-received bytes to the internal buffer.
  void Feed(const QByteArray &data);

  // Attempts to extract the next complete frame's payload from the buffer.
  // Call repeatedly until it returns something other than FrameReady to
  // drain every frame currently available.
  Status NextFrame(QByteArray &payload);

  // Discards all buffered bytes, e.g. after a fatal framing error.
  void Clear();

 private:
  QByteArray buffer_;
};

#endif  // NETWORKREMOTEMESSAGEFRAMER_H
