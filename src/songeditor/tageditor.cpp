/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <optional>

#include <QtGlobal>
#include <QCoreApplication>
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
#include <QComboBox>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QScrollArea>
#include <QtEvents>
#include <QSettings>
#include <QMimeData>

#include "constants/timeconstants.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/settings.h"
#include "constants/collectionsettings.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/fileutils.h"
#include "utilities/lrcutils.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "utilities/coveroptions.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagid3v2version.h"
#include "widgets/busyindicator.h"
#include "widgets/lineedit.h"
#include "collection/collectionbackend.h"
#include "playlist/playlist.h"
#include "playlist/playlistdelegates.h"
#ifdef HAVE_TAGFETCHER
#  include "tagfetcher/tagfetcher.h"
#  include "trackselectiondialog.h"
#endif
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/coverproviders.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverimageresult.h"
#include "songeditor.h"
#include "tageditor.h"
#include "ui_tageditor.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-const-variable"
#endif

namespace {
// The settings group has the name from before the class was renamed, so the saved settings are kept.
constexpr char kSettingsGroup[] = "EditTagDialog";
constexpr int kSmallImageSize = 128;

// ID3v2 version constants
constexpr int kID3v2_Version_3 = 3;
constexpr int kID3v2_Version_4 = 4;
constexpr int kComboBoxIndex_ID3v2_3 = 0;
constexpr int kComboBoxIndex_ID3v2_4 = 1;
}  // namespace

const char TagEditor::kArtDifferentHintText[] = QT_TR_NOOP("Different art across multiple songs.");

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

TagEditor::TagEditor(const SharedPtr<NetworkAccessManager> network,
                     const SharedPtr<TagReaderClient> tagreader_client,
                     const SharedPtr<CollectionBackend> collection_backend,
                     const SharedPtr<AlbumCoverLoader> albumcover_loader,
                     const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                     const SharedPtr<CoverProviders> cover_providers,
                     const SharedPtr<LyricsProviders> lyrics_providers,
                     const SharedPtr<StreamingServices> streaming_services,
                     QWidget *parent)
    : SongEditor(tagreader_client, collection_backend, lyrics_providers, parent),
      ui_(new Ui_TagEditor),
      albumcover_loader_(albumcover_loader),
      current_albumcover_loader_(current_albumcover_loader),
      cover_providers_(cover_providers),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
#ifdef HAVE_TAGFETCHER
      tag_fetcher_(new TagFetcher(network, this)),
      results_dialog_(new TrackSelectionDialog(tagreader_client, this)),
#endif
      cover_menu_(new QMenu(this)),
      image_no_cover_thumbnail_(ImageUtils::GenerateNoCoverImage(QSize(128, 128), devicePixelRatioF())),
      summary_cover_art_id_(0),
      tags_cover_art_id_(0),
      cover_art_is_set_(false) {

  QObject::connect(&*albumcover_loader_, &AlbumCoverLoader::AlbumCoverLoaded, this, &TagEditor::AlbumCoverLoaded);

#ifdef HAVE_TAGFETCHER
  QObject::connect(tag_fetcher_, &TagFetcher::ResultAvailable, results_dialog_, &TrackSelectionDialog::FetchTagFinished, Qt::QueuedConnection);
  QObject::connect(tag_fetcher_, &TagFetcher::Progress, results_dialog_, &TrackSelectionDialog::FetchTagProgress);
  QObject::connect(results_dialog_, &TrackSelectionDialog::SongChosen, this, &TagEditor::FetchTagSongChosen);
  QObject::connect(results_dialog_, &TrackSelectionDialog::finished, tag_fetcher_, &TagFetcher::Cancel);
#endif
  album_cover_choice_controller_->Init(network, tagreader_client, collection_backend, albumcover_loader, current_albumcover_loader, cover_providers, streaming_services);

  ui_->setupUi(this);
  ui_->splitter->setSizes(QList<int>() << 200 << width() - 200);

  // The lyrics tabs are added, and the fields in the tabs are found.
  InitSongEditor(ui_->song_list, ui_->tab_widget, ui_->button_box, ui_->loading_label);

  ui_->fetch_tag->setIcon(QPixmap::fromImage(QImage(u":/pictures/musicbrainz.png"_s)));
#ifdef HAVE_TAGFETCHER
  ui_->fetch_tag->setEnabled(true);
#else
  ui_->fetch_tag->setEnabled(false);
#endif

  // Set the colour of all the labels on the summary page
  const bool light = palette().color(QPalette::Base).value() > 128;
  const QColor color = palette().color(QPalette::WindowText);
  QPalette summary_label_palette(palette());
  summary_label_palette.setColor(QPalette::WindowText, light ? color.lighter(150) : color.darker(150));

  const QList<QLabel*> labels = ui_->tab_summary->findChildren<QLabel*>();
  for (QLabel *label : labels) {
    if (label->property("field_label").toBool()) {
      label->setPalette(summary_label_palette);
    }
  }

  QObject::connect(ui_->playcount_reset, &QPushButton::clicked, this, &TagEditor::ResetPlayStatistics);
  QObject::connect(ui_->rating, &RatingWidget::RatingChanged, this, &TagEditor::SongRated);
#ifdef HAVE_TAGFETCHER
  QObject::connect(ui_->fetch_tag, &QPushButton::clicked, this, &TagEditor::FetchTag);
#endif

  QList<QAction*> actions = album_cover_choice_controller_->GetAllActions();

  QObject::connect(album_cover_choice_controller_, &AlbumCoverChoiceController::Error, this, &TagEditor::Error);
  QObject::connect(album_cover_choice_controller_->cover_from_file_action(), &QAction::triggered, this, &TagEditor::LoadCoverFromFile);
  QObject::connect(album_cover_choice_controller_->cover_to_file_action(), &QAction::triggered, this, &TagEditor::SaveCoverToFile);
  QObject::connect(album_cover_choice_controller_->cover_from_url_action(), &QAction::triggered, this, &TagEditor::LoadCoverFromURL);
  QObject::connect(album_cover_choice_controller_->search_for_cover_action(), &QAction::triggered, this, &TagEditor::SearchForCover);
  QObject::connect(album_cover_choice_controller_->unset_cover_action(), &QAction::triggered, this, &TagEditor::UnsetCover);
  QObject::connect(album_cover_choice_controller_->clear_cover_action(), &QAction::triggered, this, &TagEditor::ClearCover);
  QObject::connect(album_cover_choice_controller_->delete_cover_action(), &QAction::triggered, this, &TagEditor::DeleteCover);
  QObject::connect(album_cover_choice_controller_->show_cover_action(), &QAction::triggered, this, &TagEditor::ShowCover);
  QObject::connect(ui_->checkbox_embedded_cover, &QCheckBox::toggled, album_cover_choice_controller_, &AlbumCoverChoiceController::set_save_embedded_cover_override);

  cover_menu_->addActions(actions);

  ui_->tags_art_button->setMenu(cover_menu_);

  ui_->tags_art->installEventFilter(this);
  ui_->tags_art->setAcceptDrops(true);

  ui_->summary_art->installEventFilter(this);

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

QString TagEditor::SettingsGroup() const {

  return QLatin1String(kSettingsGroup);

}

TagEditor::~TagEditor() {
  delete ui_;
}

void TagEditor::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {
    album_cover_choice_controller_->ReloadSettings();
    cover_types_ = AlbumCoverLoaderOptions::LoadTypes();
  }

  // Restores the geometry and the current tab.
  SongEditor::showEvent(e);

}

void TagEditor::LoadingChanged(const bool loading) {

#ifdef HAVE_TAGFETCHER
  ui_->fetch_tag->setEnabled(!loading);
#else
  Q_UNUSED(loading)
#endif

}

void TagEditor::NoSongs() {

  // Show a summary with empty information
  UpdateSummaryTab(Song());
  ui_->tab_widget->setCurrentWidget(ui_->tab_summary);

}

bool TagEditor::eventFilter(QObject *o, QEvent *e) {

  if (e->type() == QEvent::Wheel) {
    if (QAbstractSpinBox *spinbox = qobject_cast<QAbstractSpinBox*>(o)) {
      if (!spinbox->hasFocus()) {
        QCoreApplication::sendEvent(ui_->scrollarea->viewport(), e);
        return true;
      }
    }
  }

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

  return SongEditor::eventFilter(o, e);

}

void TagEditor::UpdateSelection(const QModelIndexList &indexes) {

  // If we're editing multiple songs then we have to disable certain tabs
  const bool multiple = indexes.count() > 1;
  ui_->tab_widget->setTabEnabled(ui_->tab_widget->indexOf(ui_->tab_summary), !multiple);

  if (multiple) {
    UpdateSummaryTab(Song());
    UpdateStatisticsTab(Song());
  }
  else {
    UpdateSummaryTab(data_.value(indexes.first().row()).original_);
    UpdateStatisticsTab(data_.value(indexes.first().row()).original_);
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
  bool titlesort_enabled = false;
  bool artistsort_enabled = false;
  bool albumsort_enabled = false;
  bool has_id3v2_support = false;
  int id3v2_version = 0;
  bool id3v2_version_different = false;
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
    if (song.titlesort_supported()) {
      titlesort_enabled = true;
    }
    if (song.artistsort_supported()) {
      artistsort_enabled = true;
    }
    if (song.albumsort_supported()) {
      albumsort_enabled = true;
    }
    if (song.id3v2_tags_supported()) {
      has_id3v2_support = true;
      if (id3v2_version == 0) {
        id3v2_version = song.id3v2_version();
      }
      else if (id3v2_version != song.id3v2_version()) {
        id3v2_version_different = true;
      }
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
  // Keep the base HTML so the art summary can be appended later without losing the rich-text formatting (toPlainText() would strip the bold markup).
  tags_summary_text_ = summary;
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

  const bool embedded_cover = first_song.save_embedded_cover_supported() && (first_song.art_embedded() || album_cover_choice_controller_->get_collection_save_album_cover_type() == CoverOptions::CoverType::Embedded);
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
  ui_->titlesort->setEnabled(titlesort_enabled);
  ui_->artistsort->setEnabled(artistsort_enabled);
  ui_->albumsort->setEnabled(albumsort_enabled);

  ui_->label_id3v2_version->setVisible(has_id3v2_support);
  ui_->combobox_id3v2_version->setVisible(has_id3v2_support);

  if (has_id3v2_support) {
    // Set default based on existing version(s)
    if (id3v2_version_different || id3v2_version == 0) {
      // Mixed versions or unknown - default to ID3v2.4
      ui_->combobox_id3v2_version->setCurrentIndex(kComboBoxIndex_ID3v2_4);
    }
    else if (id3v2_version == kID3v2_Version_3) {
      ui_->combobox_id3v2_version->setCurrentIndex(kComboBoxIndex_ID3v2_3);
    }
    else {
      ui_->combobox_id3v2_version->setCurrentIndex(kComboBoxIndex_ID3v2_4);
    }
  }

}

void TagEditor::SetText(QLabel *label, const int value, const QString &suffix, const QString &def) {
  label->setText(value <= 0 ? def : (QString::number(value) + QLatin1Char(' ') + suffix));
}

void TagEditor::SetDate(QLabel *label, const qint64 time) {

  if (time == std::numeric_limits<qint64>::max()) {  // -1
    label->setText(QObject::tr("Unknown"));
  }
  else {
    label->setText(QDateTime::fromSecsSinceEpoch(time).toString(QLocale::system().dateTimeFormat(QLocale::LongFormat)));
  }

}

void TagEditor::UpdateSummaryTab(const Song &song) {

  AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
  cover_options.types = cover_types_;
  cover_options.desired_scaled_size = QSize(kSmallImageSize, kSmallImageSize);
  cover_options.device_pixel_ratio = devicePixelRatioF();
  summary_cover_art_id_ = albumcover_loader_->LoadImageAsync(cover_options, song);

  summary_text_ = u"<p><b>"_s + song.PrettyTitleWithArtist().toHtmlEscaped() + u"</b></p>"_s;
  ui_->summary->setText(summary_text_);

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

QString TagEditor::GetArtSummary(const Song &song, const AlbumCoverLoaderResult::Type cover_type) {

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

QString TagEditor::GetArtSummary(const UpdateCoverAction cover_action) {

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

void TagEditor::UpdateStatisticsTab(const Song &song) {

  ui_->playcount->setText(QString::number(song.playcount()));
  ui_->skipcount->setText(QString::number(song.skipcount()));
  ui_->lastplayed->setText(song.lastplayed() <= 0 ? tr("Never") : QDateTime::fromSecsSinceEpoch(song.lastplayed()).toString(QLocale::system().dateTimeFormat(QLocale::LongFormat)));

}

void TagEditor::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (id == summary_cover_art_id_) {
    if (result.success && !result.image_scaled.isNull() && result.type != AlbumCoverLoaderResult::Type::Unset) {
      ui_->summary_art->setPixmap(QPixmap::fromImage(result.image_scaled));
    }
    else {
      ui_->summary_art->setPixmap(QPixmap::fromImage(image_no_cover_thumbnail_));
    }
    if (ui_->song_list->selectionModel()->selectedIndexes().count() > 0) {
      const QModelIndex idx = ui_->song_list->selectionModel()->selectedIndexes().first();
      QString summary = summary_text_;
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
      QString summary = tags_summary_text_;
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

Song *TagEditor::GetFirstSelected() {

  const QModelIndexList sel = ui_->song_list->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) return nullptr;
  return &data_[sel.first().row()].current_;

}

void TagEditor::LoadCoverFromFile() {

  Song *song = GetFirstSelected();
  if (!song) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromFile(song);
  if (result.is_valid()) UpdateCover(UpdateCoverAction::New, result);

}

void TagEditor::SaveCoverToFile() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;

  const Data first_data = data_.value(ui_->song_list->selectionModel()->selectedIndexes().first().row());
  album_cover_choice_controller_->SaveCoverToFileManual(first_data.current_, first_data.cover_result_);

}

void TagEditor::LoadCoverFromURL() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->LoadImageFromURL();
  if (result.is_valid()) UpdateCover(UpdateCoverAction::New, result);

}

void TagEditor::SearchForCover() {

  Song *song = GetFirstSelected();
  if (!song) return;

  const AlbumCoverImageResult result = album_cover_choice_controller_->SearchForImage(song);
  if (result.is_valid()) UpdateCover(UpdateCoverAction::New, result);

}

void TagEditor::UnsetCover() {

  Song *song = GetFirstSelected();
  if (!song) return;

  song->set_art_embedded(false);
  song->clear_art_automatic();
  song->clear_art_manual();
  song->set_art_unset(true);

  UpdateCover(UpdateCoverAction::Unset);

}

void TagEditor::ClearCover() {

  Song *song = GetFirstSelected();
  if (!song) return;

  song->set_art_embedded(false);
  song->clear_art_automatic();
  song->clear_art_manual();
  song->set_art_unset(false);

  UpdateCover(UpdateCoverAction::Clear);

}

void TagEditor::DeleteCover() {

  UpdateCover(UpdateCoverAction::Delete);

}

void TagEditor::ShowCover() {

  if (ui_->song_list->selectionModel()->selectedIndexes().isEmpty()) return;
  const Data first_data = data_.value(ui_->song_list->selectionModel()->selectedIndexes().first().row());
  album_cover_choice_controller_->ShowCover(first_data.current_, first_data.cover_result_.image);

}

void TagEditor::UpdateCover(const UpdateCoverAction cover_action, const AlbumCoverImageResult &cover_result) {

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
    if (artist != data_[idx.row()].current_.effective_albumartist() || album != data_[idx.row()].current_.album()) {
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

void TagEditor::Save() {

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

    // Updates the lyrics storage and writes the LRC file.
    bool lyrics_changed = false;
    bool synced_lyrics_changed = false;
    PrepareLyricsForSave(&ref, &lyrics_changed, &synced_lyrics_changed);

    // Lyrics are only written to the tags when they are stored there, otherwise changing them alone does not require writing the tags.
    Song song_without_lyrics(ref.current_);
    song_without_lyrics.set_lyrics(ref.original_.lyrics());
    song_without_lyrics.set_synced_lyrics(ref.original_.synced_lyrics());
    const bool save_lyrics_tags = lyrics_changed && ref.current_.lyrics_storage_type() == Song::StorageType::Tag;
    const bool save_synced_lyrics_tags = synced_lyrics_changed && ref.current_.synced_lyrics_storage_type() == Song::StorageType::Tag;

    const bool save_tags = !song_without_lyrics.IsMetadataEqual(ref.original_) || save_lyrics_tags || save_synced_lyrics_tags;
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
            if (cover_url.isValid()) {
              ref.current_.set_art_manual(cover_url);
              ref.current_.set_art_unset(false);
            }
            else {
              // Saving the cover failed, UpdateCover() already cleared the cover when it was chosen, so restore the existing cover instead of clearing it.
              ref.current_.set_art_manual(ref.original_.art_manual());
              ref.current_.set_art_unset(ref.original_.art_unset());
              // The cover is not changed, so writing the other tags must not handle it as a new cover either.
              ref.cover_action_ = UpdateCoverAction::None;
            }
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
      // For streaming tracks, skip tag writing since there's no local file.
      // The metadata will be applied directly to the playlist item in MainWindow::TagEditorAccepted.
      if (ref.current_.is_stream()) {
        continue;
      }

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

      // Determine ID3v2 version based on user selection
      TagID3v2Version tag_id3v2_version = TagID3v2Version::Default;
      if (ref.current_.filetype() == Song::FileType::MPEG || ref.current_.filetype() == Song::FileType::WAV || ref.current_.filetype() == Song::FileType::AIFF) {
        tag_id3v2_version = ui_->combobox_id3v2_version->currentIndex() == kComboBoxIndex_ID3v2_3 ? TagID3v2Version::V3 : TagID3v2Version::V4;
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
      // Lyrics saved to the database or an LRC file do not depend on writing the tags, so they are still saved to the collection if writing the tags fails.
      Song song_lyrics_not_in_tags;
      if ((lyrics_changed && ref.current_.lyrics_storage_type() != Song::StorageType::Tag) || (synced_lyrics_changed && ref.current_.synced_lyrics_storage_type() != Song::StorageType::Tag)) {
        song_lyrics_not_in_tags = ref.original_;
        if (lyrics_changed && ref.current_.lyrics_storage_type() != Song::StorageType::Tag) {
          song_lyrics_not_in_tags.set_lyrics(ref.current_.lyrics());
          song_lyrics_not_in_tags.set_lyrics_storage_type(ref.current_.lyrics_storage_type());
        }
        if (synced_lyrics_changed && ref.current_.synced_lyrics_storage_type() != Song::StorageType::Tag) {
          song_lyrics_not_in_tags.set_synced_lyrics(ref.current_.synced_lyrics());
          song_lyrics_not_in_tags.set_synced_lyrics_storage_type(ref.current_.synced_lyrics_storage_type());
          song_lyrics_not_in_tags.set_lrc_mtime(ref.current_.lrc_mtime());
        }
      }

      TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(ref.current_.url().toLocalFile(), ref.current_, save_tags_options, save_tag_cover_data, tag_id3v2_version);
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, i, ref, save_lyrics_tags, save_synced_lyrics_tags, song_lyrics_not_in_tags, connection]() {
        // The lyrics stored in the tags were not saved when writing the tags failed.
        if (!reply->success()) {
          RevertLyrics(i, save_lyrics_tags, save_synced_lyrics_tags);
        }
        SongSaveTagsComplete(reply, ref.current_.url().toLocalFile(), ref.current_, ref.cover_action_, song_lyrics_not_in_tags);
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

    // Lyrics stored in the database or in an LRC file still need the collection to be updated when nothing was written to the tags.
    if (!(save_tags || save_playcount || save_rating || save_embedded_cover) && (lyrics_changed || synced_lyrics_changed) && ref.current_.is_local_collection_song()) {
      collection_songs_.insert(ref.current_.id(), ref.current_);
    }

  }

  if (save_tag_pending_ <= 0) SaveDataFinished();

}

void TagEditor::ResetPlayStatistics() {

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

void TagEditor::SongRated(const float rating) {

  const QModelIndexList indexes = ui_->song_list->selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) return;

  for (const QModelIndex &idx : indexes) {
    if (!data_[idx.row()].current_.is_valid()) continue;
    data_[idx.row()].current_.set_rating(rating);
  }

}

void TagEditor::FetchTag() {

#ifdef HAVE_TAGFETCHER

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

void TagEditor::FetchTagSongChosen(const Song &original_song, const Song &new_metadata) {

#ifdef HAVE_TAGFETCHER

  const QUrl url = original_song.url();

  // Find the song with this URL, not the local filename, which is empty for all streams.
  auto data_it = std::find_if(data_.begin(), data_.end(), [&url](const Data &d) {
    return d.original_.url() == url;
  });
  if (data_it == data_.end()) {
    qLog(Warning) << "Could not find song with URL" << url;
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

void TagEditor::SongSaveTagsComplete(TagReaderReplyPtr reply, const QString &filename, Song song, const UpdateCoverAction cover_action, const Song &song_lyrics_not_in_tags) {

  SongSaveTagsFinished(reply->success(), reply->error(), filename, song, cover_action, song_lyrics_not_in_tags);

}

void TagEditor::SongSaveTagsFinished(const bool success, const QString &error, const QString &filename, Song song, const UpdateCoverAction cover_action, const Song &song_lyrics_not_in_tags) {

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
    }
    if (cover_action != UpdateCoverAction::None && song == current_albumcover_loader_->last_song()) {
      current_albumcover_loader_->LoadAlbumCover(song);
    }
  }

  // Updates the collection when all songs are saved.
  SongSaveFinished(success, error, filename, song, song_lyrics_not_in_tags);

}
