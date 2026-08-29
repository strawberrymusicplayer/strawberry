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

#include "gtest_include.h"
#include "gmock_include.h"

#include <QByteArray>
#include <QDataStream>
#include <QProtobufSerializer>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QProtobufSerializer>

#include "networkremote/networkremotemessageframer.h"
#include "networkremote/networkremotehelper.h"
#include "networkremote/RemoteMessages.qpb.h"

using namespace nwr_types;
namespace {
using Status = NetworkRemoteMessageFramer::Status;

// Serializes a Message the same way NetworkRemoteOutgoingMsg::SendMsg() does, a 4-byte big-endian length prefix followed by the serialized payload.
QByteArray BuildFrame(nwr::Message msg) {
  msg.setVersion(kProtocolVersion);
  QProtobufSerializer serializer;
  const QByteArray payload = serializer.serialize(&msg);

  QByteArray framed;
  QDataStream len_stream(&framed, QIODevice::WriteOnly);
  len_stream.setByteOrder(QDataStream::BigEndian);
  len_stream << static_cast<quint32>(payload.size());
  framed.append(payload);
  return framed;
}

// Builds a raw frame from an explicit declared length and payload bytes, independent of what the payload actually contains.
QByteArray BuildRawFrame(quint32 declared_len, const QByteArray &payload) {
  QByteArray framed;
  QDataStream len_stream(&framed, QIODevice::WriteOnly);
  len_stream.setByteOrder(QDataStream::BigEndian);
  len_stream << declared_len;
  framed.append(payload);
  return framed;
}

nwr::Message MakeRequestPlay() {
  nwr::Message msg;
  msg.setType(MsgType::MSG_TYPE_REQUEST_PLAY);
  nwr::RequestPlay req;
  req.setPlay(true);
  msg.setRequestPlay(req);
  return msg;
}

nwr::Message MakeRequestPause() {
  nwr::Message msg;
  msg.setType(MsgType::MSG_TYPE_REQUEST_PAUSE);
  nwr::RequestPause req;
  req.setPause(true);
  msg.setRequestPause(req);
  return msg;
}

class NetworkRemoteMessageFramerTest : public ::testing::Test {
 protected:
  NetworkRemoteMessageFramer framer_;
};

// Feeding nothing yet leaves the framer waiting for more data.
TEST_F(NetworkRemoteMessageFramerTest, EmptyBufferNeedsMoreData) {
  QByteArray payload;
  EXPECT_EQ(framer_.NextFrame(payload), Status::NeedMoreData);
}

// A single complete frame fed in one call extracts cleanly, and a second call on the now-empty buffer correctly reports there's nothing left.
TEST_F(NetworkRemoteMessageFramerTest, CompleteFrameExtractsInOneCall) {
  const QByteArray frame = BuildFrame(MakeRequestPlay());
  framer_.Feed(frame);

  QByteArray payload;
  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload, frame.mid(4));

  EXPECT_EQ(framer_.NextFrame(payload), Status::NeedMoreData);
}

// A frame whose bytes arrive split across several Feed() calls (e.g. the length header arrives separately from the payload, or the payload itself arrives in pieces) only becomes ready once every byte has arrived.
TEST_F(NetworkRemoteMessageFramerTest, FragmentedFrameAssemblesAcrossFeeds) {
  const QByteArray frame = BuildFrame(MakeRequestPlay());
  ASSERT_GT(frame.size(), 6);

  QByteArray payload;

  // Just the length header.
  framer_.Feed(frame.left(2));
  EXPECT_EQ(framer_.NextFrame(payload), Status::NeedMoreData);

  // Rest of the header plus a couple of payload bytes.
  framer_.Feed(frame.mid(2, 4));
  EXPECT_EQ(framer_.NextFrame(payload), Status::NeedMoreData);

  // Everything else.
  framer_.Feed(frame.mid(6));
  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload, frame.mid(4));
}

// Two complete frames fed together (as they would arrive coalesced in a single socket read) are both extractable, in order, via repeated calls.
TEST_F(NetworkRemoteMessageFramerTest, CoalescedFramesBothExtract) {
  const QByteArray frame_a = BuildFrame(MakeRequestPlay());
  const QByteArray frame_b = BuildFrame(MakeRequestPause());
  framer_.Feed(frame_a + frame_b);

  QByteArray payload;
  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload, frame_a.mid(4));

  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload, frame_b.mid(4));

  EXPECT_EQ(framer_.NextFrame(payload), Status::NeedMoreData);
}

// A declared frame length beyond kMaxMsgLen is rejected as soon as the 4-byte header is available, without needing any payload bytes at all.
TEST_F(NetworkRemoteMessageFramerTest, OversizedFrameIsRejected) {
  framer_.Feed(BuildRawFrame(kMaxMsgLen + 1, QByteArray()));

  QByteArray payload;
  EXPECT_EQ(framer_.NextFrame(payload), Status::OversizedFrame);
}

// A declared length right at kMaxMsgLen is still accepted (the limit is inclusive), pinning the boundary so it can't drift in either direction.
TEST_F(NetworkRemoteMessageFramerTest, MaxSizedFrameIsAccepted) {
  const QByteArray payload_bytes(static_cast<int>(kMaxMsgLen), '\x01');
  framer_.Feed(BuildRawFrame(kMaxMsgLen, payload_bytes));

  QByteArray payload;
  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload.size(), static_cast<int>(kMaxMsgLen));
}


// Several complete frames followed by a still-incomplete trailing frame:
// - the complete ones extract normally and only the trailing partial frame is left pending - this is the scenario the buffer-check-ordering fix specifically targets (a burst of valid traffic must not trip the limit just because it arrived alongside an incomplete tail).
TEST_F(NetworkRemoteMessageFramerTest, CompleteFramesDrainBeforeIncompleteTailIsJudged) {
  const QByteArray frame_a = BuildFrame(MakeRequestPlay());
  const QByteArray frame_b = BuildFrame(MakeRequestPause());
  const QByteArray partial_next = BuildFrame(MakeRequestPlay()).left(3);

  framer_.Feed(frame_a + frame_b + partial_next);

  QByteArray payload;
  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload, frame_a.mid(4));

  ASSERT_EQ(framer_.NextFrame(payload), Status::FrameReady);
  EXPECT_EQ(payload, frame_b.mid(4));

  // Only the trailing partial frame remains, and it's nowhere near the overflow limit, so this must be NeedMoreData, not BufferOverflow.
  EXPECT_EQ(framer_.NextFrame(payload), Status::NeedMoreData);
}
// A frame declaring the maximum legal length never causes the buffer to
// grow unboundedly: kMaxMsgLen alone is sufficient to bound worst-case
// buffer size at 4 + kMaxMsgLen, with no separate buffer-size cap needed.
TEST_F(NetworkRemoteMessageFramerTest, MaxSizedFrameArrivingSlowlyCompletesWithoutIssue) {
    framer_.Feed(BuildRawFrame(kMaxMsgLen, QByteArray()));

    const QByteArray chunk(64 * 1024, '\x01');
    QByteArray payload;
    Status status = Status::NeedMoreData;
    qint64 fed = 4;
    while (fed < static_cast<qint64>(4) + kMaxMsgLen) {
        framer_.Feed(chunk);
        fed += chunk.size();
        status = framer_.NextFrame(payload);
        if (status == Status::FrameReady) break;
    }

    EXPECT_EQ(status, Status::FrameReady);
}

// A single Feed() call large enough to exceed kMaxBufferedBytes on its own -
// e.g. a peer that sends a legal-looking header immediately followed by a
// huge batch of data in one socket read - is rejected outright, rather than
// silently appended and left for NextFrame() to discover too late.
TEST_F(NetworkRemoteMessageFramerTest, OversizedSingleFeedIsRejected) {
    // A legal header (well within kMaxMsgLen), followed by enough bytes that
    // the whole call exceeds kMaxBufferedBytes on its own.
    QByteArray oversized_batch = BuildRawFrame(1000, QByteArray());
    oversized_batch.append(QByteArray(static_cast<int>(kMaxBufferedBytes), '\x01'));

    EXPECT_FALSE(framer_.Feed(oversized_batch));

    // The rejected call must not have corrupted internal state - a
    // subsequent legitimate frame should still parse normally.
    framer_.Feed(BuildFrame(MakeRequestPlay()));
    QByteArray payload;
    EXPECT_EQ(framer_.NextFrame(payload), Status::FrameReady);
}

}  // namespace
