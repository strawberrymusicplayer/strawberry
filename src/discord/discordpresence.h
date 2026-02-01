/*
 * Strawberry Music Player
 * Copyright 2025-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DISCORDPRESENCE_H
#define DISCORDPRESENCE_H

#include <QString>

struct DiscordPresence {
  DiscordPresence();

  int type;
  int status_display_type;
  QString name;
  QString state;
  QString details;
  qint64 start_timestamp;
  qint64 end_timestamp;
  QString large_image_key;
  QString large_image_text;
  QString small_image_key;
  QString small_image_text;
  QString party_id;
  int party_size;
  int party_max;
  int party_privacy;
  QString match_secret;
  QString join_secret;
  QString spectate_secret;
  bool instance;
};

#endif  // DISCORDPRESENCE_H
