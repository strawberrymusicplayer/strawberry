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

#ifndef DISCORD_RPC_H
#define DISCORD_RPC_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DiscordRichPresence {
  int type;
  int status_display_type;
  const char *name;    /* max 128 bytes */
  const char *state;   /* max 128 bytes */
  const char *details; /* max 128 bytes */
  int64_t startTimestamp;
  int64_t endTimestamp;
  const char *largeImageKey;  /* max 32 bytes */
  const char *largeImageText; /* max 128 bytes */
  const char *smallImageKey;  /* max 32 bytes */
  const char *smallImageText; /* max 128 bytes */
  const char *partyId;        /* max 128 bytes */
  int partySize;
  int partyMax;
  int partyPrivacy;
  const char *matchSecret;    /* max 128 bytes */
  const char *joinSecret;     /* max 128 bytes */
  const char *spectateSecret; /* max 128 bytes */
  int8_t instance;
} DiscordRichPresence;

typedef struct DiscordUser {
  const char *userId;
  const char *username;
  const char *discriminator;
  const char *avatar;
} DiscordUser;

typedef struct DiscordEventHandlers {
  void (*ready)(const DiscordUser *request);
  void (*disconnected)(int errorCode, const char *message);
  void (*errored)(int errorCode, const char *message);
  void (*joinGame)(const char *joinSecret);
  void (*spectateGame)(const char *spectateSecret);
  void (*joinRequest)(const DiscordUser *request);
} DiscordEventHandlers;

#define DISCORD_REPLY_NO      0
#define DISCORD_REPLY_YES     1
#define DISCORD_REPLY_IGNORE  2
#define DISCORD_PARTY_PRIVATE 0
#define DISCORD_PARTY_PUBLIC  1

void Discord_Initialize(const char *applicationId, DiscordEventHandlers *handlers, const int autoRegister);
void Discord_Shutdown(void);

// checks for incoming messages, dispatches callbacks
void Discord_RunCallbacks(void);

void Discord_UpdatePresence(const DiscordRichPresence *presence);
void Discord_ClearPresence(void);

void Discord_Respond(const char *userid, /* DISCORD_REPLY_ */ int reply);

void Discord_UpdateHandlers(DiscordEventHandlers *handlers);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // DISCORD_RPC_H
