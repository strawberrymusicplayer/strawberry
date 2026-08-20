/*
 * Strawberry Music Player
 * Copyright 2025, Leopold List <leo@zudiewiener.com>
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

#ifndef NETWORKREMOTETYPES_H
#define NETWORKREMOTETYPES_H

#include <QtGlobal>
#include "networkremote/RemoteMessages.qpb.h"

// Convenience aliases for the generated protobuf types in nw::remote.
//
// - `nwr` shortens the namespace itself; every generated message/request/
//   response type (Message, ResponseConnect, RequestPlaySong, ...) should
//   still be referenced through it (e.g. nwr::ResponseConnect) so it stays
//   visually clear the type comes from the wire protocol, not local code.
//
// - The four enum types below get their own bare aliases on top of that.
//   Qt's protobuf generator wraps every proto `enum` in a `Q_GADGET` struct
//   (e.g. MsgTypeGadget) so it can be exposed to Qt's meta-object system,
//   with the actual enum nested one level inside (MsgTypeGadget::MsgType).
//   That extra Gadget layer is pure codegen boilerplate, not meaningful
//   namespacing, so it's collapsed here rather than repeated at every call
//   site (nwr::MsgTypeGadget::MsgType::MSG_TYPE_PLAY -> MsgType::MSG_TYPE_PLAY).
//
// Include this header (rather than redeclaring these locally) in any file
// that works with the NetworkRemote wire protocol.
namespace nwr = nw::remote;

using MsgType = nwr::MsgTypeGadget::MsgType;
using ReasonDisconnect = nwr::ReasonDisconnectGadget::ReasonDisconnect;
using PlaylistRejectReason = nwr::PlaylistRejectReasonGadget::PlaylistRejectReason;
using PlayerState = nwr::PlayerStateGadget::PlayerState;
using EngineState = nwr::EngineStateGadget::EngineState;

// Protocol version history:
// 1 - initial protocol (song info, transport control, engine state push)
// 2 - position/length in ResponseSongMetadata, version field in Message
constexpr quint32 kProtocolVersion = 5;

// Oldest client protocol version this server accepts.
// 0 = clients that predate the version field.
constexpr quint32 kMinSupportedVersion = 5;

// Maximum size (bytes) of a single incoming protobuf message payload,
// excluding the 4-byte length header. Protects against a peer declaring
// (or the accumulated buffer implying) an unreasonably large frame.
constexpr quint32 kMaxMsgLen = 1024 * 1024; // 1 MiB

// Maximum bytes allowed to accumulate in the incoming read buffer before a
// complete frame has been assembled. Guards against unbounded growth if a
// peer never completes a frame (see networkremoteincomingmsg.cpp).
constexpr quint64 kMaxBufferedBytes = 4ULL + kMaxMsgLen;

// Maximum bytes allowed to sit unsent in a client's outbound socket buffer
// before the connection is considered stalled and dropped. Qt imposes no
// write-buffer limit itself (see QAbstractSocket docs), so this guards
// against unbounded memory growth from a slow or unresponsive peer.
constexpr qint64 kMaxOutboundBufferBytes = 4 * 1024 * 1024; // 4 MiB

#endif // NETWORKREMOTETYPES_H
