/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <algorithm>
#include <utility>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>

#include <QtGlobal>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QDir>
#include <QAction>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QPixmap>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QSize>
#include <QSpinBox>
#include <QCheckBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QKeySequence>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QtEvents>
#include <QSettings>
#include <QMimeData>

#include "core/iconloader.h"
#include "core/logging.h"
#include "core/settings.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "utilities/coveroptions.h"
#include "tagreader/tagreaderclient.h"
#include "widgets/busyindicator.h"
#include "widgets/lineedit.h"
#include "collection/collectionbackend.h"
#include "playlist/playlist.h"
#include "playlist/playlistdelegates.h"
#ifdef HAVE_MUSICBRAINZ
#  include "musicbrainz/tagfetcher.h"
#  include "trackselectiondialog.h"
#endif
#include "lyrics/lyricsfetcher.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/coverproviders.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverimageresult.h"
#include "edittagdialog.h"
#include "ui_edittagdialog.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "EditTagDialog";
constexpr int kSmallImageSize = 128;
}  // namespace

const char EditTagDialog::kTagsDifferentHintText[] = QT_TR_NOOP("(different across multiple songs)");
const char EditTagDialog::kArtDifferentHintText[] = QT_TR_NOOP("Different art across multiple songs.");

EditTagDialog::EditTagDialog(const SharedPtr<NetworkAccessManager> network,
                             const SharedPtr<TagReaderClient> tagreader_client,
                             const SharedPtr<CollectionBackend> collection_backend,
                             const SharedPtr<AlbumCoverLoader> albumcover_loader,
                             const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                             const SharedPtr<CoverProviders> cover_providers,
                             const SharedPtr<LyricsProviders> lyrics_providers,
                             const SharedPtr<StreamingServices> streaming_services,
                             QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_EditTagDialog),
      tagreader_client_(tagreader_client),
      collection_backend_(collection_backend),
      albumcover_loader_(albumcover_loader),
      current_albumcover_loader_(current_albumcover_loader),
      cover_providers_(cover_providers),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
#ifdef HAVE_MUSICBRAINZ
      tag_fetcher_(new TagFetcher(network, this)),
      results_dialog_(new TrackSelectionDialog(tagreader_client, this)),
#endif
      lyrics_fetcher_(new LyricsFetcher(lyrics_providers, this)),
      cover_menu_(new QMenu(this)),
      image_no_cover_thumbnail_(ImageUtils::GenerateNoCoverImage(QSize(128, 128), devicePixelRatioF())),
      loading_(false),
      ignore_edits_(false),
      summary_cover_art_id_(0),
      tags_cover_art_id_(0),
      cover_art_is_set_(false),
      save_tag_pending_(0),
      lyrics_id_(-1) {

  QObject::connect(&*albumcover_loader_, &AlbumCoverLoader::AlbumCoverLoaded, this, &EditTagDialog::AlbumCoverLoaded);

#ifdef HAVE_MUSICBRAINZ
  QObject::connect(tag_fetcher_, &TagFetcher::ResultAvailable, results_dialog_, &TrackSelectionDialog::FetchTagFinished, Qt::QueuedConnection);
  QObject::connect(tag_fetcher_, &TagFetcher::Progress, results_dialog_, &TrackSelectionDialog::FetchTagProgress);
  QObject::connect(results_dialog_, &TrackSelectionDialog::SongChosen, this, &EditTagDialog::FetchTagSongChosen);
  QObject::connect(results_dialog_, &TrackSelectionDialog::finished, tag_fetcher_, &TagFetcher::Cancel);
#endif
  QObject::connect(lyrics_fetcher_, &LyricsFetcher::LyricsFetched, this, &EditTagDialog::UpdateLyrics);

  album_cover_choice_controller_->Init(network, tagreader_client, collection_backend, albumcover_loader, current_albumcover_loader, cover_providers, streaming_services);

  ui_->setupUi(this);
  ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);
  ui_->loading_label->hide();
  ui_->label_lyrics->hide();

  ui_->fetch_tag->setIcon(QPixmap::fromImage(QImage(u":/pictures/musicbrainz.png"_s)));
#ifdef HAVE_MUSICBRAINZ
  ui_->fetch_tag->setEnabled(true);
#else
  ui_->fetch_tag->setEnabled(false);
#endif

  // An editable field is one that has a label as a buddy.
  // The label is important because it gets turned bold when the field is changed.
  QList<QLabel*> labels = findChildren<QLabel*>();
  for (QLabel *label : std::as_const(labels)) {
    QWidget *widget = label->buddy();
    if (widget) {
      // Store information about the field
      fields_ << FieldData(label, widget, widget->objectName());  // clazy:exclude=reserve-candidates

      // Connect the edited signal
      if (LineEdit *lineedit = qobject_cast<LineEdit*>(widget)) {
        QObject::connect(lineedit, &LineEdit::textChanged, this, &EditTagDialog::FieldValueEdited);
        QObject::connect(lineedit, &LineEdit::Reset, this, &EditTagDialog::ResetField);
      }
      else if (TextEdit *textedit = qobject_cast<TextEdit*>(widget)) {
        QObject::connect(textedit, &TextEdit::textChanged, this, &EditTagDialog::FieldValueEdited);
        QObject::connect(textedit, &TextEdit::Reset, this, &EditTagDialog::ResetField);
      }
      else if (SpinBox *spinbox = qobject_cast<SpinBox*>(widget)) {
        QObject::connect(spinbox, QOverload<int>::of(&SpinBox::valueChanged), this, &EditTagDialog::FieldValueEdited);
        QObject::connect(spinbox, &SpinBox::Reset, this, &EditTagDialog::ResetField);
      }
      else if (CheckBox *checkbox = qobject_cast<CheckBox*>(widget)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(checkbox, &QCheckBox::checkStateChanged, this, &EditTagDialog::FieldValueEdited);
#else
        QObject::connect(checkbox, &QCheckBox::stateChanged, this, &EditTagDialog::FieldValueEdited);
#endif
        QObject::connect(checkbox, &CheckBox::Reset, this, &EditTagDialog::ResetField);
      }
      else if (RatingBox *ratingbox = qobject_cast<RatingBox*>(widget)) {
        QObject::connect(ratingbox, &RatingWidget::RatingChanged, this, &EditTagDialog::FieldValueEdited);
        QObject::connect(ratingbox, &RatingBox::Reset, this, &EditTagDialog::ResetField);
      }
    }
  }

  // Set the colour of all the labels on the summary page
  const bool light = palette().color(QPalette::Base).value() > 128;
  const QColor color = palette().color(QPalette::WindowText);
  QPalette summary_label_palette(palette());
  summary_label_palette.setColor(QPalette::WindowText, light ? color.lighter(150) : color.darker(150));

  labels = ui_->tab_summary->findChildren<QLabel*>();
  for (QLabel *label : std::as_const(labels)) {
    if (label->property("field_label").toBool()) {
      label->setPalette(summary_label_palette);
    }
  }

  QObject::connect(ui_->song_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &EditTagDialog::SelectionChanged);
  QObject::connect(ui_->button_box, &QDialogButtonBox::clicked, this, &EditTagDialog::ButtonClicked);
  QObject::connect(ui_->playcount_reset, &QPushButton::clicked, this, &EditTagDialog::ResetPlayStatistics);
  QObject::connect(ui_->rating, &RatingWidget::RatingChanged, this, &EditTagDialog::SongRated);
#ifdef HAVE_MUSICBRAINZ
  QObject::connect(ui_->fetch_tag, &QPushButton::clicked, this, &EditTagDialog::FetchTag);
#endif
  QObject::connect(ui_->fetch_lyrics, &QPushButton::clicked, this, &EditTagDialog::FetchLyrics);

  QList<QAction*> actions = album_cover_choice_controller_->GetAllActions();

  QObject::connect(album_cover_choice_controller_, &AlbumCoverChoiceController::Error, this, &EditTagDialog::Error);
  QObject::connect(album_cover_choice_controller_->cover_from_file_action(), &QAction::triggered, this, &EditTagDialog::LoadCoverFromFile);
  QObject::connect(album_cover_choice_controller_->cover_to_file_action(), &QAction::triggered, this, &EditTagDialog::SaveCoverToFile);
  QObject::connect(album_cover_choice_controller_->cover_from_url_action(), &QAction::triggered, this, &EditTagDialog::LoadCoverFromURL);
  QObject::connect(album_cover_choice_controller_->search_for_cover_action(), &QAction::triggered, this, &EditTagDialog::SearchForCover);
  QObject::connect(album_cover_choice_controller_->unset_cover_action(), &QAction::triggered, this, &EditTagDialog::UnsetCover);
  QObject::connect(album_cover_choice_controller_->clear_cover_action(), &QAction::triggered, this, &EditTagDialog::ClearCover);
  QObject::connect(album_cover_choice_controller_->delete_cover_action(), &QAction::triggered, this, &EditTagDialog::DeleteCover);
  QObject::connect(album_cover_choice_controller_->show_cover_action(), &QAction::triggered, this, &EditTagDialog::ShowCover);
  QObject::connect(ui_->checkbox_embedded_cover, &QCheckBox::toggled, album_cover_choice_controller_, &AlbumCoverChoiceController::set_save_embedded_cover_override);

  cover_menu_->addActions(actions);

  ui_->tags_art_button->setMenu(cover_menu_);

  ui_->tags_art->installEventFilter(this);
  ui_->tags_art->setAcceptDrops(true);

  ui_->summary_art->installEventFilter(this);

  // Add the next/previous buttons
  previous_button_ = new QPushButton(IconLoader::Load(u"go-previous"_s), tr("Previous"), this);
  next_button_ = new QPushButton(IconLoader::Load(u"go-next"_s), tr("Next"), this);
  ui_->button_box->addButton(previous_button_, QDialogButtonBox::ResetRole);
  ui_->button_box->addButton(next_button_, QDialogButtonBox::ResetRole);

  QObject::connect(previous_button_, &QPushButton::clicked, this, &EditTagDialog::PreviousSong);
  QObject::connect(next_button_, &QPushButton::clicked, this, &EditTagDialog::NextSong);

  // Set some shortcuts for the buttons
  new QShortcut(QKeySequence::Back, previous_button_, SLOT(click()));
  new QShortcut(QKeySequence::Forward, next_button_, SLOT(click()));
  new QShortcut(QKeySequence::MoveToPreviousPage, previous_button_, SLOT(click()));
  new QShortcut(QKeySequence::MoveToNextPage, next_button_, SLOT(click()));

  // Show the shortcuts as tooltips
  previous_button_->setToolTip(QStringLiteral("%1 (%2 / %3)").arg(
      previous_button_->text(),
      QKeySequence(QKeySequence::Back).toString(QKeySequence::NativeText),
      QKeySequence(QKeySequence::MoveToPreviousPage).toString(QKeySequence::NativeText)));
  next_button_->setToolTip(QStringLiteral("%1 (%2 / %3)").arg(
      next_button_->text(),
      QKeySequence(QKeySequence::Forward).toString(QKeySequence::NativeText),
      QKeySequence(QKeySequence::MoveToNextPage).toString(QKeySequence::NativeText)));

  new TagCompleter(collection_backend, Playlist::Column::Artist, ui_->artist);
  new TagCompleter(collection_backend, Playlist::Column::ArtistSort, ui_->artistsort);
  new TagCompleter(collection_backend, Playlist::Column::Album, ui_->album);
  new TagCompleter(collection_backend, Playlist::Column::AlbumSort, ui_->albumsort);
  new TagCompleter(collection_backend, Playlist::Column::AlbumArtist, ui_->albumartist);
  new TagCompleter(collection_backend, Playlist::Column::AlbumArtistSort, ui_->albumartistsort);
  new TagCompleter(collection_backend, Playlist::Column::Genre, ui_->genre);
  new TagCompleter(collection_backend, Playlist::Column::Composer, ui_->composer);
  new TagCompleter(collection_backend, Playlist::Column::ComposerSort, ui_->composersort);
  new TagCompleter(collection_backend, Playlist::Column::Performer, ui_->performer);
  new TagCompleter(collection_backend, Playlist::Column::PerformerSort, ui_->performersort);
  new TagCompleter(collection_backend, Playlist::Column::Grouping, ui_->grouping);
  new TagCompleter(collection_backend, Playlist::Column::TitleSort, ui_->titlesort);

}

EditTagDialog::~EditTagDialog() {
  delete ui_;
}

void EditTagDialog::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {

    // Set the dialog's height to the smallest possible
    resize(width(), sizeHint().height());

    // Restore the tab that was current last time.
    Settings s;
    s.beginGroup(kSettingsGroup);
    if (s.contains("geometry")) {
      restoreGeometry(s.value("geometry").toByteArray());
    }
    ui_->tab_widget->setCurrentIndex(s.value("current_tab").toInt());
    s.endGroup();

    album_cover_choice_controller_->ReloadSettings();

    cover_types_ = AlbumCoverLoaderOptions::LoadTypes();

  }

  QDialog::showEvent(e);

}

void EditTagDialog::hideEvent(QHideEvent *e) {

  // Save the current tab
  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("geometry", saveGeometry());
  s.setValue("current_tab", ui_->tab_widget->currentIndex());
  s.endGroup();

  QDialog::hideEvent(e);

}

void EditTagDialog::accept() {

  // Show the loading indicator
  if (!SetLoading(tr("Saving tracks") + u"..."_s)) return;

  SaveData();

}

bool EditTagDialog::eventFilter(QObject *o, QEvent *e) {

  if (o == ui_->tags_art) {
    switch (e->type()) {
      case QEvent::MouseButtonRelease:{
        QMouseEvent *mouse_event = static_cast<QMouseEvent*>(e);
        if (mouse_event && mouse_event->button() == Qt::RightButton) {
          cover_menu_->popup(mouse_event->globalPosition().toPoint());
        }
        break;
      }

      case QEvent::MouseButtonDblClick:
        ShowCover();
        break;

      case QEvent::DragEnter:{
        QDragEnterEvent *event = static_cast<QDragEnterEvent*>(e);
        if (AlbumCoverChoiceController::CanAcceptDrag(event)) {
          event->acceptProposedAction();
        }
        break;
      }

      case QEvent::Drop:{
        const QDropEvent *event = static_cast<QDropEvent*>(e);
        if (event->mimeData()->hasImage()) {
          QImage image = qvariant_cast<QImage>(event->mimeData()->imageData());
          if (!image.isNull()) {
            UpdateCover(UpdateCoverAction::New, AlbumCoverImageResult(image));
          }
        }
        break;
      }

      default:
        break;
    }
  }
  if (o == ui_->summary_art) {
    switch (e->type()) {
      case QEvent::MouseButtonDblClick:
        ShowCover();
        break;
      default:
        break;
    }
  }

  return QDialog::eventFilter(o, e);

}

bool EditTagDialog::SetLoading(const QString &message) {

  const bool loading = !message.isEmpty();
  if (loading == loading_) return false;
  loading_ = loading;

  ui_->button_box->setEnabled(!loading);
  ui_->tab_widget->setEnabled(!loading);
  ui_->song_list->setEnabled(!loading);
#ifdef HAVE_MUSICBRAINZ
  ui_->fetch_tag->setEnabled(!loading);
#endif
  ui_->loading_label->setVisible(loading);
  ui_->loading_label->set_text(message);

  return true;

}

QList<EditTagDialog::Data> EditTagDialog::LoadData(const SongList &songs) const {

  QList<Data> ret;

  for (const Song &song : songs) {
    if (song.IsEditable()) {
      // Try reloading the tags from file
      Song copy(song);
      const TagReaderResult result = tagreader_client_->ReadFileBlocking(copy.url().toLocalFile(), &copy);
      if (result.success() && copy.is_valid()) {
        copy.MergeUserSetData(song, false, false);
        ret << Data(copy);
      }
    }
  }

  return ret;

}

void EditTagDialog::SetSongs(const SongList &s, const PlaylistItemPtrList &items) {

  // Show the loading indicator
  if (!SetLoading(tr("Loading tracks") + u"..."_s)) return;

  data_.clear();
  playlist_items_ = items;
  ui_->song_list->clear();
  collection_songs_.clear();

  // Reload tags in the background
  QFuture<QList<Data>> future = QtConcurrent::run(&EditTagDialog::LoadData, this, s);
  QFutureWatcher<QList<Data>> *watcher = new QFutureWatcher<QList<Data>>();
  QObject::connect(watcher, &QFutureWatcher<QList<Data>>::finished, this, &EditTagDialog::SetSongsFinished);
  watcher->setFuture(future);

}

void EditTagDialog::SetSongsFinished() {

  QFutureWatcher<QList<Data>> *watcher = static_cast<QFutureWatcher<QList<Data>>*>(sender());
  QList<Data> result_data = watcher->result();
  watcher->deleteLater();

  if (!SetLoading(QString())) return;

  data_ = result_data;

  if (data_.count() == 0) {
    // If there were no valid songs, disable everything
    ui_->song_list->setEnabled(false);
    ui_->tab_widget->setEnabled(false);

    // Show a summary with empty information
    UpdateSummaryTab(Song());
    ui_->tab_widget->setCurrentWidget(ui_->tab_summary);

    SetSongListVisibility(false);
    return;
  }

  // Add the filenames to the list
  for (const Data &tag_data : std::as_const(data_)) {
    ui_->song_list->addItem(tag_data.current_.basefilename());
  }

  // Select all
  ui_->song_list->setCurrentRow(0);
  ui_->song_list->selectAll();

  // Hide the list if there's only one song in it
  SetSongListVisibility(data_.count() != 1);

}

void EditTagDialog::SetSongListVisibility(bool visible) {

  ui_->song_list->setVisible(visible);
  previous_button_->setEnabled(visible);
  next_button_->setEnabled(visible);

}

QVariant EditTagDialog::Data::value(const Song &song, const QString &id) {

  if (id == "title"_L1) return song.title();
  if (id == "titlesort"_L1) return song.titlesort();
  if (id == "artist"_L1) return song.artist();
  if (id == "artistsort"_L1) return song.artistsort();
  if (id == "album"_L1) return song.album();
  if (id == "albumsort"_L1) return song.albumsort();
  if (id == "albumartist"_L1) return song.albumartist();
  if (id == "albumartistsort"_L1) return song.albumartistsort();
  if (id == "composer"_L1) return song.composer();
  if (id == "composersort"_L1) return song.composersort();
  if (id == "performer"_L1) return song.performer();
  if (id == "performersort"_L1) return song.performersort();
  if (id == "grouping"_L1) return song.grouping();
  if (id == "genre"_L1) return song.genre();
  if (id == "comment"_L1) return song.comment();
  if (id == "lyrics"_L1) return song.lyrics();
  if (id == "track"_L1) return song.track();
  if (id == "disc"_L1) return song.disc();
  if (id == "year"_L1) return song.year();
  if (id == "compilation"_L1) return song.compilation();
  if (id == "rating"_L1) { return song.rating(); }
  qLog(Warning) << "Unknown ID" << id;
  return QVariant();

}

void EditTagDialog::Data::set_value(const QString &id, const QVariant &value) {

  if (id == "title"_L1) current_.set_title(value.toString());
  else if (id == "titlesort"_L1) current_.set_titlesort(value.toString());
  else if (id == "artist"_L1) current_.set_artist(value.toString());
  else if (id == "artistsort"_L1) current_.set_artistsort(value.toString());
  else if (id == "album"_L1) current_.set_album(value.toString());
  else if (id == "albumsort"_L1) current_.set_albumsort(value.toString());
  else if (id == "albumartist"_L1) current_.set_albumartist(value.toString());
  else if (id == "albumartistsort"_L1) current_.set_albumartistsort(value.toString());
  else if (id == "composer"_L1) current_.set_composer(value.toString());
  else if (id == "composersort"_L1) current_.set_composersort(value.toString());
  else if (id == "performer"_L1) current_.set_performer(value.toString());
  else if (id == "performersort"_L1) current_.set_performersort(value.toString());
  else if (id == "grouping"_L1) current_.set_grouping(value.toString());
  else if (id == "genre"_L1) current_.set_genre(value.toString());
  else if (id == "comment"_L1) current_.set_comment(value.toString());
  else if (id == "lyrics"_L1) current_.set_lyrics(value.toString());
  else if (id == "track"_L1) current_.set_track(value.toInt());
  else if (id == "disc"_L1) current_.set_disc(value.toInt());
  else if (id == "year"_L1) current_.set_year(value.toInt());
  else if (id == "compilation"_L1) current_.set_compilation(value.toBool());
  else if (id == "rating"_L1) { current_.set_rating(value.toFloat()); }
  else qLog(Warning) << "Unknown ID" << id;

}

bool EditTagDialog::DoesValueVary(const QModelIndexList &sel, const QString &id) const {

  QVariant value = data_[sel.first().row()].current_value(id);
  for (int i = 1; i < sel.count(); ++i) {
    if (value != data_[sel[i].row()].current_value(id)) return true;
  }
  return false;

}

bool EditTagDialog::IsValueModified(const QModelIndexList &sel, const QString &id) const {

  if (id == u"track"_s || id == u"disc"_s || id == u"year"_s) {
    return std::any_of(sel.begin(), sel.end(), [this, id](const QModelIndex &i) {
      const int original = data_[i.row()].original_value(id).toInt();
      const int current = data_[i.row()].current_value(id).toInt();
      return original != current && (original != -1 || current != 0);
    });
  }
  else if (id == u"rating"_s) {
    return std::any_of(sel.begin(), sel.end(), [this, id](const QModelIndex &i) {
      const float original = data_[i.row()].original_value(id).toFloat();
      const float current = data_[i.row()].current_value(id).toFloat();
      return original != current && (original != -1 || current != 0);
    });
  }
  return std::any_of(sel.begin(), sel.end(), [this, id](const QModelIndex &i) { return data_[i.row()].original_value(id) != data_[i.row()].current_value(id); });

}

void EditTagDialog::InitFieldValue(const FieldData &field, const QModelIndexList &sel) {

  const bool varies = DoesValueVary(sel, field.id_);

  if (ExtendedEditor *editor = dynamic_cast<ExtendedEditor*>(field.editor_)) {
    editor->clear();
    editor->clear_hint();
    if (varies) {
      editor->set_hint(tr(kTagsDifferentHintText));
      editor->set_partially();
    }
    else {
      editor->set_value(data_.value(sel.value(0).row()).current_value(field.id_));
    }
  }
  else if (field.editor_) {
    qLog(Error) << "Missing editor for" << field.editor_->objectName();
  }

  UpdateModifiedField(field, sel);

}

void EditTagDialog::UpdateFieldValue(const FieldData &field, const QModelIndexList &sel) {

  // Get the value from the field
  QVariant value;

  if (ExtendedEditor *editor = dynamic_cast<ExtendedEditor*>(field.editor_)) {
    value = editor->value();
  }
  else if (field.editor_) {
    qLog(Error) << "Missing editor for" << field.editor_->objectName();
  }

  // Did we get it?
  if (!value.isValid()) {
    return;
  }

  // Set it in each selected song
  for (const QModelIndex &i : sel) {
    data_[i.row()].set_value(field.id_, value);
  }

  UpdateModifiedField(field, sel);

}

void EditTagDialog::UpdateModifiedField(const FieldData &field, const QModelIndexList &sel) {

  const bool modified = IsValueModified(sel, field.id_);

  // Update the boldness
  QFont new_font(font());
  new_font.setBold(modified);
  field.label_->setFont(new_font);
  if (field.editor_) {
    if (ExtendedEditor *editor = dynamic_cast<ExtendedEditor*>(field.editor_)) {
      editor->set_font(new_font);
      editor->set_reset_button(modified);
    }
    else {
      field.editor_->setFont(new_font);
    }
  }

}

void EditTagDialog::ResetFieldValue(const FieldData &field, const QModelIndexList &sel) {

  // Reset each selected song
  for (const QModelIndex &i : sel) {
    Data tag_data = data_.value(i.row());
    tag_data.set_value(field.id_, tag_data.original_value(field.id_));
    data_[i.row()] = tag_data;
  }

  // Reset the field
  InitFieldValue(field, sel);

}

void EditTagDialog::SelectionChanged() {

  const QModelIndexList indexes = ui_->song_list->selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  // Set the editable fields
  UpdateUI(indexes);

  lyrics_id_ = -1;

  // If we're editing multiple songs then we have to disable certain tabs
  const bool multiple = indexes.count() > 1;
  ui_->tab_widget->setTabEnabled(ui_->tab_widget->indexOf(ui_->tab_summary), !multiple);
  ui_->tab_widget->setTabEnabled(ui_->tab_widget->indexOf(ui_->tab_lyrics), !multiple);

  if (multiple) {
    UpdateSummaryTab(Song());
    UpdateStatisticsTab(Song());
  }
  else {
    UpdateSummaryTab(data_[indexes.first().row()].original_);
    UpdateStatisticsTab(data_[indexes.first().row()].original_);
  }

  const Song first_song = data_.value(indexes.first().row()).original_;
  const UpdateCoverAction first_cover_action = data_.value(indexes.first().row()).cover_action_;
  bool art_different = false;
  bool action_different = false;
  bool albumartist_enabled = false;
  bool albumartistsort_enabled = false;
  bool composer_enabled = false;
  bool composersort_enabled = false;
  bool performer_enabled = false;
  bool performersort_enabled = false;
  bool grouping_enabled = false;
  bool genre_enabled = false;
  bool compilation_enabled = false;
  bool rating_enabled = false;
  bool comment_enabled = false;
  bool lyrics_enabled = false;
  bool titlesort_enabled = false;
  bool artistsort_enabled = false;
  bool albumsort_enabled = false;
  for (const QModelIndex &idx : indexes) {
    if (data_.value(idx.row()).cover_action_ == UpdateCoverAction::None) {
      data_[idx.row()].cover_result_ = AlbumCoverImageResult();
    }
    const Song song = data_.value(idx.row()).original_;
    if (data_.value(idx.row()).cover_action_ != first_cover_action || (first_cover_action != UpdateCoverAction::None && data_[idx.row()].cover_result_.image_data != data_[indexes.first().row()].cover_result_.image_data)) {
      action_different = true;
    }
    if (data_.value(idx.row()).cover_action_ != first_cover_action ||
        song.art_manual() != first_song.art_manual() ||
        song.art_embedded() != first_song.art_embedded() ||
        song.art_automatic() != first_song.art_automatic() ||
        (song.art_embedded() && first_song.art_embedded() && (first_song.effective_albumartist() != song.effective_albumartist() || first_song.album() != song.album()))
    ) {
      art_different = true;
    }
    if (song.albumartist_supported()) {
      albumartist_enabled = true;
    }
    if (song.albumartistsort_supported()) {
      albumartistsort_enabled = true;
    }
    if (song.composer_supported()) {
      composer_enabled = true;
    }
    if (song.composersort_supported()) {
      composersort_enabled = true;
    }
    if (song.performer_supported()) {
      performer_enabled = true;
    }
    if (song.performersort_supported()) {
      performersort_enabled = true;
    }
    if (song.grouping_supported()) {
      grouping_enabled = true;
    }
    if (song.genre_supported()) {
      genre_enabled = true;
    }
    if (song.compilation_supported()) {
      compilation_enabled = true;
    }
    if (song.rating_supported()) {
      rating_enabled = true;
    }
    if (song.comment_supported()) {
      comment_enabled = true;
    }
    if (song.lyrics_supported()) {
      lyrics_enabled = true;
    }
    if (song.titlesort_supported()) {
      titlesort_enabled = true;
    }
    if (song.artistsort_supported()) {
      artistsort_enabled = true;
    }
    if (song.albumsort_supported()) {
      albumsort_enabled = true;
    }
  }

  QString summary;
  if (indexes.count() == 1) {
    summary += "<p><b>"_L1 + first_song.PrettyTitleWithArtist().toHtmlEscaped() + "</b></p>"_L1;
  }
  else {
    summary += "<p><b>"_L1;
    summary += tr("%1 songs selected.").arg(indexes.count());
    summary += "</b></p>"_L1;
  }
  ui_->tags_summary->setText(summary);

  const bool enable_change_art = first_song.is_local_collection_song();
  ui_->tags_art_button->setEnabled(enable_change_art);
  if ((art_different && first_cover_action != UpdateCoverAction::New) || action_different) {
    tags_cover_art_id_ = 0;  // Cancels any pending art load.
    ui_->tags_art->clear();
    ui_->tags_art->setText(QLatin1String(kArtDifferentHintText));
    album_cover_choice_controller_->show_cover_action()->setEnabled(false);
    album_cover_choice_controller_->cover_to_file_action()->setEnabled(false);
    album_cover_choice_controller_->cover_from_file_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->cover_from_url_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->search_for_cover_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->unset_cover_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->clear_cover_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->delete_cover_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->search_for_cover_action()->setEnabled(enable_change_art);
  }
  else {
    ui_->tags_art->clear();
    album_cover_choice_controller_->show_cover_action()->setEnabled(first_song.has_valid_art() && !first_song.art_unset());
    album_cover_choice_controller_->cover_to_file_action()->setEnabled(first_song.has_valid_art() && !first_song.art_unset());
    album_cover_choice_controller_->cover_from_file_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->cover_from_url_action()->setEnabled(enable_change_art);
    album_cover_choice_controller_->search_for_cover_action()->setEnabled(cover_providers_->HasAnyProviders() && enable_change_art);
    album_cover_choice_controller_->unset_cover_action()->setEnabled(enable_change_art && !first_song.art_unset());
    album_cover_choice_controller_->clear_cover_action()->setEnabled(enable_change_art && (!first_song.art_manual().isEmpty() || first_song.art_unset()));
    album_cover_choice_controller_->delete_cover_action()->setEnabled(enable_change_art && (first_song.art_embedded() || !first_song.art_automatic().isEmpty() || !first_song.art_manual().isEmpty()));
    AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::RawImageData | AlbumCoverLoaderOptions::Option::OriginalImage | AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
    cover_options.types = cover_types_;
    cover_options.desired_scaled_size = QSize(kSmallImageSize, kSmallImageSize);
    cover_options.device_pixel_ratio = devicePixelRatioF();
    if (data_.value(indexes.first().row()).cover_action_ == UpdateCoverAction::None) {
      tags_cover_art_id_ = albumcover_loader_->LoadImageAsync(cover_options, first_song);
    }
    else {
      tags_cover_art_id_ = albumcover_loader_->LoadImageAsync(cover_options, data_[indexes.first().row()].cover_result_);
    }
  }

  const bool embedded_cover = (first_song.save_embedded_cover_supported() && (first_song.art_embedded() || album_cover_choice_controller_->get_collection_save_album_cover_type() == CoverOptions::CoverType::Embedded));
  ui_->checkbox_embedded_cover->setChecked(embedded_cover);
  album_cover_choice_controller_->set_save_embedded_cover_override(embedded_cover);

  ui_->albumartist->setEnabled(albumartist_enabled);
  ui_->albumartistsort->setEnabled(albumartistsort_enabled);
  ui_->composer->setEnabled(composer_enabled);
  ui_->composersort->setEnabled(composersort_enabled);
  ui_->performer->setEnabled(performer_enabled);
  ui_->performersort->setEnabled(performersort_enabled);
  ui_->grouping->setEnabled(grouping_enabled);
  ui_->genre->setEnabled(genre_enabled);
  ui_->compilation->setEnabled(compilation_enabled);
  ui_->rating->setEnabled(rating_enabled);
  ui_->comment->setEnabled(comment_enabled);
  ui_->lyrics->setEnabled(lyrics_enabled);
  ui_->titlesort->setEnabled(titlesort_enabled);
  ui_->artistsort->setEnabled(artistsort_enabled);
  ui_->albumsort->setEnabled(albumsort_enabled);

}

void EditTagDialog::UpdateUI(const QModelIndexList &indexes) {

  ignore_edits_ = true;
  for (const FieldData &field : std::as_const(fields_)) {
    InitFieldValue(field, indexes);
  }
  ignore_edits_ = false;

}

void EditTagDialog::SetText(QLabel *label, const int value, const QString &suffix, const QString &def) {
  label->setText(value <= 0 ? def : (QString::number(value) + QLatin1Char(' ') + suffix));
}

void EditTagDialog::SetDate(QLabel *label, const qint64 time) {

  if (time == std::numeric_limits<qint64>::max()) {  // -1
    label->setText(QObject::tr("Unknown"));
  }
  else {
    label->setText(QDateTime::fromSecsSinceEpoch(time).toString(QLocale::system().dateTimeFormat(QLocale::LongFormat)));
  }

}

void EditTagDialog::UpdateSummaryTab(const Song &song) {

  AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
  cover_options.types = cover_types_;
  cover_options.desired_scaled_size = QSize(kSmallImageSize, kSmallImageSize);
  cover_options.device_pixel_ratio = devicePixelRatioF();
  summary_cover_art_id_ = albumcover_loader_->LoadImageAsync(cover_options, song);

  ui_->summary->setText(u"<p><b>"_s + song.PrettyTitleWithArtist().toHtmlEscaped() + u"</b></p>"_s);

  ui_->length->setText(Utilities::PrettyTimeNanosec(song.length_nanosec()));

  SetText(ui_->samplerate, song.samplerate(), u"Hz"_s);
  SetText(ui_->bitdepth, song.bitdepth(), u"Bit"_s);
  SetText(ui_->bitrate, song.bitrate(), tr("kbps"));
  ui_->ebur128_integrated_loudness->setText(song.Ebur128LoudnessLUFSToText());
  ui_->ebur128_loudness_range->setText(song.Ebur128LoudnessRangeLUToText());
  SetDate(ui_->mtime, song.mtime());
  SetDate(ui_->ctime, song.ctime());

  if (song.filesize() == -1) {
    ui_->filesize->setText(tr("Unknown"));
  }
  else {
    ui_->filesize->setText(Utilities::PrettySize(static_cast<quint64>(song.filesize())));
  }

  ui_->filetype->setText(song.TextForFiletype());

  if (song.url().isLocalFile()) {
    ui_->filename->setText(song.url().fileName());
    ui_->path->setText(QFileInfo(QDir::toNativeSeparators(song.url().toLocalFile())).path());
  }
  else {
    ui_->filename->setText(song.url().toString());
    ui_->path->clear();
  }

  ui_->art_embedded->setText(song.art_embedded() ? tr("Yes") : tr("No"));

  if (song.art_manual().isEmpty()) {
    ui_->art_manual->setText(tr("None"));
  }
  else {
    ui_->art_manual->setText(song.art_manual().toString());
  }

  if (song.art_automatic().isEmpty()) {
    ui_->art_automatic->setText(tr("None"));
  }
  else {
    ui_->art_automatic->setText(song.art_automatic().toString());
  }

  ui_->art_unset->setText(song.art_unset() ? tr("Yes") : tr("No"));

}

QString EditTagDialog::GetArtSummary(const Song &song, const AlbumCoverLoaderResult::Type cover_type) {

  QString summary;

  switch (cover_type) {
    case AlbumCoverLoaderResult::Type::None:
      break;
    case AlbumCoverLoaderResult::Type::Unset:
      summary = tr("Cover is unset.").toHtmlEscaped();
      break;
    case AlbumCoverLoaderResult::Type::Embedded:
      summary = tr("Cover from embedded image.");
      break;
    case AlbumCoverLoaderResult::Type::Manual:
      summary = tr("Cover from %1").arg(song.art_manual().toString()).toHtmlEscaped();
      break;
    case AlbumCoverLoaderResult::Type::Automatic:
      summary = tr("Cover from %1").arg(song.art_automatic().toString()).toHtmlEscaped();
      break;
  }

  if (summary.isEmpty()) {
    summary = tr("Cover art not set").toHtmlEscaped();
  }

  if (!song.is_local_collection_song()) {
    if (!summary.isEmpty()) summary += "<br />"_L1;
    summary = tr("Album cover editing is only available for collection songs.");
  }

  return summary;

}

QString EditTagDialog::GetArtSummary(const UpdateCoverAction cover_action) {

  switch (cover_action) {
    case UpdateCoverAction::Clear:
      return tr("Cover changed: Will be cleared when saved.").toHtmlEscaped();
    case UpdateCoverAction::Unset:
      return tr("Cover changed: Will be unset when saved.").toHtmlEscaped();
    case UpdateCoverAction::Delete:
      return tr("Cover changed: Will be deleted when saved.").toHtmlEscaped();
    case UpdateCoverAction::New:
      return tr("Cover changed: Will set new when saved.").toHtmlEscaped();
    case UpdateCoverAction::None:
      break;
  }

  return QString();

}

void EditTagDialog::UpdateStatisticsTab(const Song &song) {

  ui_->playcount->setText(QString::number(song.playcount()));
  ui_->skipcount->setText(QString::number(song.skipcount()));
  ui_->lastplayed->setText(song.lastplayed() <= 0 ? tr("Never") : QDateTime::fromSecsSinceEpoch(song.lastplayed()).toString(QLocale::system().dateTimeFormat(QLocale::LongFormat)));

}

void EditTagDialog::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (id == summary_cover_art_id_) {
    if (result.success && !result.image_scaled.isNull() && result.type != AlbumCoverLoaderResult::Type::Unset) {
      ui_->summary_art->setPixmap(QPixmap::fromImage(result.image_scaled));
    }
    else {
      ui_->summary_art->setPixmap(QPixmap::fromImage(image_no_cover_thumbnail_));
    }
    if (ui_->song_list->selectionModel()->selectedIndexes().count() > 0) {
      const QModelIndex idx = ui_->song_list->selectionModel()->selectedIndexes().first();
      QString summary = ui_->summary->toPlainText();
      summary += "<br />"_L1;
      summary += "<br />"_L1;
      summary += GetArtSummary(data_[idx.row()].current_, result.type);
      ui_->summary->setText(summary);
    }
    summary_cover_art_id_ = 0;
  }
  else if (id == tags_cover_art_id_) {
    if (result.success && !result.image_scaled.isNull() && result.type != AlbumCoverLoaderResult::Type::Unset) {
      ui_->tags_art->setPixmap(QPixmap::fromImage(result.image_scaled));
    }
    else {
      ui_->tags_art->setPixmap(QPixmap::fromImage(image_no_cover_thumbnail_));
    }
    Song first_song;
    UpdateCoverAction cover_action = UpdateCoverAction::None;
    const QModelIndexList indexes = ui_->song_list->selectionModel()->selectedIndexes();
    for (const QModelIndex &idx : indexes) {
      data_[idx.row()].cover_result_ = result.album_cover;
      if (!first_song.is_valid()) {
        first_song = data_.value(idx.row()).current_;
        cover_action = data_.value(idx.row()).cover_action_;
      }
    }
    bool enable_change_art = false;
    if (first_song.is_valid()) {
      QString summary = ui_->tags_summary->toPlainText();
      summary += "<br />"_L1;
      summary += "<br />"_L1;
      if (cover_action == UpdateCoverAction::None) {
        summary += GetArtSummary(first_song, result.type);
      }
      else {
        summary += GetArtSummary(cover_action);
      }
      ui_->tags_summary->setText(summary);
      enable_change_art = first_song.is_local_collection_song() && !first_song.effective_albumartist().isEmpty() && !first_song.album().isEmpty();
    }
    tags_cover_art_id_ = 0;
    album_cover_choice_controller_->show_cover_action()->setEnabled(result.success && result.type != AlbumCoverLoaderResult::Type::Unset);
    album_cover_choice_controller_->cover_to_file_action()->setEnabled(result.success && result.type != AlbumCoverLoaderResult::Type::Unset);
    album_cover_choice_controller_->delete_cover_action()->setEnabled(enable_change_art && result.success && result.type != AlbumCoverLoaderResult::Type::Unset);
  }

}

void EditTagDialog::FieldValueEdited() {

  if (ignore_edits_) return;

  const QModelIndexList sel = ui_->song_list->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) {
    return;
  }

  QWidget *w = qobject_cast<QWidget*>(sender());

  // Find the field
  for (const FieldData &field : std::as_const(fields_)) {
    if (field.editor_ == w) {
      UpdateFieldValue(field, sel);
      return;
    }
  }

}

void EditTagDialog::ResetField() {

  const QModelIndexList sel = ui_->song_list->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) {
    return;
  }

  QWidget *w = qobject_cast<QWidget*>(sender());

  // Find the field
  for (const FieldData &field : std::as_const(fields_)) {
    if (field.editor_ == w) {
      ignore_edits_ = true;
      ResetFieldValue(field, sel);
      ignore_edits_ = false;
      return;
    }
  }

}

Song *EditTagDialog::GetFirstSelected() {

  const QModelIndexList sel = ui_->song_list->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) return nullptr;
  return &data_[sel.first().row()].current_;

}

void EditTagDialog::LoadCoverFromFile() {

  Song *song = GetFirstSelected();
  if (!song) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromFile(song);
  if (result.is_valid()) UpdateCover(UpdateCoverAction::New, result);

}

void EditTagDialog::SaveCoverToFile() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;

  const Data first_data = data_.value(ui_->song_list->selectionModel()->selectedIndexes().first().row());
  album_cover_choice_controller_->SaveCoverToFileManual(first_data.current_, first_data.cover_result_);

}

void EditTagDialog::LoadCoverFromURL() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromURL();
  if (result.is_valid()) UpdateCover(UpdateCoverAction::New, result);

}

void EditTagDialog::SearchForCover() {

  Song *song = GetFirstSelected();
  if (!song) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->SearchForImage(song);
  if (result.is_valid()) UpdateCover(UpdateCoverAction::New, result);

}

void EditTagDialog::UnsetCover() {

  Song *song = GetFirstSelected();
  if (!song) return;

  song->set_art_embedded(false);
  song->clear_art_automatic();
  song->clear_art_manual();
  song->set_art_unset(true);

  UpdateCover(UpdateCoverAction::Unset);

}

void EditTagDialog::ClearCover() {

  Song *song = GetFirstSelected();
  if (!song) return;

  song->set_art_embedded(false);
  song->clear_art_automatic();
  song->clear_art_manual();
  song->set_art_unset(false);

  UpdateCover(UpdateCoverAction::Clear);

}

void EditTagDialog::DeleteCover() {

  UpdateCover(UpdateCoverAction::Delete);

}

void EditTagDialog::ShowCover() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;
  const Data first_data = data_.value(ui_->song_list->selectionModel()->selectedIndexes().first().row());
  album_cover_choice_controller_->ShowCover(first_data.current_, first_data.cover_result_.image);

}

void EditTagDialog::UpdateCover(const UpdateCoverAction cover_action, const AlbumCoverImageResult &cover_result) {

  const QModelIndexList indexes = ui_->song_list->selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  QString artist = data_.value(indexes.first().row()).current_.effective_albumartist();
  QString album = data_.value(indexes.first().row()).current_.album();

  for (const QModelIndex &idx : indexes) {
    data_[idx.row()].cover_action_ = cover_action;
    data_[idx.row()].cover_result_ = cover_result;
    if (cover_action == UpdateCoverAction::New) {
      data_[idx.row()].current_.clear_art_manual();
      data_[idx.row()].current_.set_art_unset(false);
    }
    else if (cover_action == UpdateCoverAction::Unset) {
      data_[idx.row()].current_.set_art_unset(true);
    }
    else if (cover_action == UpdateCoverAction::Clear || cover_action == UpdateCoverAction::Delete) {
      data_[idx.row()].current_.set_art_embedded(false);
      data_[idx.row()].current_.clear_art_manual();
      data_[idx.row()].current_.clear_art_automatic();
      data_[idx.row()].current_.set_art_unset(false);
    }
    if (artist != data_[idx.row()].current_.effective_albumartist() || album != data_[idx.row()].current_.effective_albumartist()) {
      artist.clear();
      album.clear();
    }
  }

  // Now check if we have any other songs cached that share that artist and album (and would therefore be changed as well)
  if (!artist.isEmpty() && !album.isEmpty()) {
    for (int i = 0; i < data_.count(); ++i) {
      if (data_[i].current_.effective_albumartist() == artist && data_[i].current_.album() == album) {
        data_[i].cover_action_ = cover_action;
        data_[i].cover_result_ = cover_result;
        if (cover_action == UpdateCoverAction::New) {
          data_[i].current_.clear_art_manual();
          data_[i].current_.set_art_unset(false);
        }
        else if (cover_action == UpdateCoverAction::Unset) {
          data_[i].current_.set_art_unset(true);
        }
        else if (cover_action == UpdateCoverAction::Clear || cover_action == UpdateCoverAction::Delete) {
          data_[i].current_.set_art_embedded(false);
          data_[i].current_.clear_art_manual();
          data_[i].current_.clear_art_automatic();
          data_[i].current_.set_art_unset(false);
        }
      }
    }
  }

  UpdateSummaryTab(data_[indexes.first().row()].current_);
  SelectionChanged();

}

void EditTagDialog::NextSong() {

  if (ui_->song_list->count() == 0) {
    return;
  }

  int row = (ui_->song_list->currentRow() + 1) % ui_->song_list->count();
  ui_->song_list->setCurrentRow(row);

}

void EditTagDialog::PreviousSong() {

  if (ui_->song_list->count() == 0) {
    return;
  }

  int row = (ui_->song_list->currentRow() - 1 + ui_->song_list->count()) % ui_->song_list->count();
  ui_->song_list->setCurrentRow(row);

}

void EditTagDialog::ButtonClicked(QAbstractButton *button) {

  if (button == ui_->button_box->button(QDialogButtonBox::Discard)) {
    reject();
  }

}

void EditTagDialog::SaveData() {

  QMap<QString, QUrl> cover_urls;

  for (int i = 0; i < data_.count(); ++i) {
    Data &ref = data_[i];

    QString embedded_cover_from_file;
    // If embedded album cover is selected, and it isn't saved to the tags, then save it even if no action was done.
    if (ui_->checkbox_embedded_cover->isChecked() && ref.cover_action_ == UpdateCoverAction::None && !ref.original_.art_embedded() && ref.original_.save_embedded_cover_supported()) {
      if (ref.original_.art_manual_is_valid()) {
        ref.cover_action_ = UpdateCoverAction::New;
        embedded_cover_from_file = ref.original_.art_manual().toLocalFile();
      }
      else if (ref.original_.art_automatic_is_valid()) {
        ref.cover_action_ = UpdateCoverAction::New;
        embedded_cover_from_file = ref.original_.art_automatic().toLocalFile();
      }
    }

    const bool save_tags = !ref.current_.IsMetadataEqual(ref.original_);
    const bool save_rating = !ref.current_.IsRatingEqual(ref.original_);
    const bool save_playcount = ref.current_.playcount() == 0 && ref.current_.skipcount() == 0 && ref.current_.lastplayed() == -1 && !ref.current_.IsPlayStatisticsEqual(ref.original_);
    const bool save_embedded_cover = ref.cover_action_ != UpdateCoverAction::None && ui_->checkbox_embedded_cover->isChecked() && ref.original_.save_embedded_cover_supported();

    if (ref.cover_action_ != UpdateCoverAction::None) {
      switch (ref.cover_action_) {
        case UpdateCoverAction::None:
          break;
        case UpdateCoverAction::New:{
          if ((!ref.current_.effective_albumartist().isEmpty() && !ref.current_.album().isEmpty()) &&
              (!ui_->checkbox_embedded_cover->isChecked() || !ref.original_.save_embedded_cover_supported())) {
            QUrl cover_url;
            if (!ref.cover_result_.cover_url.isEmpty() && ref.cover_result_.cover_url.isLocalFile() && QFile::exists(ref.cover_result_.cover_url.toLocalFile())) {
              cover_url = ref.cover_result_.cover_url;
            }
            else {
              QString cover_hash = QString::fromLatin1(CoverUtils::Sha1CoverHash(ref.current_.effective_albumartist(), ref.current_.album()).toHex());
              if (cover_urls.contains(cover_hash)) {
                cover_url = cover_urls[cover_hash];
              }
              else {
                cover_url = album_cover_choice_controller_->SaveCoverToFileAutomatic(&ref.current_, ref.cover_result_);
                if (cover_url.isValid()) {
                  cover_urls.insert(cover_hash, cover_url);
                }
              }
            }
            ref.current_.set_art_manual(cover_url);
            ref.current_.set_art_unset(false);
          }
          break;
        }
        case UpdateCoverAction::Unset:
          ref.current_.set_art_embedded(false);
          ref.current_.clear_art_manual();
          ref.current_.clear_art_automatic();
          ref.current_.set_art_unset(true);
          break;
        case UpdateCoverAction::Clear:
          ref.current_.set_art_embedded(false);
          ref.current_.clear_art_manual();
          ref.current_.clear_art_automatic();
          ref.current_.set_art_unset(false);
          break;
        case UpdateCoverAction::Delete:{
          ref.current_.set_art_embedded(false);
          if (!ref.original_.art_automatic().isEmpty()) {
            if (ref.original_.art_automatic_is_valid()) {
              const QString art_automatic = ref.original_.art_automatic().toLocalFile();
              if (QFile::exists(art_automatic)) {
                QFile::remove(art_automatic);
              }
            }
            ref.current_.clear_art_automatic();
          }
          if (!ref.original_.art_manual().isEmpty()) {
            if (ref.original_.art_manual_is_valid()) {
              const QString art_manual = ref.original_.art_manual().toLocalFile();
              if (QFile::exists(art_manual)) {
                QFile::remove(art_manual);
              }
            }
            ref.current_.clear_art_manual();
          }
          ref.current_.set_art_unset(false);
          break;
        }
      }
    }

    if (save_tags || save_playcount || save_rating || save_embedded_cover) {
      // Not to confuse the collection model.
      if (ref.current_.track() <= 0) { ref.current_.set_track(-1); }
      if (ref.current_.disc() <= 0) { ref.current_.set_disc(-1); }
      if (ref.current_.year() <= 0) { ref.current_.set_year(-1); }
      if (ref.current_.originalyear() <= 0) { ref.current_.set_originalyear(-1); }
      if (ref.current_.lastplayed() <= 0) { ref.current_.set_lastplayed(-1); }
      ++save_tag_pending_;
      SaveTagCoverData save_tag_cover_data;
      if (save_embedded_cover && ref.cover_action_ == UpdateCoverAction::New) {
        if (!ref.cover_result_.image.isNull()) {
          save_tag_cover_data.cover_mimetype = ref.cover_result_.mime_type;
        }
        else if (!embedded_cover_from_file.isEmpty()) {
          save_tag_cover_data.cover_filename = embedded_cover_from_file;
        }
        save_tag_cover_data.cover_data = ref.cover_result_.image_data;
      }
      TagReaderClient::SaveOptions save_tags_options;
      if (save_tags) {
        save_tags_options |= TagReaderClient::SaveOption::Tags;
      }
      if (save_playcount) {
        save_tags_options |= TagReaderClient::SaveOption::Playcount;
      }
      if (save_rating) {
        save_tags_options |= TagReaderClient::SaveOption::Rating;
      }
      if (save_embedded_cover) {
        save_tags_options |= TagReaderClient::SaveOption::Cover;
      }
      TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(ref.current_.url().toLocalFile(), ref.current_, save_tags_options, save_tag_cover_data);
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, ref, connection]() {
        SongSaveTagsComplete(reply, ref.current_.url().toLocalFile(), ref.current_, ref.cover_action_);
        QObject::disconnect(*connection);
      }, Qt::QueuedConnection);
    }
    // If the cover was changed, but no tags written, make sure to update the collection.
    else if (ref.cover_action_ != UpdateCoverAction::None && !ref.current_.effective_albumartist().isEmpty() && !ref.current_.album().isEmpty()) {
      if (ref.current_.is_local_collection_song()) {
        collection_songs_.insert(ref.current_.id(), ref.current_);
      }
      if (ref.current_ == current_albumcover_loader_->last_song()) {
        current_albumcover_loader_->LoadAlbumCover(ref.current_);
      }
    }

  }

  if (save_tag_pending_ <= 0) SaveDataFinished();

}

void EditTagDialog::SaveDataFinished() {

  if (!collection_songs_.isEmpty()) {
    collection_backend_->AddOrUpdateSongsAsync(collection_songs_.values());
    collection_songs_.clear();
  }

  if (!SetLoading(QString())) return;

  QDialog::accept();

}

void EditTagDialog::ResetPlayStatistics() {

  const QModelIndexList idx_list = ui_->song_list->selectionModel()->selectedIndexes();
  if (idx_list.isEmpty()) return;

  Song *song = &data_[idx_list.first().row()].current_;
  if (!song->is_valid()) return;

  if (QMessageBox::question(this, tr("Reset song play statistics"), tr("Are you sure you want to reset this song's play statistics?"), QMessageBox::Reset, QMessageBox::Cancel) != QMessageBox::Reset) {
    return;
  }

  song->set_playcount(0);
  song->set_skipcount(0);
  song->set_lastplayed(-1);

  UpdateStatisticsTab(*song);

}

void EditTagDialog::SongRated(const float rating) {

  const QModelIndexList indexes = ui_->song_list->selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  for (const QModelIndex &idx : indexes) {
    if (!data_[idx.row()].current_.is_valid()) continue;
    data_[idx.row()].current_.set_rating(rating);
  }

}

void EditTagDialog::FetchTag() {

#ifdef HAVE_MUSICBRAINZ

  const QModelIndexList sel = ui_->song_list->selectionModel()->selectedIndexes();

  SongList songs;

  for (const QModelIndex &idx : sel) {
    const Song song = data_.value(idx.row()).original_;
    if (!song.is_valid()) {
      continue;
    }
    songs << song;
  }

  if (songs.isEmpty()) return;

  results_dialog_->Init(songs);
  tag_fetcher_->StartFetch(songs);

  results_dialog_->show();

#endif

}

void EditTagDialog::FetchTagSongChosen(const Song &original_song, const Song &new_metadata) {

#ifdef HAVE_MUSICBRAINZ

  const QString filename = original_song.url().toLocalFile();

  // Find the song with this filename
  auto data_it = std::find_if(data_.begin(), data_.end(), [&filename](const Data &d) {
    return d.original_.url().toLocalFile() == filename;
  });
  if (data_it == data_.end()) {
    qLog(Warning) << "Could not find song to filename: " << filename;
    return;
  }

  // Update song data
  data_it->current_.set_title(new_metadata.title());
  data_it->current_.set_artist(new_metadata.artist());
  data_it->current_.set_album(new_metadata.album());
  data_it->current_.set_track(new_metadata.track());
  data_it->current_.set_year(new_metadata.year());

  // Is it currently being displayed in the UI?
  if (ui_->song_list->currentRow() == std::distance(data_.begin(), data_it)) {
    // Yes! Additionally, update UI
    const QModelIndexList sel = ui_->song_list->selectionModel()->selectedIndexes();
    UpdateUI(sel);
  }

#else
  Q_UNUSED(original_song)
  Q_UNUSED(new_metadata)
#endif

}

void EditTagDialog::FetchLyrics() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;
  const Song song = data_.value(ui_->song_list->selectionModel()->selectedIndexes().first().row()).current_;
  lyrics_fetcher_->Clear();
  ui_->lyrics->setPlainText(tr("loading..."));
  lyrics_id_ = static_cast<qint64>(lyrics_fetcher_->Search(song.effective_albumartist(), song.artist(), song.album(), song.title()));

}

void EditTagDialog::UpdateLyrics(const quint64 id, const QString &provider, const QString &lyrics) {

  Q_UNUSED(provider);

  if (static_cast<qint64>(id) != lyrics_id_) return;
  lyrics_id_ = -1;
  if (lyrics.isEmpty()) {
    ui_->lyrics->setPlainText(tr("Not found."));
  }
  else {
    ui_->lyrics->setPlainText(lyrics);
  }

}

void EditTagDialog::SongSaveTagsComplete(TagReaderReplyPtr reply, const QString &filename, Song song, const UpdateCoverAction cover_action) {

  --save_tag_pending_;

  const bool success = reply->success();
  const QString error = reply->error();

  if (success) {
    if (song.is_local_collection_song()) {
      if (collection_songs_.contains(song.id())) {
        Song old_song = collection_songs_.take(song.id());
        song.set_art_automatic(old_song.art_automatic());
        song.set_art_manual(old_song.art_manual());
      }
      switch (cover_action) {
        case UpdateCoverAction::None:
          break;
        case UpdateCoverAction::New:
          song.clear_art_manual();
          song.set_art_embedded(true);
          break;
        case UpdateCoverAction::Clear:
        case UpdateCoverAction::Delete:
          song.set_art_embedded(false);
          break;
        case UpdateCoverAction::Unset:
          song.set_art_embedded(false);
          song.set_art_unset(true);
          break;
      }
      collection_songs_.insert(song.id(), song);
    }
    if (cover_action != UpdateCoverAction::None && song == current_albumcover_loader_->last_song()) {
      current_albumcover_loader_->LoadAlbumCover(song);
    }
  }
  else {
    if (error.isEmpty()) {
      Q_EMIT Error(tr("Could not write metadata to %1").arg(filename));
    }
    else {
      Q_EMIT Error(tr("Could not write metadata to %1: %2").arg(filename, error));
    }
  }

  if (save_tag_pending_ <= 0) SaveDataFinished();

}
