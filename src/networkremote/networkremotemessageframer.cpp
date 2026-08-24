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

#include "networkremotemessageframer.h"

#include <QDataStream>

#include "networkremoteprotothelper.h"

void NetworkRemoteMessageFramer::Feed(const QByteArray &data) {
  buffer_.append(data);
}

void NetworkRemoteMessageFramer::Clear() {
  buffer_.clear();
}

NetworkRemoteMessageFramer::Status NetworkRemoteMessageFramer::NextFrame(QByteArray &payload) {
  if (buffer_.size() < 4) {
    return Status::NeedMoreData;
  }

  QDataStream len_stream(buffer_.left(4));
  len_stream.setByteOrder(QDataStream::BigEndian);
  quint32 msg_len = 0;
  len_stream >> msg_len;

  if (msg_len > kMaxMsgLen) {
    return Status::OversizedFrame;
  }

  if (static_cast<quint64>(buffer_.size()) < 4ULL + msg_len) {
    return Status::NeedMoreData;
  }

  payload = buffer_.mid(4, msg_len);
  buffer_.remove(0, 4 + msg_len);
  return Status::FrameReady;
}
