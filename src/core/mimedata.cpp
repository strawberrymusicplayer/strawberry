/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QString>

#include "mimedata.h"

MimeData::MimeData(const bool clear, const bool play_now, const bool enqueue, const bool enqueue_next_now, const bool open_in_new_playlist, QObject *parent)
      : override_user_settings_(false),
        clear_first_(clear),
        play_now_(play_now),
        enqueue_now_(enqueue),
        enqueue_next_now_(enqueue_next_now),
        open_in_new_playlist_(open_in_new_playlist),
        from_doubleclick_(false) {

  Q_UNUSED(parent);

}

QString MimeData::get_name_for_new_playlist() const {
  return name_for_new_playlist_.isEmpty() ? tr("Playlist") : name_for_new_playlist_;
}
