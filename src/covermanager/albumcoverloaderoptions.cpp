/*
* Strawberry Music Player
* Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include "albumcoverloaderoptions.h"

#include <QSettings>

#include "core/settings.h"
#include "constants/coverssettings.h"

using namespace Qt::Literals::StringLiterals;

AlbumCoverLoaderOptions::AlbumCoverLoaderOptions(const Options _options, const QSize _desired_scaled_size, const qreal _device_pixel_ratio, const Types &_types)
    : options(_options),
      desired_scaled_size(_desired_scaled_size),
      device_pixel_ratio(_device_pixel_ratio),
      types(_types) {}

AlbumCoverLoaderOptions::Types AlbumCoverLoaderOptions::LoadTypes() {

  Types cover_types;

  Settings s;
  s.beginGroup(CoversSettings::kSettingsGroup);
  const QStringList all_cover_types = QStringList() << u"art_unset"_s << u"art_embedded"_s << u"art_manual"_s << u"art_automatic"_s;
  const QStringList cover_types_strlist = s.value(CoversSettings::kTypes, all_cover_types).toStringList();
  for (const QString &cover_type_str : cover_types_strlist) {
    if (cover_type_str == "art_unset"_L1) {
      cover_types << AlbumCoverLoaderOptions::Type::Unset;
    }
    else if (cover_type_str == "art_embedded"_L1) {
      cover_types << AlbumCoverLoaderOptions::Type::Embedded;
    }
    else if (cover_type_str == "art_manual"_L1) {
      cover_types << AlbumCoverLoaderOptions::Type::Manual;
    }
    else if (cover_type_str == "art_automatic"_L1) {
      cover_types << AlbumCoverLoaderOptions::Type::Automatic;
    }
  }

  s.endGroup();

  return cover_types;

}
