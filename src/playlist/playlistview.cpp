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

#include <cmath>
#include <algorithm>

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QTreeView>
#include <QHeaderView>
#include <QClipboard>
#include <QKeySequence>
#include <QMimeData>
#include <QMetaType>
#include <QList>
#include <QSize>
#include <QTimeLine>
#include <QTimer>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QRegion>
#include <QStyleOptionViewItem>
#include <QLinearGradient>
#include <QScrollBar>
#include <QtEvents>
#include <QSettings>

#include "includes/qt_blurimage.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/player.h"
#include "playlistmanager.h"
#include "playlist.h"
#include "playlistdelegates.h"
#include "playlistheader.h"
#include "playlistview.h"
#include "playlistfilter.h"
#include "playlistproxystyle.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"
#include "constants/appearancesettings.h"
#include "constants/playlistsettings.h"
#include "dynamicplaylistcontrols.h"

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbaritemdelegate.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kGlowIntensitySteps = 24;
constexpr int kAutoscrollGraceTimeout = 30;  // seconds
constexpr int kDropIndicatorWidth = 2;
constexpr int kDropIndicatorGradientWidth = 5;
constexpr int kHeaderStateVersion = 2;
}  // namespace

PlaylistView::PlaylistView(QWidget *parent)
    : QTreeView(parent),
      style_(new PlaylistProxyStyle(QApplication::style()->name())),
      playlist_(nullptr),
      header_(new PlaylistHeader(Qt::Horizontal, this, this)),
      background_image_type_(AppearanceSettings::BackgroundImageType::Default),
      background_image_position_(AppearanceSettings::BackgroundImagePosition::BottomRight),
      background_image_maxsize_(0),
      background_image_stretch_(false),
      background_image_do_not_cut_(true),
      background_image_keep_aspect_ratio_(true),
      blur_radius_(AppearanceSettings::kDefaultBlurRadius),
      opacity_level_(AppearanceSettings::kDefaultOpacityLevel),
      background_initialized_(false),
      set_initial_header_layout_(false),
      header_state_loaded_(false),
      header_state_restored_(false),
      read_only_settings_(false),
      previous_background_image_opacity_(0.0),
      fade_animation_(new QTimeLine(1000, this)),
      force_background_redraw_(false),
      last_height_(-1),
      last_width_(-1),
      current_background_image_x_(0),
      current_background_image_y_(0),
      previous_background_image_x_(0),
      previous_background_image_y_(0),
      bars_enabled_(true),
      glow_enabled_(true),
      select_track_(false),
      auto_sort_(false),
      currently_glowing_(false),
      glow_intensity_step_(0),
      inhibit_autoscroll_timer_(new QTimer(this)),
      inhibit_autoscroll_(false),
      currently_autoscrolling_(false),
      row_height_(-1),
      currenttrack_play_(u":/pictures/currenttrack_play.png"_s),
      currenttrack_pause_(u":/pictures/currenttrack_pause.png"_s),
      cached_current_row_row_(-1),
      drop_indicator_row_(-1),
      drag_over_(false),
      column_alignment_(DefaultColumnAlignment()),
      rating_locked_(false),
      dynamic_controls_(new DynamicPlaylistControls(this)),
      rating_delegate_(nullptr) {

  setHeader(header_);
  header_->setSectionsMovable(true);
  header_->setFirstSectionMovable(true);
  header_->setSortIndicator(static_cast<int>(Playlist::Column::Title), Qt::AscendingOrder);

  setStyle(style_);
  setMouseTracking(true);
  setAlternatingRowColors(true);
  setAttribute(Qt::WA_MacShowFocusRect, false);
#ifdef Q_OS_MACOS
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#endif

  QObject::connect(header_, &PlaylistHeader::sectionResized, this, &PlaylistView::HeaderSectionResized);
  QObject::connect(header_, &PlaylistHeader::sectionMoved, this, &PlaylistView::SetHeaderState);
  QObject::connect(header_, &PlaylistHeader::sortIndicatorChanged, this, &PlaylistView::SetHeaderState);
  QObject::connect(header_, &PlaylistHeader::SectionVisibilityChanged, this, &PlaylistView::SetHeaderState);

  QObject::connect(header_, &PlaylistHeader::sectionResized, this, &PlaylistView::InvalidateCachedCurrentPixmap);
  QObject::connect(header_, &PlaylistHeader::sectionMoved, this, &PlaylistView::InvalidateCachedCurrentPixmap);
  QObject::connect(header_, &PlaylistHeader::SectionVisibilityChanged, this, &PlaylistView::InvalidateCachedCurrentPixmap);
  QObject::connect(header_, &PlaylistHeader::StretchEnabledChanged, this, &PlaylistView::StretchChanged);

  QObject::connect(header_, &PlaylistHeader::SectionRatingLockStatusChanged, this, &PlaylistView::SetRatingLockStatus);
  QObject::connect(header_, &PlaylistHeader::MouseEntered, this, &PlaylistView::RatingHoverOut);

  inhibit_autoscroll_timer_->setInterval(kAutoscrollGraceTimeout * 1000);
  inhibit_autoscroll_timer_->setSingleShot(true);
  QObject::connect(inhibit_autoscroll_timer_, &QTimer::timeout, this, &PlaylistView::InhibitAutoscrollTimeout);

  horizontalScrollBar()->installEventFilter(this);
  verticalScrollBar()->installEventFilter(this);

  dynamic_controls_->hide();

  // To proper scale all pixmaps
  device_pixel_ratio_ = devicePixelRatioF();

  // For fading
  QObject::connect(fade_animation_, &QTimeLine::valueChanged, this, &PlaylistView::FadePreviousBackgroundImage);
  fade_animation_->setDirection(QTimeLine::Direction::Backward);  // 1.0 -> 0.0

}

PlaylistView::~PlaylistView() {
  style_->deleteLater();
}

void PlaylistView::Init(const SharedPtr<Player> player,
                        const SharedPtr<PlaylistManager> playlist_manager,
                        const SharedPtr<CollectionBackend> collection_backend,
#ifdef HAVE_MOODBAR
                        const SharedPtr<MoodbarLoader> moodbar_loader,
#endif
                        const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader) {

  player_ = player;
  playlist_manager_ = playlist_manager;
  collection_backend_ = collection_backend;
  current_albumcover_loader_ = current_albumcover_loader;

#ifdef HAVE_MOODBAR
  moodbar_loader_ = moodbar_loader;
#endif

  SetItemDelegates();

  QObject::connect(&*playlist_manager, &PlaylistManager::CurrentSongChanged, this, &PlaylistView::SongChanged);
  QObject::connect(&*current_albumcover_loader_, &CurrentAlbumCoverLoader::AlbumCoverLoaded, this, &PlaylistView::AlbumCoverLoaded);
  QObject::connect(&*player, &Player::Playing, this, &PlaylistView::StartGlowing);
  QObject::connect(&*player, &Player::Paused, this, &PlaylistView::StopGlowing);
  QObject::connect(&*player, &Player::Stopped, this, &PlaylistView::Stopped);

}

void PlaylistView::SetItemDelegates() {

  setItemDelegate(new PlaylistDelegateBase(this));

  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Title), new TextItemDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::TitleSort), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::TitleSort));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Album), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::Album));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::AlbumSort), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::AlbumSort));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Artist), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::Artist));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::ArtistSort), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::ArtistSort));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::AlbumArtist), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::AlbumArtist));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::AlbumArtistSort), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::AlbumArtistSort));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Genre), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::Genre));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Composer), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::Composer));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::ComposerSort), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::ComposerSort));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Performer), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::Performer));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::PerformerSort), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::PerformerSort));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Grouping), new TagCompletionItemDelegate(this, collection_backend_, Playlist::Column::Grouping));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Length), new LengthItemDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Filesize), new SizeItemDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Filetype), new FileTypeItemDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::DateCreated), new DateItemDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::DateModified), new DateItemDelegate(this));

  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Samplerate), new PlaylistDelegateBase(this, tr("Hz")));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Bitdepth), new PlaylistDelegateBase(this, tr("Bit")));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Bitrate), new PlaylistDelegateBase(this, tr("kbps")));

  setItemDelegateForColumn(static_cast<int>(Playlist::Column::URL), new NativeSeparatorsDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::LastPlayed), new LastPlayedItemDelegate(this));

  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Source), new SongSourceDelegate(this));

#ifdef HAVE_MOODBAR
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Moodbar), new MoodbarItemDelegate(moodbar_loader_, this, this));
#endif

  rating_delegate_ = new RatingItemDelegate(this);
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::Rating), rating_delegate_);

  setItemDelegateForColumn(static_cast<int>(Playlist::Column::EBUR128IntegratedLoudness), new Ebur128LoudnessLUFSItemDelegate(this));
  setItemDelegateForColumn(static_cast<int>(Playlist::Column::EBUR128LoudnessRange), new Ebur128LoudnessRangeLUItemDelegate(this));

}

void PlaylistView::setModel(QAbstractItemModel *m) {

  if (model()) {
    QObject::disconnect(model(), &QAbstractItemModel::dataChanged, this, &PlaylistView::InvalidateCachedCurrentPixmap);
    QObject::disconnect(model(), &QAbstractItemModel::layoutAboutToBeChanged, this, &PlaylistView::RatingHoverOut);

    // When changing the model, always invalidate the current pixmap.
    // If a remote client uses "stop after", without invaliding the stop mark would not appear.
    InvalidateCachedCurrentPixmap();
  }

  QTreeView::setModel(m);

  QObject::connect(model(), &QAbstractItemModel::dataChanged, this, &PlaylistView::InvalidateCachedCurrentPixmap);
  QObject::connect(model(), &QAbstractItemModel::layoutAboutToBeChanged, this, &PlaylistView::RatingHoverOut);

}

void PlaylistView::SetPlaylist(Playlist *playlist) {

  if (playlist_) {
    QObject::disconnect(playlist_, &Playlist::MaybeAutoscroll, this, &PlaylistView::MaybeAutoscroll);
    QObject::disconnect(playlist_, &Playlist::destroyed, this, &PlaylistView::PlaylistDestroyed);
    QObject::disconnect(playlist_, &Playlist::QueueChanged, this, &PlaylistView::Update);

    QObject::disconnect(playlist_, &Playlist::DynamicModeChanged, this, &PlaylistView::DynamicModeChanged);
    QObject::disconnect(dynamic_controls_, &DynamicPlaylistControls::Expand, playlist_, &Playlist::ExpandDynamicPlaylist);
    QObject::disconnect(dynamic_controls_, &DynamicPlaylistControls::Repopulate, playlist_, &Playlist::RepopulateDynamicPlaylist);
    QObject::disconnect(dynamic_controls_, &DynamicPlaylistControls::TurnOff, playlist_, &Playlist::TurnOffDynamicPlaylist);
  }

  playlist_ = playlist;
  RestoreHeaderState();
  DynamicModeChanged(playlist->is_dynamic());
  setFocus();
  JumpToLastPlayedTrack();
  playlist->set_auto_sort(auto_sort_);

  QObject::connect(playlist_, &Playlist::RestoreFinished, this, &PlaylistView::JumpToLastPlayedTrack);
  QObject::connect(playlist_, &Playlist::MaybeAutoscroll, this, &PlaylistView::MaybeAutoscroll);
  QObject::connect(playlist_, &Playlist::destroyed, this, &PlaylistView::PlaylistDestroyed);
  QObject::connect(playlist_, &Playlist::QueueChanged, this, &PlaylistView::Update);

  QObject::connect(playlist_, &Playlist::DynamicModeChanged, this, &PlaylistView::DynamicModeChanged);
  QObject::connect(dynamic_controls_, &DynamicPlaylistControls::Expand, playlist_, &Playlist::ExpandDynamicPlaylist);
  QObject::connect(dynamic_controls_, &DynamicPlaylistControls::Repopulate, playlist_, &Playlist::RepopulateDynamicPlaylist);
  QObject::connect(dynamic_controls_, &DynamicPlaylistControls::TurnOff, playlist_, &Playlist::TurnOffDynamicPlaylist);

}

void PlaylistView::LoadHeaderState() {

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  // Since we use serialized internal data structures, we cannot read anything but the current version
  const int header_state_version = s.value(PlaylistSettings::kStateVersion, 0).toInt();
  if (s.contains(PlaylistSettings::kState)) {
    if (header_state_version == kHeaderStateVersion) {
      header_state_ = s.value(PlaylistSettings::kState).toByteArray();
    }
    else {
      // Force header state reset since column indices may have changed between versions
      header_state_.clear();
    }
  }
  if (s.contains(PlaylistSettings::kColumnAlignments)) {
    if (header_state_version == kHeaderStateVersion) {
      column_alignment_ = s.value(PlaylistSettings::kColumnAlignments).value<ColumnAlignmentMap>();
    }
    else {
      // Force column alignment reset since column indices may have changed between versions
      column_alignment_.clear();
    }
  }
  s.endGroup();

  if (column_alignment_.isEmpty()) {
    column_alignment_ = DefaultColumnAlignment();
  }

  header_state_loaded_ = true;

}

void PlaylistView::SetHeaderState() {

  if (!header_state_loaded_) return;
  header_state_ = header_->SaveState();

}

void PlaylistView::ResetHeaderState() {

  set_initial_header_layout_ = true;
  header_state_ = header_->ResetState();
  RestoreHeaderState();

}

void PlaylistView::RestoreHeaderState() {

  if (!header_state_loaded_) LoadHeaderState();

  if (header_state_.isEmpty() || !header_->RestoreState(header_state_)) {
    set_initial_header_layout_ = true;
  }

  if (set_initial_header_layout_) {

    header_->SetStretchEnabled(true);

    header_->HideSection(static_cast<int>(Playlist::Column::TitleSort));
    header_->HideSection(static_cast<int>(Playlist::Column::ArtistSort));
    header_->HideSection(static_cast<int>(Playlist::Column::AlbumSort));
    header_->HideSection(static_cast<int>(Playlist::Column::AlbumArtist));
    header_->HideSection(static_cast<int>(Playlist::Column::AlbumArtistSort));
    header_->HideSection(static_cast<int>(Playlist::Column::Performer));
    header_->HideSection(static_cast<int>(Playlist::Column::PerformerSort));
    header_->HideSection(static_cast<int>(Playlist::Column::Composer));
    header_->HideSection(static_cast<int>(Playlist::Column::ComposerSort));
    header_->HideSection(static_cast<int>(Playlist::Column::Year));
    header_->HideSection(static_cast<int>(Playlist::Column::OriginalYear));
    header_->HideSection(static_cast<int>(Playlist::Column::Disc));
    header_->HideSection(static_cast<int>(Playlist::Column::Genre));
    header_->HideSection(static_cast<int>(Playlist::Column::URL));
    header_->HideSection(static_cast<int>(Playlist::Column::BaseFilename));
    header_->HideSection(static_cast<int>(Playlist::Column::Filesize));
    header_->HideSection(static_cast<int>(Playlist::Column::DateCreated));
    header_->HideSection(static_cast<int>(Playlist::Column::DateModified));
    header_->HideSection(static_cast<int>(Playlist::Column::PlayCount));
    header_->HideSection(static_cast<int>(Playlist::Column::SkipCount));
    header_->HideSection(static_cast<int>(Playlist::Column::LastPlayed));
    header_->HideSection(static_cast<int>(Playlist::Column::Comment));
    header_->HideSection(static_cast<int>(Playlist::Column::Grouping));
    header_->HideSection(static_cast<int>(Playlist::Column::Moodbar));
    header_->HideSection(static_cast<int>(Playlist::Column::Rating));
    header_->HideSection(static_cast<int>(Playlist::Column::HasCUE));
    header_->HideSection(static_cast<int>(Playlist::Column::EBUR128IntegratedLoudness));
    header_->HideSection(static_cast<int>(Playlist::Column::EBUR128LoudnessRange));
    header_->HideSection(static_cast<int>(Playlist::Column::BPM));
    header_->HideSection(static_cast<int>(Playlist::Column::Mood));
    header_->HideSection(static_cast<int>(Playlist::Column::InitialKey));

    header_->ShowSection(static_cast<int>(Playlist::Column::Track));
    header_->ShowSection(static_cast<int>(Playlist::Column::Title));
    header_->ShowSection(static_cast<int>(Playlist::Column::Artist));
    header_->ShowSection(static_cast<int>(Playlist::Column::Album));
    header_->ShowSection(static_cast<int>(Playlist::Column::Samplerate));
    header_->ShowSection(static_cast<int>(Playlist::Column::Bitdepth));
    header_->ShowSection(static_cast<int>(Playlist::Column::Bitrate));
    header_->ShowSection(static_cast<int>(Playlist::Column::Filetype));
    header_->ShowSection(static_cast<int>(Playlist::Column::Source));

    header_->moveSection(header_->visualIndex(static_cast<int>(Playlist::Column::Track)), 0);

    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Track), 0.06);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Title), 0.23);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Artist), 0.23);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Album), 0.23);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Length), 0.04);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Samplerate), 0.05);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Bitdepth), 0.04);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Bitrate), 0.04);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Filetype), 0.04);
    header_->SetColumnWidth(static_cast<int>(Playlist::Column::Source), 0.04);

    header_state_ = header_->SaveState();
    header_->RestoreState(header_state_);

    set_initial_header_layout_ = false;

  }

  // Make sure at least one column is visible
  bool all_hidden = true;
  for (int i = 0; i < header_->count(); ++i) {
    if (!header_->isSectionHidden(i) && header_->sectionSize(i) > 0) {
      all_hidden = false;
      break;
    }
  }
  if (all_hidden) {
    header_->ShowSection(static_cast<int>(Playlist::Column::Title));
  }

  header_state_restored_ = true;

  Q_EMIT ColumnAlignmentChanged(column_alignment_);

}

void PlaylistView::HeaderSectionResized(const int logical_index, const int old_size, const int new_size) {

  Q_UNUSED(logical_index)
  Q_UNUSED(old_size)

  if (new_size != 0) {
    SetHeaderState();
  }

}

void PlaylistView::ReloadBarPixmaps() {

  currenttrack_bar_left_ = LoadBarPixmap(u":/pictures/currenttrack_bar_left.png"_s, true);
  currenttrack_bar_mid_ = LoadBarPixmap(u":/pictures/currenttrack_bar_mid.png"_s, false);
  currenttrack_bar_right_ = LoadBarPixmap(u":/pictures/currenttrack_bar_right.png"_s, true);

}

QList<QPixmap> PlaylistView::LoadBarPixmap(const QString &filename, const bool keep_aspect_ratio) {

  QImage image(filename);
  QImage image_scaled;
  if (keep_aspect_ratio) {
    image_scaled = image.scaledToHeight(row_height_, Qt::SmoothTransformation);
  }
  else {
    image_scaled = image.scaled(image.width(), row_height_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

  // Colour the bar with the palette colour
  QPainter p(&image_scaled);
  p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
  p.setOpacity(0.7);
  if (playlist_playing_song_color_.isValid()) {
    p.fillRect(image_scaled.rect(), playlist_playing_song_color_);
  }
  else {
    p.fillRect(image_scaled.rect(), QApplication::palette().color(QPalette::Highlight));
  }
  p.end();

  // Animation steps
  QList<QPixmap> ret;
  ret.reserve(kGlowIntensitySteps);
  for (int i = 0; i < kGlowIntensitySteps; ++i) {
    QImage step(image_scaled.copy());
    p.begin(&step);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.setOpacity(0.4 - 0.6 * sin(static_cast<float>(i) / kGlowIntensitySteps * (M_PI / 2)));
    p.fillRect(step.rect(), Qt::white);
    p.end();
    ret << QPixmap::fromImage(step);
  }

  return ret;

}

void PlaylistView::LoadTinyPlayPausePixmaps(const int desired_size) {

  QImage image_play = QImage(u":/pictures/tiny-play.png"_s).scaled(desired_size, desired_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QImage image_pause = QImage(u":/pictures/tiny-pause.png"_s).scaled(desired_size, desired_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  pixmap_tinyplay_ = QPixmap::fromImage(image_play);
  pixmap_tinypause_ = QPixmap::fromImage(image_pause);

}

void PlaylistView::drawTree(QPainter *painter, const QRegion &region) const {

  const_cast<PlaylistView*>(this)->current_paint_region_ = region;
  QTreeView::drawTree(painter, region);
  const_cast<PlaylistView*>(this)->current_paint_region_ = QRegion();

}

void PlaylistView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  QStyleOptionViewItem opt(option);

  bool is_current = idx.data(Playlist::Role_IsCurrent).toBool();
  bool is_paused = idx.data(Playlist::Role_IsPaused).toBool();

  if (is_current) {

    if (bars_enabled_) {

      const_cast<PlaylistView*>(this)->last_current_item_ = idx;
      const_cast<PlaylistView*>(this)->last_glow_rect_ = opt.rect;

      int step = glow_intensity_step_;
      if (step >= kGlowIntensitySteps) {
        step = 2 * (kGlowIntensitySteps - 1) - step + 1;
      }

      if (currenttrack_bar_left_.count() < kGlowIntensitySteps ||
          currenttrack_bar_mid_.count() < kGlowIntensitySteps ||
          currenttrack_bar_right_.count() < kGlowIntensitySteps ||
          opt.rect.height() != row_height_) {
        // Recreate the pixmaps if the height changed since last time
        const_cast<PlaylistView*>(this)->row_height_ = opt.rect.height();
        const_cast<PlaylistView*>(this)->ReloadBarPixmaps();
      }

      QRect middle(opt.rect);
      middle.setLeft(middle.left() + currenttrack_bar_left_[0].width());
      middle.setRight(middle.right() - currenttrack_bar_right_[0].width());

      // Selection
      if (selectionModel()->isSelected(idx)) {
        painter->fillRect(opt.rect, opt.palette.color(QPalette::Highlight));
      }

      // Draw the bar
      painter->setRenderHint(QPainter::SmoothPixmapTransform);
      painter->drawPixmap(opt.rect.topLeft(), currenttrack_bar_left_[step]);
      painter->drawPixmap(opt.rect.topRight() - currenttrack_bar_right_[0].rect().topRight(), currenttrack_bar_right_[step]);
      painter->drawPixmap(middle, currenttrack_bar_mid_[step]);

      // Draw the play icon
      QPoint play_pos(currenttrack_bar_left_[0].width() / 3 * 2, (opt.rect.height() - currenttrack_play_.height()) / 2);
      painter->drawPixmap(opt.rect.topLeft() + play_pos, is_paused ? currenttrack_pause_ : currenttrack_play_);

      // Set the font
      opt.palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QApplication::palette().color(QPalette::Active, QPalette::HighlightedText));
      opt.palette.setColor(QPalette::Text, QApplication::palette().color(QPalette::HighlightedText));
      opt.palette.setColor(QPalette::Highlight, Qt::transparent);
      opt.palette.setColor(QPalette::AlternateBase, Qt::transparent);
      opt.decorationSize = QSize(20, 20);

      // Draw the actual row data on top.  We cache this, because it's fairly expensive (1-2ms), and we do it many times per second.
      if (cached_current_row_rect_ != opt.rect || cached_current_row_row_ != idx.row() || cached_current_row_.isNull()) {
        // We can't update the cache if we're not drawing the entire region,
        // QTreeView clips its drawing to only the columns in the region, so it wouldn't update the whole pixmap properly.
        const bool whole_region = current_paint_region_.boundingRect().width() == viewport()->width();
        if (whole_region) {
          const_cast<PlaylistView*>(this)->UpdateCachedCurrentRowPixmap(opt, idx);
          painter->drawPixmap(opt.rect, cached_current_row_);
        }
        else {
          QTreeView::drawRow(painter, opt, idx);
        }
      }
      else {
        painter->drawPixmap(opt.rect, cached_current_row_);
      }
    }
    else {
      painter->save();
      if (pixmap_tinyplay_.isNull() || pixmap_tinypause_.isNull() || opt.rect.height() != row_height_) {
        const_cast<PlaylistView*>(this)->row_height_ = opt.rect.height();
        const_cast<PlaylistView*>(this)->LoadTinyPlayPausePixmaps(static_cast<int>(static_cast<float>(opt.rect.height()) / 1.4F));
      }
      int pixmap_width = 0;
      int pixmap_height = 0;
      if (is_paused) {
        pixmap_width = pixmap_tinypause_.width();
        pixmap_height = pixmap_tinypause_.height();
      }
      else {
        pixmap_width = pixmap_tinyplay_.width();
        pixmap_height = pixmap_tinyplay_.height();
      }
      QPoint play_pos(pixmap_width / 2, (opt.rect.height() - pixmap_height) / 2);
      if (selectionModel()->isSelected(idx)) {
        painter->fillRect(opt.rect, opt.palette.color(QPalette::Highlight));
      }
      painter->drawPixmap(opt.rect.topLeft() + play_pos, is_paused ? pixmap_tinypause_ : pixmap_tinyplay_);
      painter->restore();
      QTreeView::drawRow(painter, opt, idx);
    }
  }
  else {
    QTreeView::drawRow(painter, opt, idx);
  }

}

void PlaylistView::UpdateCachedCurrentRowPixmap(QStyleOptionViewItem option, const QModelIndex &idx) {  // clazy:exclude=function-args-by-ref

  cached_current_row_rect_ = option.rect;
  cached_current_row_row_ = idx.row();

  option.rect.moveTo(0, 0);
  cached_current_row_ = QPixmap(static_cast<int>(option.rect.width() * device_pixel_ratio_), static_cast<int>(option.rect.height() * device_pixel_ratio_));
  cached_current_row_.setDevicePixelRatio(device_pixel_ratio_);
  cached_current_row_.fill(Qt::transparent);

  QPainter p(&cached_current_row_);
  QTreeView::drawRow(&p, option, idx);

}

void PlaylistView::InvalidateCachedCurrentPixmap() {
  cached_current_row_ = QPixmap();
}

void PlaylistView::timerEvent(QTimerEvent *event) {
  QTreeView::timerEvent(event);
  if (event->timerId() == glow_timer_.timerId()) GlowIntensityChanged();
}

void PlaylistView::GlowIntensityChanged() {
  glow_intensity_step_ = (glow_intensity_step_ + 1) % (kGlowIntensitySteps * 2);

  viewport()->update(last_glow_rect_);
}

void PlaylistView::StopGlowing() {

  currently_glowing_ = false;
  glow_timer_.stop();
  glow_intensity_step_ = kGlowIntensitySteps;

}

void PlaylistView::StartGlowing() {

  currently_glowing_ = true;
  if (isVisible() && glow_enabled_) {
    glow_timer_.start(1500 / kGlowIntensitySteps, this);
  }

}

void PlaylistView::hideEvent(QHideEvent *e) {
  glow_timer_.stop();
  QTreeView::hideEvent(e);
}

void PlaylistView::showEvent(QShowEvent *e) {

  if (currently_glowing_ && glow_enabled_) {
    glow_timer_.start(1500 / kGlowIntensitySteps, this);
  }

  MaybeAutoscroll(Playlist::AutoScroll::Maybe);

  QTreeView::showEvent(e);

}

namespace {
bool CompareSelectionRanges(const QItemSelectionRange &a, const QItemSelectionRange &b) {
  return b.bottom() < a.bottom();
}
}  // namespace

void PlaylistView::keyPressEvent(QKeyEvent *event) {

  if (!model() || state() == QAbstractItemView::EditingState) {
    QTreeView::keyPressEvent(event);
  }
  else if (event == QKeySequence::Delete) {
    RemoveSelected();
    event->accept();
  }
#ifdef Q_OS_MACOS
  else if (event->key() == Qt::Key_Backspace) {
    RemoveSelected();
    event->accept();
  }
#endif
  else if (event == QKeySequence::Copy) {
    CopyCurrentSongToClipboard();
  }
  else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
    if (currentIndex().isValid()) Q_EMIT PlayItem(currentIndex(), Playlist::AutoScroll::Never);
    event->accept();
  }
  else if (event->modifiers() != Qt::ControlModifier && event->key() == Qt::Key_Space) {
    Q_EMIT PlayPause();
    event->accept();
  }
  else if (event->key() == Qt::Key_Left) {
    Q_EMIT SeekBackward();
    event->accept();
  }
  else if (event->key() == Qt::Key_Right) {
    Q_EMIT SeekForward();
    event->accept();
  }
  else if (event->modifiers() == Qt::NoModifier && ((event->key() >= Qt::Key_Exclam && event->key() <= Qt::Key_Z) || event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Escape)) {
    Q_EMIT FocusOnFilterSignal(event);
    event->accept();
  }
  else {
    QTreeView::keyPressEvent(event);
  }

}

void PlaylistView::contextMenuEvent(QContextMenuEvent *e) {
  Q_EMIT RightClicked(e->globalPos(), indexAt(e->pos()));
  e->accept();
}

void PlaylistView::RemoveSelected() {

  int rows_removed = 0;
  QItemSelection selection(selectionModel()->selection());

  if (selection.isEmpty()) {
    return;
  }

  // Store the last selected row, which is the last in the list
  int last_row = selection.last().top();

  // Sort the selection, so we remove the items at the *bottom* first, ensuring we don't have to mess around with changing row numbers
  std::sort(selection.begin(), selection.end(), CompareSelectionRanges);

  for (const QItemSelectionRange &range : selection) {
    if (range.top() < last_row) rows_removed += range.height();
    model()->removeRows(range.top(), range.height(), range.parent());
  }

  int new_row = last_row - rows_removed;
  // Index of the first column for the row to select
  QModelIndex new_idx = model()->index(new_row, 0);

  // Select the new current item, we want always the item after the last selected
  if (new_idx.isValid()) {
    // Workaround to update keyboard selected row, if it's not the first row (this also triggers selection)
    if (new_row != 0) {
      keyPressEvent(new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier));
    }
    // Update visual selection with the entire row
    selectionModel()->select(new_idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  }
  else {
    // We're removing the last item, select the new last row
    selectionModel()->select(model()->index(model()->rowCount() - 1, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  }

}

QList<int> PlaylistView::GetEditableColumns() {

  QList<int> columns;
  QHeaderView *h = header();
  for (int col = 0; col < h->count(); col++) {
    if (h->isSectionHidden(col)) continue;
    QModelIndex idx = model()->index(0, col);
    if (idx.flags() & Qt::ItemIsEditable) columns << h->visualIndex(col);
  }
  std::sort(columns.begin(), columns.end());
  return columns;

}

QModelIndex PlaylistView::NextEditableIndex(const QModelIndex &current) {

  QList<int> columns = GetEditableColumns();
  QHeaderView *h = header();
  int idx = static_cast<int>(columns.indexOf(h->visualIndex(current.column())));

  if (idx + 1 >= columns.size()) {
    return model()->index(current.row() + 1, h->logicalIndex(columns.first()));
  }

  return model()->index(current.row(), h->logicalIndex(columns[idx + 1]));

}

QModelIndex PlaylistView::PrevEditableIndex(const QModelIndex &current) {

  QList<int> columns = GetEditableColumns();
  QHeaderView *h = header();
  int idx = static_cast<int>(columns.indexOf(h->visualIndex(current.column())));

  if (idx - 1 < 0) {
    return model()->index(current.row() - 1, h->logicalIndex(columns.last()));
  }

  return model()->index(current.row(), h->logicalIndex(columns[idx - 1]));

}

void PlaylistView::closeEditor(QWidget *editor, const QAbstractItemDelegate::EndEditHint hint) {

  if (hint == QAbstractItemDelegate::NoHint) {
    QTreeView::closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
  }
  else if (hint == QAbstractItemDelegate::EditNextItem || hint == QAbstractItemDelegate::EditPreviousItem) {

    QModelIndex idx;
    if (hint == QAbstractItemDelegate::EditNextItem) {
      idx = NextEditableIndex(currentIndex());
    }
    else {
      idx = PrevEditableIndex(currentIndex());
    }

    if (idx.isValid()) {
      QTreeView::closeEditor(editor, QAbstractItemDelegate::NoHint);
      setCurrentIndex(idx);
      QAbstractItemView::edit(idx);
    }
    else {
      QTreeView::closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
    }
  }
  else {
    QTreeView::closeEditor(editor, hint);
  }

}

void PlaylistView::mouseMoveEvent(QMouseEvent *event) {

  // Check whether rating section is locked by user or not
  if (!rating_locked_) {
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid() && idx.data(Playlist::Role_CanSetRating).toBool()) {
      RatingHoverIn(idx, event->pos());
    }
    else if (rating_delegate_->is_mouse_over()) {
      RatingHoverOut();
    }
  }

  if (!drag_over_) {
    QTreeView::mouseMoveEvent(event);
  }

}

void PlaylistView::leaveEvent(QEvent *e) {

  if (rating_delegate_->is_mouse_over() && !rating_locked_) {
    RatingHoverOut();
  }

  QTreeView::leaveEvent(e);

}

void PlaylistView::mousePressEvent(QMouseEvent *event) {

  if (editTriggers() & QAbstractItemView::NoEditTriggers) {
    QTreeView::mousePressEvent(event);
    return;
  }

  QModelIndex idx = indexAt(event->pos());
  if (idx.isValid()) {
    switch (event->button()) {
      case Qt::XButton1:
        player_->Previous();
        break;
      case Qt::XButton2:
        player_->Next();
        break;
      case Qt::LeftButton:{
        if (idx.data(Playlist::Role_CanSetRating).toBool() && !rating_locked_) {
          // Calculate which star was clicked
          float new_rating = RatingPainter::RatingForPos(event->pos(), visualRect(idx));
          if (selectedIndexes().contains(idx)) {
            // Update all the selected item ratings
            QModelIndexList src_index_list;
            const QModelIndexList indexes = selectedIndexes();
            for (const QModelIndex &i : indexes) {
              if (i.data(Playlist::Role_CanSetRating).toBool()) {
                src_index_list << playlist_->filter()->mapToSource(i);
              }
            }
            if (!src_index_list.isEmpty()) {
              playlist_->RateSongs(src_index_list, new_rating);
            }
          }
          else {
            // Update only this item rating
            playlist_->RateSong(playlist_->filter()->mapToSource(idx), new_rating);
          }
        }
        break;
      }
      default:
        break;
    }
  }

  inhibit_autoscroll_ = true;
  inhibit_autoscroll_timer_->start();

  QTreeView::mousePressEvent(event);

}

void PlaylistView::scrollContentsBy(const int dx, const int dy) {

  if (dx != 0) {
    InvalidateCachedCurrentPixmap();
  }
  cached_tree_ = QPixmap();

  QTreeView::scrollContentsBy(dx, dy);

  if (!currently_autoscrolling_) {
    // We only want to do this if the scroll was initiated by the user
    inhibit_autoscroll_ = true;
    inhibit_autoscroll_timer_->start();
  }

}

void PlaylistView::InhibitAutoscrollTimeout() {
  // For 30 seconds after the user clicks on or scrolls the playlist we promise not to automatically scroll the view to keep up with a track change.
  inhibit_autoscroll_ = false;
}

void PlaylistView::MaybeAutoscroll(const Playlist::AutoScroll autoscroll) {

  if (autoscroll == Playlist::AutoScroll::Always || (autoscroll == Playlist::AutoScroll::Maybe && !inhibit_autoscroll_)) {
    JumpToCurrentlyPlayingTrack();
  }

}

void PlaylistView::JumpToCurrentlyPlayingTrack() {

  Q_ASSERT(playlist_);

  if (playlist_->current_row() == -1) return;

  QModelIndex current = playlist_->filter()->mapFromSource(playlist_->index(playlist_->current_row(), 0));
  if (!current.isValid()) return;

  if (visibleRegion().boundingRect().contains(visualRect(current))) return;

  // Usage of the "Jump to the currently playing track" action shall enable autoscroll
  inhibit_autoscroll_ = false;

  // Scroll to the item
  currently_autoscrolling_ = true;
  scrollTo(current, QAbstractItemView::PositionAtCenter);
  currently_autoscrolling_ = false;

}

void PlaylistView::JumpToLastPlayedTrack() {

  Q_ASSERT(playlist_);

  if (playlist_->last_played_row() == -1) return;

  QModelIndex last_played = playlist_->filter()->mapFromSource(playlist_->index(playlist_->last_played_row(), 0));
  if (!last_played.isValid()) return;

  // Select last played song
  last_current_item_ = last_played;
  setCurrentIndex(last_current_item_);

  // Scroll to the item
  currently_autoscrolling_ = true;
  scrollTo(last_played, QAbstractItemView::PositionAtCenter);
  currently_autoscrolling_ = false;

}

void PlaylistView::paintEvent(QPaintEvent *event) {

  // Reimplemented to draw the background image.
  // Reimplemented also to draw the drop indicator
  // When the user is dragging some stuff over the playlist paintEvent gets called for the entire viewport every time the user moves the mouse.
  // The drawTree is kinda expensive, so we cache the result and draw from the cache while the user is dragging.
  // The cached pixmap gets invalidated in dragLeaveEvent, dropEvent and scrollContentsBy.

  // Draw background
  if (background_image_type_ == AppearanceSettings::BackgroundImageType::Custom || background_image_type_ == AppearanceSettings::BackgroundImageType::Album) {
    if (!background_image_.isNull() || !previous_background_image_.isNull()) {
      QPainter background_painter(viewport());

      int pb_height = height() - header_->height();
      int pb_width = verticalScrollBar()->isVisible() ? width() - style()->pixelMetric(QStyle::PM_ScrollBarExtent) : width();

      // Check if we should recompute the background image
      if (pb_height != last_height_ || pb_width != last_width_ || force_background_redraw_) {

        if (background_image_.isNull()) {
          cached_scaled_background_image_ = QPixmap();
        }
        else {
          if (background_image_stretch_) {
            if (background_image_keep_aspect_ratio_) {
              if (background_image_do_not_cut_) {
                cached_scaled_background_image_ = QPixmap::fromImage(background_image_.scaled(pb_width, pb_height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
              }
              else {
                if (pb_height >= pb_width) {
                  cached_scaled_background_image_ = QPixmap::fromImage(background_image_.scaledToHeight(pb_height, Qt::SmoothTransformation));
                }
                else {
                  cached_scaled_background_image_ = QPixmap::fromImage(background_image_.scaledToWidth(pb_width, Qt::SmoothTransformation));
                }
              }
            }
            else {
              cached_scaled_background_image_ = QPixmap::fromImage(background_image_.scaled(pb_width, pb_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
          }
          else {
            int resize_width = qMin(qMin(background_image_.size().width(), pb_width), background_image_maxsize_);
            int resize_height = qMin(qMin(background_image_.size().height(), pb_height), background_image_maxsize_);
            cached_scaled_background_image_ = QPixmap::fromImage(background_image_.scaled(resize_width, resize_height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
          }
        }

        last_height_ = pb_height;
        last_width_ = pb_width;
        force_background_redraw_ = false;
      }

      // Actually draw the background image
      if (!cached_scaled_background_image_.isNull()) {
        // Set opactiy only if needed, as this deactivate hardware acceleration
        if (!qFuzzyCompare(previous_background_image_opacity_, static_cast<qreal>(0.0))) {
          background_painter.setOpacity(1.0 - previous_background_image_opacity_);
        }
        switch (background_image_position_) {
          case AppearanceSettings::BackgroundImagePosition::UpperLeft:
            current_background_image_x_ = 0;
            current_background_image_y_ = 0;
            break;
          case AppearanceSettings::BackgroundImagePosition::UpperRight:
            current_background_image_x_ = (pb_width - cached_scaled_background_image_.width());
            current_background_image_y_ = 0;
            break;
          case AppearanceSettings::BackgroundImagePosition::Middle:
            current_background_image_x_ = ((pb_width - cached_scaled_background_image_.width()) / 2);
            current_background_image_y_ = ((pb_height - cached_scaled_background_image_.height()) / 2);
            break;
          case AppearanceSettings::BackgroundImagePosition::BottomLeft:
            current_background_image_x_ = 0;
            current_background_image_y_ = (pb_height - cached_scaled_background_image_.height());
            break;
          case AppearanceSettings::BackgroundImagePosition::BottomRight:
          default:
            current_background_image_x_ = (pb_width - cached_scaled_background_image_.width());
            current_background_image_y_ = (pb_height - cached_scaled_background_image_.height());
        }
        background_painter.drawPixmap(current_background_image_x_, current_background_image_y_, cached_scaled_background_image_);
      }
      // Draw the previous background image if we're fading
      if (!previous_background_image_.isNull()) {
        background_painter.setOpacity(previous_background_image_opacity_);
        background_painter.drawPixmap(previous_background_image_x_, previous_background_image_y_, previous_background_image_);
      }
    }
  }

  QPainter p(viewport());

  if (drop_indicator_row_ != -1) {
    if (cached_tree_.isNull()) {
      cached_tree_ = QPixmap(static_cast<int>(size().width() * device_pixel_ratio_), static_cast<int>(size().height() * device_pixel_ratio_));
      cached_tree_.setDevicePixelRatio(device_pixel_ratio_);
      cached_tree_.fill(Qt::transparent);

      QPainter cache_painter(&cached_tree_);
      drawTree(&cache_painter, event->region());
    }

    p.drawPixmap(0, 0, cached_tree_);
  }
  else {
    drawTree(&p, event->region());
    return;
  }

  const int first_column = header_->logicalIndex(0);

  // Find the y position of the drop indicator
  QModelIndex drop_index = model()->index(drop_indicator_row_, first_column);
  int drop_pos = -1;
  switch (dropIndicatorPosition()) {
    case QAbstractItemView::OnItem:
      return;  // Don't draw anything

    case QAbstractItemView::AboveItem:
      drop_pos = visualRect(drop_index).top();
      break;

    case QAbstractItemView::BelowItem:
      drop_pos = visualRect(drop_index).bottom() + 1;
      break;

    case QAbstractItemView::OnViewport:
      if (model()->rowCount() == 0) {
        drop_pos = 1;
      }
      else {
        drop_pos = 1 + visualRect(model()->index(model()->rowCount() - 1, first_column)).bottom();
      }
      break;
  }

  // Draw a nice gradient first
  QColor line_color(QApplication::palette().color(QPalette::Highlight));
  QColor shadow_color(line_color.lighter(140));
  QColor shadow_fadeout_color(shadow_color);
  shadow_color.setAlpha(255);
  shadow_fadeout_color.setAlpha(0);

  QLinearGradient gradient(QPoint(0, drop_pos - kDropIndicatorGradientWidth), QPoint(0, drop_pos + kDropIndicatorGradientWidth));
  gradient.setColorAt(0.0, shadow_fadeout_color);
  gradient.setColorAt(0.5, shadow_color);
  gradient.setColorAt(1.0, shadow_fadeout_color);
  QPen gradient_pen(QBrush(gradient), kDropIndicatorGradientWidth * 2);
  p.setPen(gradient_pen);
  p.drawLine(QPoint(0, drop_pos), QPoint(width(), drop_pos));

  // Now draw the line on top
  QPen line_pen(line_color, kDropIndicatorWidth);
  p.setPen(line_pen);
  p.drawLine(QPoint(0, drop_pos), QPoint(width(), drop_pos));

  QTreeView::paintEvent(event);

}

void PlaylistView::dragMoveEvent(QDragMoveEvent *event) {

  QTreeView::dragMoveEvent(event);

  QModelIndex idx(indexAt(event->position().toPoint()));

  drop_indicator_row_ = idx.isValid() ? idx.row() : 0;

}

void PlaylistView::dragEnterEvent(QDragEnterEvent *event) {

  QTreeView::dragEnterEvent(event);
  cached_tree_ = QPixmap();
  drag_over_ = true;

}

void PlaylistView::dragLeaveEvent(QDragLeaveEvent *event) {

  QTreeView::dragLeaveEvent(event);
  cached_tree_ = QPixmap();
  drag_over_ = false;
  drop_indicator_row_ = -1;

}

void PlaylistView::dropEvent(QDropEvent *event) {

  QTreeView::dropEvent(event);
  cached_tree_ = QPixmap();
  drop_indicator_row_ = -1;
  drag_over_ = false;

}

void PlaylistView::PlaylistDestroyed() {
  playlist_ = nullptr;
  // We'll get a SetPlaylist() soon
}

void PlaylistView::ReloadSettings() {

  Settings s;

  s.beginGroup(PlaylistSettings::kSettingsGroup);
  bars_enabled_ = s.value(PlaylistSettings::kShowBars, true).toBool();
#ifdef Q_OS_MACOS
  bool glow_effect = false;
#else
  bool glow_effect = true;
#endif
  glow_enabled_ = bars_enabled_ && s.value(PlaylistSettings::kGlowEffect, glow_effect).toBool();
  bool editmetadatainline = s.value(PlaylistSettings::kEditMetadataInline, false).toBool();
  select_track_ = s.value(PlaylistSettings::kSelectTrack, false).toBool();
  auto_sort_ = s.value(PlaylistSettings::kAutoSort, false).toBool();
  setAlternatingRowColors(s.value(PlaylistSettings::kAlternatingRowColors, true).toBool());
  s.endGroup();

  s.beginGroup(AppearanceSettings::kSettingsGroup);
  QVariant background_image_type_var = s.value(AppearanceSettings::kBackgroundImageType);
  QVariant background_image_position_var = s.value(AppearanceSettings::kBackgroundImagePosition);
  int background_image_maxsize = s.value(AppearanceSettings::kBackgroundImageMaxSize).toInt();
  if (background_image_maxsize <= 10) background_image_maxsize = 9000;
  QString background_image_filename = s.value(AppearanceSettings::kBackgroundImageFilename).toString();
  bool background_image_stretch = s.value(AppearanceSettings::kBackgroundImageStretch, false).toBool();
  bool background_image_do_not_cut = s.value(AppearanceSettings::kBackgroundImageDoNotCut, true).toBool();
  bool background_image_keep_aspect_ratio = s.value(AppearanceSettings::kBackgroundImageKeepAspectRatio, true).toBool();
  int blur_radius = s.value(AppearanceSettings::kBlurRadius, AppearanceSettings::kDefaultBlurRadius).toInt();
  int opacity_level = s.value(AppearanceSettings::kOpacityLevel, AppearanceSettings::kDefaultOpacityLevel).toInt();
  QColor playlist_playing_song_color = s.value(AppearanceSettings::kPlaylistPlayingSongColor).value<QColor>();
  if (playlist_playing_song_color != playlist_playing_song_color_) {
    row_height_ = -1;
  }
  playlist_playing_song_color_ = playlist_playing_song_color;
  s.endGroup();

  if (currently_glowing_ && glow_enabled_ && isVisible()) StartGlowing();
  if (!glow_enabled_) StopGlowing();

  // Background:
  AppearanceSettings::BackgroundImageType background_image_type(AppearanceSettings::BackgroundImageType::Default);
  if (background_image_type_var.isValid()) {
    background_image_type = static_cast<AppearanceSettings::BackgroundImageType>(background_image_type_var.toInt());
  }
  else {
    background_image_type = AppearanceSettings::BackgroundImageType::Default;
  }

  AppearanceSettings::BackgroundImagePosition background_image_position(AppearanceSettings::BackgroundImagePosition::BottomRight);
  if (background_image_position_var.isValid()) {
    background_image_position = static_cast<AppearanceSettings::BackgroundImagePosition>(background_image_position_var.toInt());
  }
  else {
    background_image_position = AppearanceSettings::BackgroundImagePosition::BottomRight;
  }

  // Check if background properties have changed.
  // We change properties only if they have actually changed, to avoid to call set_background_image when it is not needed,
  // as this will cause the fading animation to start again.
  // This also avoid to do useless "force_background_redraw".

  if (
      !background_initialized_ ||
      background_image_type != background_image_type_ ||
      background_image_filename != background_image_filename_ ||
      background_image_position != background_image_position_ ||
      background_image_maxsize != background_image_maxsize_ ||
      background_image_stretch != background_image_stretch_ ||
      background_image_do_not_cut != background_image_do_not_cut_ ||
      background_image_keep_aspect_ratio != background_image_keep_aspect_ratio_ ||
      blur_radius_ != blur_radius ||
      opacity_level_ != opacity_level
     ) {

    background_initialized_ = true;
    background_image_type_ = background_image_type;
    background_image_filename_ = background_image_filename;
    background_image_position_ = background_image_position;
    background_image_maxsize_ = background_image_maxsize;
    background_image_stretch_ = background_image_stretch;
    background_image_do_not_cut_ = background_image_do_not_cut;
    background_image_keep_aspect_ratio_ = background_image_keep_aspect_ratio;
    blur_radius_ = blur_radius;
    opacity_level_ = opacity_level;

    if (background_image_type_ == AppearanceSettings::BackgroundImageType::Custom) {
      set_background_image(QImage(background_image_filename));
    }
    else if (background_image_type_ == AppearanceSettings::BackgroundImageType::Album) {
      set_background_image(current_song_cover_art_);
    }
    else {
      // User changed background image type to something that will not be painted through paintEvent: reset all background images.
      // This avoids to use old (deprecated) images for fading when selecting Album or Custom background image type later.
      set_background_image(QImage());
      cached_scaled_background_image_ = QPixmap();
      previous_background_image_ = QPixmap();
    }
    setProperty("default_background_enabled", background_image_type_ == AppearanceSettings::BackgroundImageType::Default);
    setProperty("strawbs_background_enabled", background_image_type_ == AppearanceSettings::BackgroundImageType::Strawbs);
    Q_EMIT BackgroundPropertyChanged();
    force_background_redraw_ = true;
  }

  if (editmetadatainline)
    setEditTriggers(editTriggers() | QAbstractItemView::SelectedClicked);
  else
    setEditTriggers(editTriggers() & ~QAbstractItemView::SelectedClicked);

  if (playlist_) playlist_->set_auto_sort(auto_sort_);

}

void PlaylistView::SaveSettings() {

  if (!header_state_loaded_ || read_only_settings_) return;

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  s.setValue(PlaylistSettings::kStateVersion, kHeaderStateVersion);
  s.setValue(PlaylistSettings::kState, header_->SaveState());
  s.setValue(PlaylistSettings::kColumnAlignments, QVariant::fromValue<ColumnAlignmentMap>(column_alignment_));
  s.setValue(PlaylistSettings::kRatingLocked, rating_locked_);
  s.endGroup();

}

void PlaylistView::StretchChanged(const bool stretch) {

  if (!header_state_loaded_) return;

  setHorizontalScrollBarPolicy(stretch ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
  SetHeaderState();

}

void PlaylistView::resizeEvent(QResizeEvent *e) {

  QTreeView::resizeEvent(e);

  if (dynamic_controls_->isVisible()) {
    RepositionDynamicControls();
  }

}

bool PlaylistView::eventFilter(QObject *object, QEvent *event) {

  if (event->type() == QEvent::Enter && (object == horizontalScrollBar() || object == verticalScrollBar())) {
    return false;  // clazy:exclude=base-class-event
  }
  return QAbstractItemView::eventFilter(object, event);

}

void PlaylistView::rowsInserted(const QModelIndex &parent, const int start, const int end) {

  const bool at_end = end == model()->rowCount(parent) - 1;

  QTreeView::rowsInserted(parent, start, end);

  if (at_end) {
    // If the rows were inserted at the end of the playlist then let's scroll the view so the user can see.
    scrollTo(model()->index(start, 0, parent), QAbstractItemView::PositionAtTop);
  }

}

ColumnAlignmentMap PlaylistView::DefaultColumnAlignment() {

  ColumnAlignmentMap ret;

  ret[static_cast<int>(Playlist::Column::Year)] =
  ret[static_cast<int>(Playlist::Column::OriginalYear)] =
  ret[static_cast<int>(Playlist::Column::Track)] =
  ret[static_cast<int>(Playlist::Column::Disc)] =
  ret[static_cast<int>(Playlist::Column::Length)] =
  ret[static_cast<int>(Playlist::Column::Samplerate)] =
  ret[static_cast<int>(Playlist::Column::Bitdepth)] =
  ret[static_cast<int>(Playlist::Column::Bitrate)] =
  ret[static_cast<int>(Playlist::Column::Filesize)] =
  ret[static_cast<int>(Playlist::Column::PlayCount)] =
  ret[static_cast<int>(Playlist::Column::SkipCount)] =
  ret[static_cast<int>(Playlist::Column::BPM)] =
 (Qt::AlignRight | Qt::AlignVCenter);

  return ret;

}

void PlaylistView::SetColumnAlignment(const int section, const Qt::Alignment alignment) {

  if (section < 0) return;

  column_alignment_[section] = alignment;
  Q_EMIT ColumnAlignmentChanged(column_alignment_);
  SaveSettings();

}

Qt::Alignment PlaylistView::column_alignment(int section) const {
  return column_alignment_.value(section, Qt::AlignLeft | Qt::AlignVCenter);
}

void PlaylistView::CopyCurrentSongToClipboard() const {

  // Get the display text of all visible columns.
  QStringList columns;

  for (int i = 0; i < header()->count(); ++i) {
    if (header()->isSectionHidden(i)) {
      continue;
    }

    const QVariant var_data = model()->data(currentIndex().sibling(currentIndex().row(), i));
    if (var_data.metaType().id() == QMetaType::QString) {
      columns << var_data.toString();
    }
  }

  // Get the song's URL
  const QUrl url = model()->data(currentIndex().sibling(currentIndex().row(), static_cast<int>(Playlist::Column::URL))).toUrl();

  QMimeData *mime_data = new QMimeData;
  mime_data->setUrls(QList<QUrl>() << url);
  mime_data->setText(columns.join(" - "_L1));

  QApplication::clipboard()->setMimeData(mime_data);

}

void PlaylistView::SongChanged(const Song &song) {

  song_playing_ = song;

  if (select_track_ && playlist_) {
    clearSelection();
    QItemSelection selection(playlist_->index(playlist_->current_row(), 0), playlist_->index(playlist_->current_row(), Playlist::ColumnCount - 1));
    selectionModel()->select(selection, QItemSelectionModel::Select);
  }

}

void PlaylistView::Playing() {}

void PlaylistView::Stopped() {

  if (song_playing_ == Song()) return;
  song_playing_ = Song();
  StopGlowing();
  AlbumCoverLoaded(Song());

}

void PlaylistView::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  if ((song != Song() && song_playing_ == Song()) || result.album_cover.image == current_song_cover_art_) return;

  current_song_cover_art_ = result.album_cover.image;

  if (background_image_type_ == AppearanceSettings::BackgroundImageType::Album) {
    set_background_image(result.success && result.type != AlbumCoverLoaderResult::Type::None && result.type != AlbumCoverLoaderResult::Type::Unset ? current_song_cover_art_ : QImage());
    force_background_redraw_ = true;
    update();
  }

}

void PlaylistView::set_background_image(const QImage &image) {

  // Save previous image, for fading
  previous_background_image_ = cached_scaled_background_image_;
  previous_background_image_x_ = current_background_image_x_;
  previous_background_image_y_ = current_background_image_y_;

  if (image.isNull() || image.format() == QImage::Format_ARGB32) {
    background_image_ = image;
  }
  else {
    background_image_ = image.convertToFormat(QImage::Format_ARGB32);
  }

  if (!background_image_.isNull()) {
    // Apply opacity filter: scale (not overwrite!) the alpha channel
    uchar *bits = background_image_.bits();
    for (int i = 0; i < background_image_.height() * background_image_.bytesPerLine(); i += 4) {
      bits[i + 3] = static_cast<uchar>(bits[i + 3] * (opacity_level_ / 100.0));
    }

    if (blur_radius_ != 0) {
      QImage blurred(background_image_.size(), QImage::Format_ARGB32_Premultiplied);
      blurred.fill(Qt::transparent);
      QPainter blur_painter(&blurred);
      qt_blurImage(&blur_painter, background_image_, blur_radius_, true, false);
      blur_painter.end();

      background_image_ = blurred;
    }
  }

  if (isVisible()) {
    previous_background_image_opacity_ = 1.0;
    if (fade_animation_->state() != QTimeLine::State::NotRunning) {
      fade_animation_->stop();
    }
    fade_animation_->start();
  }

}

void PlaylistView::FadePreviousBackgroundImage(const qreal value) {

  previous_background_image_opacity_ = value;
  if (qFuzzyCompare(previous_background_image_opacity_, static_cast<qreal>(0.0))) {
    previous_background_image_ = QPixmap();
    previous_background_image_opacity_ = 0.0;
  }

  update();

}

void PlaylistView::focusInEvent(QFocusEvent *event) {

  QTreeView::focusInEvent(event);

  if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason) {
    // If there's a current item but no selection it probably means the list was filtered, and the selected item does not match the filter.
    // If there's only 1 item in the view it is now impossible to select that item without using the mouse.
    const QModelIndex &current = selectionModel()->currentIndex();
    if (current.isValid() && selectionModel()->selectedIndexes().isEmpty()) {
      QItemSelection new_selection(current.sibling(current.row(), 0), current.sibling(current.row(), current.model()->columnCount(current.parent()) - 1));
      selectionModel()->select(new_selection, QItemSelectionModel::Select);
    }
  }

}

void PlaylistView::DynamicModeChanged(const bool dynamic) {

  if (dynamic) {
    RepositionDynamicControls();
    dynamic_controls_->show();
  }
  else {
    dynamic_controls_->hide();
  }

}

void PlaylistView::RepositionDynamicControls() {

  dynamic_controls_->resize(dynamic_controls_->sizeHint());
  dynamic_controls_->move((width() - dynamic_controls_->width()) / 2, height() - dynamic_controls_->height() - 20);

}

void PlaylistView::SetRatingLockStatus(const bool state) {

  if (!header_state_loaded_) return;

  rating_locked_ = state;

}

void PlaylistView::RatingHoverIn(const QModelIndex &idx, const QPoint pos) {

  if (editTriggers() & QAbstractItemView::NoEditTriggers) {
    return;
  }

  const QModelIndex old_index = rating_delegate_->mouse_over_index();
  rating_delegate_->set_mouse_over(idx, selectedIndexes(), pos);
  setCursor(Qt::PointingHandCursor);

  update(idx);
  update(old_index);
  const QModelIndexList indexes = selectedIndexes();
  for (const QModelIndex &i : indexes) {
    if (i.column() == static_cast<int>(Playlist::Column::Rating)) update(i);
  }

  if (idx.data(Playlist::Role_IsCurrent).toBool() || old_index.data(Playlist::Role_IsCurrent).toBool()) {
    InvalidateCachedCurrentPixmap();
  }

}

void PlaylistView::RatingHoverOut() {

  if (editTriggers() & QAbstractItemView::NoEditTriggers) {
    return;
  }

  const QModelIndex old_index = rating_delegate_->mouse_over_index();
  rating_delegate_->set_mouse_out();
  setCursor(QCursor());

  update(old_index);
  const QModelIndexList indexes = selectedIndexes();
  for (const QModelIndex &i : indexes) {
    if (i.column() == static_cast<int>(Playlist::Column::Rating)) {
      update(i);
    }
  }

  if (old_index.data(Playlist::Role_IsCurrent).toBool()) {
    InvalidateCachedCurrentPixmap();
  }

}
