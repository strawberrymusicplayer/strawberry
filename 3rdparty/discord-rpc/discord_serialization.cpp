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

#include "discord_serialization.h"
#include "discord_connection.h"
#include "discord_rpc.h"

namespace discord_rpc {

template<typename T>
void NumberToString(char *dest, T number) {

  if (!number) {
    *dest++ = '0';
    *dest++ = 0;
    return;
  }
  if (number < 0) {
    *dest++ = '-';
    number = -number;
  }
  char temp[32];
  int place = 0;
  while (number) {
    auto digit = number % 10;
    number = number / 10;
    temp[place++] = '0' + static_cast<char>(digit);
  }
  for (--place; place >= 0; --place) {
    *dest++ = temp[place];
  }
  *dest = 0;

}

// it's ever so slightly faster to not have to strlen the key
template<typename T>
void WriteKey(JsonWriter &w, T &k) {
  w.Key(k, sizeof(T) - 1);
}

struct WriteObject {
  JsonWriter &writer;
  WriteObject(JsonWriter &w)
      : writer(w) {
    writer.StartObject();
  }
  template<typename T>
  WriteObject(JsonWriter &w, T &name)
      : writer(w) {
    WriteKey(writer, name);
    writer.StartObject();
  }
  ~WriteObject() { writer.EndObject(); }
};

struct WriteArray {
  JsonWriter &writer;
  template<typename T>
  WriteArray(JsonWriter &w, T &name)
      : writer(w) {
    WriteKey(writer, name);
    writer.StartArray();
  }
  ~WriteArray() { writer.EndArray(); }
};

template<typename T>
void WriteOptionalString(JsonWriter &w, T &k, const char *value) {

  if (value && value[0]) {
    w.Key(k, sizeof(T) - 1);
    w.String(value);
  }

}

static void JsonWriteNonce(JsonWriter &writer, const int nonce) {

  WriteKey(writer, "nonce");
  char nonceBuffer[32];
  NumberToString(nonceBuffer, nonce);
  writer.String(nonceBuffer);

}

size_t JsonWriteRichPresenceObj(char *dest, const size_t maxLen, const int nonce, const int pid, const DiscordRichPresence *presence) {

  JsonWriter writer(dest, maxLen);

  {
    WriteObject top(writer);

    JsonWriteNonce(writer, nonce);

    WriteKey(writer, "cmd");
    writer.String("SET_ACTIVITY");

    {
      WriteObject args(writer, "args");

      WriteKey(writer, "pid");
      writer.Int(pid);

      if (presence != nullptr) {
        WriteObject activity(writer, "activity");

        if (presence->type >= 0 && presence->type <= 5) {
          WriteKey(writer, "type");
          writer.Int(presence->type);

          WriteKey(writer, "status_display_type");
          writer.Int(presence->status_display_type);
        }

        WriteOptionalString(writer, "name", presence->name);
        WriteOptionalString(writer, "state", presence->state);
        WriteOptionalString(writer, "details", presence->details);

        if (presence->startTimestamp || presence->endTimestamp) {
          WriteObject timestamps(writer, "timestamps");

          if (presence->startTimestamp) {
            WriteKey(writer, "start");
            writer.Int64(presence->startTimestamp);
          }

          if (presence->endTimestamp) {
            WriteKey(writer, "end");
            writer.Int64(presence->endTimestamp);
          }
        }

        if ((presence->largeImageKey && presence->largeImageKey[0]) ||
            (presence->largeImageText && presence->largeImageText[0]) ||
            (presence->smallImageKey && presence->smallImageKey[0]) ||
            (presence->smallImageText && presence->smallImageText[0])) {
          WriteObject assets(writer, "assets");
          WriteOptionalString(writer, "large_image", presence->largeImageKey);
          WriteOptionalString(writer, "large_text", presence->largeImageText);
          WriteOptionalString(writer, "small_image", presence->smallImageKey);
          WriteOptionalString(writer, "small_text", presence->smallImageText);
        }

        if ((presence->partyId && presence->partyId[0]) || presence->partySize ||
            presence->partyMax || presence->partyPrivacy) {
          WriteObject party(writer, "party");
          WriteOptionalString(writer, "id", presence->partyId);
          if (presence->partySize && presence->partyMax) {
            WriteArray size(writer, "size");
            writer.Int(presence->partySize);
            writer.Int(presence->partyMax);
          }

          if (presence->partyPrivacy) {
            WriteKey(writer, "privacy");
            writer.Int(presence->partyPrivacy);
          }
        }

        if ((presence->matchSecret && presence->matchSecret[0]) ||
            (presence->joinSecret && presence->joinSecret[0]) ||
            (presence->spectateSecret && presence->spectateSecret[0])) {
          WriteObject secrets(writer, "secrets");
          WriteOptionalString(writer, "match", presence->matchSecret);
          WriteOptionalString(writer, "join", presence->joinSecret);
          WriteOptionalString(writer, "spectate", presence->spectateSecret);
        }

        writer.Key("instance");
        writer.Bool(presence->instance != 0);
      }
    }
  }

  return writer.Size();
}

size_t JsonWriteHandshakeObj(char *dest, size_t maxLen, int version, const char *applicationId) {

  JsonWriter writer(dest, maxLen);

  {
    WriteObject obj(writer);
    WriteKey(writer, "v");
    writer.Int(version);
    WriteKey(writer, "client_id");
    writer.String(applicationId);
  }

  return writer.Size();

}

size_t JsonWriteSubscribeCommand(char *dest, size_t maxLen, int nonce, const char *evtName) {

  JsonWriter writer(dest, maxLen);

  {
    WriteObject obj(writer);

    JsonWriteNonce(writer, nonce);

    WriteKey(writer, "cmd");
    writer.String("SUBSCRIBE");

    WriteKey(writer, "evt");
    writer.String(evtName);
  }

  return writer.Size();

}

size_t JsonWriteUnsubscribeCommand(char *dest, size_t maxLen, int nonce, const char *evtName) {

  JsonWriter writer(dest, maxLen);

  {
    WriteObject obj(writer);

    JsonWriteNonce(writer, nonce);

    WriteKey(writer, "cmd");
    writer.String("UNSUBSCRIBE");

    WriteKey(writer, "evt");
    writer.String(evtName);
  }

  return writer.Size();

}

size_t JsonWriteJoinReply(char *dest, size_t maxLen, const char *userId, const int reply, const int nonce) {

  JsonWriter writer(dest, maxLen);

  {
    WriteObject obj(writer);

    WriteKey(writer, "cmd");
    if (reply == DISCORD_REPLY_YES) {
      writer.String("SEND_ACTIVITY_JOIN_INVITE");
    }
    else {
      writer.String("CLOSE_ACTIVITY_JOIN_REQUEST");
    }

    WriteKey(writer, "args");
    {
      WriteObject args(writer);

      WriteKey(writer, "user_id");
      writer.String(userId);
    }

    JsonWriteNonce(writer, nonce);
  }

  return writer.Size();

}

}  // namespace discord_rpc
