/*
 * Strawberry Music Player
 * This code was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QAbstractListModel>
#include <QVariant>
#include <QStringList>
#include <QMimeData>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/iconloader.h"
#include "core/simpletreemodel.h"
#include "core/settings.h"
#include "collection/collectionbackend.h"

#include "smartplaylistsitem.h"
#include "smartplaylistsmodel.h"
#include "smartplaylistsearch.h"
#include "playlistgenerator.h"
#include "playlistgeneratormimedata.h"
#include "playlistquerygenerator.h"

using namespace Qt::Literals::StringLiterals;

const char *SmartPlaylistsModel::kSettingsGroup = "SerializedSmartPlaylists";
const char *SmartPlaylistsModel::kSmartPlaylistsMimeType = "application/x-strawberry-smart-playlist-generator";
const int SmartPlaylistsModel::kSmartPlaylistsVersion = 1;

SmartPlaylistsModel::SmartPlaylistsModel(SharedPtr<CollectionBackend> collection_backend, QObject *parent)
    : SimpleTreeModel<SmartPlaylistsItem>(new SmartPlaylistsItem(this), parent),
      collection_backend_(collection_backend),
      icon_(IconLoader::Load(u"view-media-playlist"_s)) {}

SmartPlaylistsModel::~SmartPlaylistsModel() { delete root_; }

void SmartPlaylistsModel::Init() {

  default_smart_playlists_ =
    SmartPlaylistsModel::DefaultGenerators()
    << (SmartPlaylistsModel::GeneratorList()
          << PlaylistGeneratorPtr(
              new PlaylistQueryGenerator(
              QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Newest tracks")),
              SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(),
              SmartPlaylistSearch::SortType::FieldDesc,
              SmartPlaylistSearchTerm::Field::DateCreated)
            )
          )
          << PlaylistGeneratorPtr(new PlaylistQueryGenerator(
              QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "50 random tracks")),
              SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title, 50)
            )
          )
          << PlaylistGeneratorPtr(
              new PlaylistQueryGenerator(
              QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Ever played")),
              SmartPlaylistSearch(SmartPlaylistSearch::SearchType::And, SmartPlaylistSearch::TermList() << SmartPlaylistSearchTerm( SmartPlaylistSearchTerm::Field::PlayCount, SmartPlaylistSearchTerm::Operator::GreaterThan, 0), SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title)
            )
          )
          << PlaylistGeneratorPtr(
             new PlaylistQueryGenerator(
             QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Never played")),
             SmartPlaylistSearch(SmartPlaylistSearch::SearchType::And, SmartPlaylistSearch::TermList() << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::PlayCount, SmartPlaylistSearchTerm::Operator::Equals, 0), SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title)
            )
          )
          << PlaylistGeneratorPtr(
             new PlaylistQueryGenerator(
             QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Last played")),
             SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldDesc, SmartPlaylistSearchTerm::Field::LastPlayed)
            )
          )
          << PlaylistGeneratorPtr(
             new PlaylistQueryGenerator(
             QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Most played")),
             SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldDesc, SmartPlaylistSearchTerm::Field::PlayCount)
            )
          )
          << PlaylistGeneratorPtr(
             new PlaylistQueryGenerator(
             QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Favourite tracks")),
             SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldDesc, SmartPlaylistSearchTerm::Field::Rating)
            )
          )
          << PlaylistGeneratorPtr(
             new PlaylistQueryGenerator(
             QStringLiteral(QT_TRANSLATE_NOOP("Library", "Least favourite tracks")),
                 SmartPlaylistSearch(SmartPlaylistSearch::SearchType::Or, SmartPlaylistSearch::TermList()
                 << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Rating, SmartPlaylistSearchTerm::Operator::LessThan, 0.5)
                 << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::SkipCount, SmartPlaylistSearchTerm::Operator::GreaterThan, 4), SmartPlaylistSearch::SortType::FieldDesc, SmartPlaylistSearchTerm::Field::SkipCount)
             )
           )
         )
    << (SmartPlaylistsModel::GeneratorList() << PlaylistGeneratorPtr(new PlaylistQueryGenerator(QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "All tracks")), SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Artist, -1))))
    << (SmartPlaylistsModel::GeneratorList() << PlaylistGeneratorPtr(new PlaylistQueryGenerator(QStringLiteral(QT_TRANSLATE_NOOP("SmartPlaylists", "Dynamic random mix")), SmartPlaylistSearch(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title), true)));

  Settings s;
  s.beginGroup(kSettingsGroup);
  int version = s.value(collection_backend_->songs_table() + u"_version"_s, 0).toInt();

  // How many defaults do we have to write?
  int unwritten_defaults = 0;
  for (int i = version; i < default_smart_playlists_.count(); ++i) {
    unwritten_defaults += static_cast<int>(default_smart_playlists_.value(i).count());
  }

  // Save the defaults if there are any unwritten ones
  if (unwritten_defaults > 0) {
    // How many items are stored already?
    int playlist_index = s.beginReadArray(collection_backend_->songs_table());
    s.endArray();

    // Append the new ones
    s.beginWriteArray(collection_backend_->songs_table(), playlist_index + unwritten_defaults);
    for (; version < default_smart_playlists_.count(); ++version) {
      const GeneratorList generators = default_smart_playlists_.value(version);
      for (PlaylistGeneratorPtr gen : generators) {
        SaveGenerator(&s, playlist_index++, gen);
      }
    }
    s.endArray();
  }

  s.setValue(collection_backend_->songs_table() + u"_version"_s, version);

  const int count = s.beginReadArray(collection_backend_->songs_table());
  for (int i = 0; i < count; ++i) {
    s.setArrayIndex(i);
    ItemFromSmartPlaylist(s, false);
  }
  s.endArray();
  s.endGroup();

}

void SmartPlaylistsModel::ItemFromSmartPlaylist(const Settings &s, const bool notify) {

  SmartPlaylistsItem *item = new SmartPlaylistsItem(SmartPlaylistsItem::Type::SmartPlaylist, notify ? nullptr : root_);
  item->display_text = tr(qUtf8Printable(s.value("name").toString()));
  item->sort_text = item->display_text;
  item->smart_playlist_type = PlaylistGenerator::Type(s.value("type").toInt());
  item->smart_playlist_data = s.value("data").toByteArray();

  if (notify) item->InsertNotify(root_);

}

void SmartPlaylistsModel::AddGenerator(PlaylistGeneratorPtr gen) {

  Settings s;
  s.beginGroup(kSettingsGroup);

  // Count the existing items
  const int count = s.beginReadArray(collection_backend_->songs_table());
  s.endArray();

  // Add this one to the end
  s.beginWriteArray(collection_backend_->songs_table(), count + 1);
  SaveGenerator(&s, count, gen);

  // Add it to the model
  ItemFromSmartPlaylist(s, true);

  s.endArray();
  s.endGroup();

}

void SmartPlaylistsModel::UpdateGenerator(const QModelIndex &idx, PlaylistGeneratorPtr gen) {

  if (idx.parent() != ItemToIndex(root_)) return;
  SmartPlaylistsItem *item = IndexToItem(idx);
  if (!item) return;

  // Update the config
  Settings s;
  s.beginGroup(kSettingsGroup);

  // Count the existing items
  const int count = s.beginReadArray(collection_backend_->songs_table());
  s.endArray();

  s.beginWriteArray(collection_backend_->songs_table(), count);
  SaveGenerator(&s, idx.row(), gen);

  s.endArray();
  s.endGroup();

  // Update the text of the item
  item->display_text = gen->name();
  item->sort_text = item->display_text;
  item->smart_playlist_type = gen->type();
  item->smart_playlist_data = gen->Save();
  item->ChangedNotify();

}

void SmartPlaylistsModel::DeleteGenerator(const QModelIndex &idx) {

  if (idx.parent() != ItemToIndex(root_)) return;

  // Remove the item from the tree
  root_->DeleteNotify(idx.row());

  Settings s;
  s.beginGroup(kSettingsGroup);

  // Rewrite all the items to the settings
  s.beginWriteArray(collection_backend_->songs_table(), static_cast<int>(root_->children.count()));
  int i = 0;
  const QList<SmartPlaylistsItem*> children = root_->children;
  for (SmartPlaylistsItem *item : children) {
    s.setArrayIndex(i++);
    s.setValue("name", item->display_text);
    s.setValue("type", static_cast<int>(item->smart_playlist_type));
    s.setValue("data", item->smart_playlist_data);
  }
  s.endArray();
  s.endGroup();

}

void SmartPlaylistsModel::SaveGenerator(Settings *s, const int i, PlaylistGeneratorPtr generator) {

  s->setArrayIndex(i);
  s->setValue("name", generator->name());
  s->setValue("type", static_cast<int>(generator->type()));
  s->setValue("data", generator->Save());

}

PlaylistGeneratorPtr SmartPlaylistsModel::CreateGenerator(const QModelIndex &idx) const {

  PlaylistGeneratorPtr ret;

  const SmartPlaylistsItem *item = IndexToItem(idx);
  if (!item || item->type != SmartPlaylistsItem::Type::SmartPlaylist) return ret;

  ret = PlaylistGenerator::Create(item->smart_playlist_type);
  if (!ret) return ret;

  ret->set_name(item->display_text);
  ret->set_collection_backend(collection_backend_);
  ret->Load(item->smart_playlist_data);

  return ret;

}

QVariant SmartPlaylistsModel::data(const QModelIndex &idx, const int role) const {

  if (!idx.isValid()) return QVariant();
  const SmartPlaylistsItem *item = IndexToItem(idx);
  if (!item) return QVariant();

  switch (role) {
    case Qt::DecorationRole:
      return icon_;
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      return item->DisplayText();
    default:
      return QVariant();
  }

}

QStringList SmartPlaylistsModel::mimeTypes() const {
  return QStringList() << u"text/uri-list"_s;
}

QMimeData *SmartPlaylistsModel::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  PlaylistGeneratorPtr generator = CreateGenerator(indexes.first());
  if (!generator) return nullptr;

  PlaylistGeneratorMimeData *mimedata = new PlaylistGeneratorMimeData(generator);
  mimedata->setData(QLatin1String(kSmartPlaylistsMimeType), QByteArray());
  mimedata->name_for_new_playlist_ = data(indexes.first()).toString();
  return mimedata;

}
