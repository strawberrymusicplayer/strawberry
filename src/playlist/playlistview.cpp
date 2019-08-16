/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <math.h>
#include <algorithm>

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QList>
#include <QAbstractItemView>
#include <QByteArray>
#include <QClipboard>
#include <QCommonStyle>
#include <QFontMetrics>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QMimeData>
#include <QSize>
#include <QSortFilterProxyModel>
#include <QTimeLine>
#include <QTimer>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QStringBuilder>
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
#include <QtAlgorithms>
#include <QStyleOptionHeader>
#include <QStyleOptionViewItem>
#include <QProxyStyle>
#include <QTreeView>
#include <QLinearGradient>
#include <QScrollBar>
#include <QtEvents>
#include <QSettings>

#include "core/application.h"
#include "core/player.h"
#include "core/qt_blurimage.h"
#include "core/song.h"
#include "playlistmanager.h"
#include "playlist.h"
#include "playlistdelegates.h"
#include "playlistheader.h"
#include "playlistview.h"
#include "covermanager/currentalbumcoverloader.h"
#include "settings/appearancesettingspage.h"
#include "settings/playlistsettingspage.h"

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbaritemdelegate.h"
#endif

using std::sort;

const int PlaylistView::kGlowIntensitySteps = 24;
const int PlaylistView::kAutoscrollGraceTimeout = 30;  // seconds
const int PlaylistView::kDropIndicatorWidth = 2;
const int PlaylistView::kDropIndicatorGradientWidth = 5;

PlaylistProxyStyle::PlaylistProxyStyle(QStyle *base)
    : QProxyStyle(base), common_style_(new QCommonStyle) {}

void PlaylistProxyStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {

  if (element == CE_Header) {
    const QStyleOptionHeader *header_option = qstyleoption_cast<const QStyleOptionHeader*>(option);
    const QRect &rect = header_option->rect;
    const QString &text = header_option->text;
    const QFontMetrics &font_metrics = header_option->fontMetrics;

    // Spaces added to make transition less abrupt
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    if (rect.width() < font_metrics.horizontalAdvance(text + "  ")) {
#else
    if (rect.width() < font_metrics.width(text + "  ")) {
#endif
      const Playlist::Column column = static_cast<Playlist::Column>(header_option->section);
      QStyleOptionHeader new_option(*header_option);
      new_option.text = Playlist::abbreviated_column_name(column);
      QProxyStyle::drawControl(element, &new_option, painter, widget);
      return;
    }
  }

  if (element == CE_ItemViewItem)
    common_style_->drawControl(element, option, painter, widget);
  else
    QProxyStyle::drawControl(element, option, painter, widget);

}

void PlaylistProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {

  if (element == QStyle::PE_PanelItemViewRow || element == QStyle::PE_PanelItemViewItem)
    common_style_->drawPrimitive(element, option, painter, widget);
  else
    QProxyStyle::drawPrimitive(element, option, painter, widget);

}


PlaylistView::PlaylistView(QWidget *parent)
    : QTreeView(parent),
      app_(nullptr),
      style_(new PlaylistProxyStyle(style())),
      playlist_(nullptr),
      header_(new PlaylistHeader(Qt::Horizontal, this, this)),
      background_image_type_(AppearanceSettingsPage::BackgroundImageType_Default),
      background_image_position_(AppearanceSettingsPage::BackgroundImagePosition_BottomRight),
      background_image_maxsize_(0),
      background_image_stretch_(false),
      background_image_do_not_cut_(true),
      background_image_keep_aspect_ratio_(true),
      blur_radius_(AppearanceSettingsPage::kDefaultBlurRadius),
      opacity_level_(AppearanceSettingsPage::kDefaultOpacityLevel),
      initialized_(false),
      background_initialized_(false),
      setting_initial_header_layout_(false),
      read_only_settings_(true),
      state_loaded_(false),
      previous_background_image_opacity_(0.0),
      fade_animation_(new QTimeLine(1000, this)),
      force_background_redraw_(false),
      last_height_(-1),
      last_width_(-1),
      current_background_image_x_(0),
      current_background_image_y_(0),
      previous_background_image_x_(0),
      previous_background_image_y_(0),
      glow_enabled_(true),
      currently_glowing_(false),
      glow_intensity_step_(0),
      inhibit_autoscroll_timer_(new QTimer(this)),
      inhibit_autoscroll_(false),
      currently_autoscrolling_(false),
      row_height_(-1),
      currenttrack_play_(":/pictures/currenttrack_play.png"),
      currenttrack_pause_(":/pictures/currenttrack_pause.png"),
      cached_current_row_row_(-1),
      drop_indicator_row_(-1),
      drag_over_(false) {

  setHeader(header_);
  header_->setSectionsMovable(true);
  setStyle(style_);
  setMouseTracking(true);

  connect(header_, SIGNAL(sectionResized(int,int,int)), SLOT(SaveGeometry()));
  connect(header_, SIGNAL(sectionMoved(int,int,int)), SLOT(SaveGeometry()));
  connect(header_, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), SLOT(SaveGeometry()));
  connect(header_, SIGNAL(SectionVisibilityChanged(int,bool)), SLOT(SaveGeometry()));

  connect(header_, SIGNAL(sectionResized(int,int,int)), SLOT(InvalidateCachedCurrentPixmap()));
  connect(header_, SIGNAL(sectionMoved(int,int,int)), SLOT(InvalidateCachedCurrentPixmap()));
  connect(header_, SIGNAL(SectionVisibilityChanged(int,bool)), SLOT(InvalidateCachedCurrentPixmap()));
  connect(header_, SIGNAL(StretchEnabledChanged(bool)), SLOT(StretchChanged(bool)));

  inhibit_autoscroll_timer_->setInterval(kAutoscrollGraceTimeout * 1000);
  inhibit_autoscroll_timer_->setSingleShot(true);
  connect(inhibit_autoscroll_timer_, SIGNAL(timeout()), SLOT(InhibitAutoscrollTimeout()));

  horizontalScrollBar()->installEventFilter(this);
  verticalScrollBar()->installEventFilter(this);

  setAlternatingRowColors(true);

  setAttribute(Qt::WA_MacShowFocusRect, false);

#ifdef Q_OS_MACOS
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#endif
  // For fading
  connect(fade_animation_, SIGNAL(valueChanged(qreal)), SLOT(FadePreviousBackgroundImage(qreal)));
  fade_animation_->setDirection(QTimeLine::Backward);  // 1.0 -> 0.0

  initialized_ = true;

}

PlaylistView::~PlaylistView() {
  delete style_;
}

void PlaylistView::SetApplication(Application *app) {

  Q_ASSERT(app);
  app_ = app;
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(const Song&)), this, SLOT(SongChanged(const Song&)));
  connect(app_->current_albumcover_loader(), SIGNAL(AlbumCoverLoaded(const Song&, const QUrl&, const QImage&)), SLOT(AlbumCoverLoaded(const Song&, const QUrl&, const QImage&)));
  connect(app_->player(), SIGNAL(Playing()), SLOT(StartGlowing()));
  connect(app_->player(), SIGNAL(Paused()), SLOT(StopGlowing()));
  connect(app_->player(), SIGNAL(Stopped()), SLOT(Stopped()));

}

void PlaylistView::SetItemDelegates(CollectionBackend *backend) {

  setItemDelegate(new PlaylistDelegateBase(this));

  setItemDelegateForColumn(Playlist::Column_Title, new TextItemDelegate(this));
  setItemDelegateForColumn(Playlist::Column_Album, new TagCompletionItemDelegate(this, backend, Playlist::Column_Album));
  setItemDelegateForColumn(Playlist::Column_Artist, new TagCompletionItemDelegate(this, backend, Playlist::Column_Artist));
  setItemDelegateForColumn(Playlist::Column_AlbumArtist, new TagCompletionItemDelegate(this, backend, Playlist::Column_AlbumArtist));
  setItemDelegateForColumn(Playlist::Column_Genre, new TagCompletionItemDelegate(this, backend, Playlist::Column_Genre));
  setItemDelegateForColumn(Playlist::Column_Composer, new TagCompletionItemDelegate(this, backend, Playlist::Column_Composer));
  setItemDelegateForColumn(Playlist::Column_Performer, new TagCompletionItemDelegate(this, backend, Playlist::Column_Performer));
  setItemDelegateForColumn(Playlist::Column_Grouping, new TagCompletionItemDelegate(this, backend, Playlist::Column_Grouping));
  setItemDelegateForColumn(Playlist::Column_Length, new LengthItemDelegate(this));
  setItemDelegateForColumn(Playlist::Column_Filesize, new SizeItemDelegate(this));
  setItemDelegateForColumn(Playlist::Column_Filetype, new FileTypeItemDelegate(this));
  setItemDelegateForColumn(Playlist::Column_DateCreated, new DateItemDelegate(this));
  setItemDelegateForColumn(Playlist::Column_DateModified, new DateItemDelegate(this));

  setItemDelegateForColumn(Playlist::Column_Samplerate, new PlaylistDelegateBase(this, ("Hz")));
  setItemDelegateForColumn(Playlist::Column_Bitdepth, new PlaylistDelegateBase(this, ("Bit")));
  setItemDelegateForColumn(Playlist::Column_Bitrate, new PlaylistDelegateBase(this, tr("kbps")));

  setItemDelegateForColumn(Playlist::Column_Filename, new NativeSeparatorsDelegate(this));
  setItemDelegateForColumn(Playlist::Column_LastPlayed, new LastPlayedItemDelegate(this));

  setItemDelegateForColumn(Playlist::Column_Source, new SongSourceDelegate(this));

#ifdef HAVE_MOODBAR
  setItemDelegateForColumn(Playlist::Column_Mood, new MoodbarItemDelegate(app_, this, this));
#endif

}

void PlaylistView::SetPlaylist(Playlist *playlist) {

  if (playlist_) {
    disconnect(playlist_, SIGNAL(CurrentSongChanged(Song)), this, SLOT(MaybeAutoscroll()));
    disconnect(playlist_, SIGNAL(destroyed()), this, SLOT(PlaylistDestroyed()));
    disconnect(playlist_, SIGNAL(QueueChanged()), this, SLOT(update()));
  }

  playlist_ = playlist;
  LoadGeometry();
  ReloadSettings();
  setFocus();
  read_only_settings_ = false;
  JumpToLastPlayedTrack();

  connect(playlist_, SIGNAL(RestoreFinished()), SLOT(JumpToLastPlayedTrack()));
  connect(playlist_, SIGNAL(CurrentSongChanged(Song)), SLOT(MaybeAutoscroll()));
  connect(playlist_, SIGNAL(destroyed()), SLOT(PlaylistDestroyed()));
  connect(playlist_, SIGNAL(QueueChanged()), SLOT(update()));

}

void PlaylistView::setModel(QAbstractItemModel *m) {

  if (model()) {
    disconnect(model(), SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(InvalidateCachedCurrentPixmap()));

    // When changing the model, always invalidate the current pixmap.
    // If a remote client uses "stop after", without invaliding the stop mark would not appear.
    InvalidateCachedCurrentPixmap();
  }

  QTreeView::setModel(m);

  connect(model(), SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(InvalidateCachedCurrentPixmap()));

}

void PlaylistView::LoadGeometry() {

  if (!state_loaded_) {
    QSettings s;
    s.beginGroup(Playlist::kSettingsGroup);
    state_ = s.value("state").toByteArray();
    state_loaded_ = true;
    s.endGroup();
  }

  if (!header_->RestoreState(state_)) {
    // Maybe we're upgrading from a version that persisted the state with QHeaderView.
    if (!header_->restoreState(state_)) {
      header_->HideSection(Playlist::Column_AlbumArtist);
      header_->HideSection(Playlist::Column_Performer);
      header_->HideSection(Playlist::Column_Composer);
      header_->HideSection(Playlist::Column_Year);
      header_->HideSection(Playlist::Column_OriginalYear);
      header_->HideSection(Playlist::Column_Disc);
      header_->HideSection(Playlist::Column_Genre);
      header_->HideSection(Playlist::Column_Filename);
      header_->HideSection(Playlist::Column_BaseFilename);
      header_->HideSection(Playlist::Column_Filesize);
      header_->HideSection(Playlist::Column_DateCreated);
      header_->HideSection(Playlist::Column_DateModified);
      header_->HideSection(Playlist::Column_PlayCount);
      header_->HideSection(Playlist::Column_SkipCount);
      header_->HideSection(Playlist::Column_LastPlayed);
      header_->HideSection(Playlist::Column_Comment);
      header_->HideSection(Playlist::Column_Grouping);
      header_->HideSection(Playlist::Column_Mood);

      header_->moveSection(header_->visualIndex(Playlist::Column_Track), 0);
      setting_initial_header_layout_ = true;
    }
    else {
      setting_initial_header_layout_ = true;
    }
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
    header_->ShowSection(Playlist::Column_Title);
  }

}

void PlaylistView::SaveGeometry() {

  if (!initialized_ || !state_loaded_) return;
  state_ = header_->SaveState();

}

void PlaylistView::ReloadBarPixmaps() {

  currenttrack_bar_left_ = LoadBarPixmap(":/pictures/currenttrack_bar_left.png");
  currenttrack_bar_mid_ = LoadBarPixmap(":/pictures/currenttrack_bar_mid.png");
  currenttrack_bar_right_ = LoadBarPixmap(":/pictures/currenttrack_bar_right.png");

}

QList<QPixmap> PlaylistView::LoadBarPixmap(const QString &filename) {

  QImage image(filename);
  image = image.scaledToHeight(row_height_, Qt::SmoothTransformation);

  // Colour the bar with the palette colour
  QPainter p(&image);
  p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
  p.setOpacity(0.7);
  p.fillRect(image.rect(), QApplication::palette().color(QPalette::Highlight));
  p.end();

  // Animation steps
  QList<QPixmap> ret;
  for (int i = 0; i < kGlowIntensitySteps; ++i) {
    QImage step(image.copy());
    p.begin(&step);
    p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    p.setOpacity(0.4 - 0.6 * sin(float(i) / kGlowIntensitySteps * (M_PI / 2)));
    p.fillRect(step.rect(), Qt::white);
    p.end();
    ret << QPixmap::fromImage(step);
  }

  return ret;

}

void PlaylistView::drawTree(QPainter *painter, const QRegion &region) const {

  const_cast<PlaylistView*>(this)->current_paint_region_ = region;
  QTreeView::drawTree(painter, region);
  const_cast<PlaylistView*>(this)->current_paint_region_ = QRegion();

}

void PlaylistView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

  QStyleOptionViewItem opt(option);

  bool is_current = index.data(Playlist::Role_IsCurrent).toBool();
  bool is_paused = index.data(Playlist::Role_IsPaused).toBool();

  if (is_current) {
    const_cast<PlaylistView*>(this)->last_current_item_ = index;
    const_cast<PlaylistView*>(this)->last_glow_rect_ = opt.rect;

    int step = glow_intensity_step_;
    if (step >= kGlowIntensitySteps)
      step = 2 * (kGlowIntensitySteps - 1) - step + 1;

    int row_height = opt.rect.height();
    if (row_height != row_height_) {
      // Recreate the pixmaps if the height changed since last time
      const_cast<PlaylistView*>(this)->row_height_ = row_height;
      const_cast<PlaylistView*>(this)->ReloadBarPixmaps();
    }

    QRect middle(opt.rect);
    middle.setLeft(middle.left() + currenttrack_bar_left_[0].width());
    middle.setRight(middle.right() - currenttrack_bar_right_[0].width());

    // Selection
    if (selectionModel()->isSelected(index))
      painter->fillRect(opt.rect, opt.palette.color(QPalette::Highlight));

    // Draw the bar
    painter->drawPixmap(opt.rect.topLeft(), currenttrack_bar_left_[step]);
    painter->drawPixmap(opt.rect.topRight() - currenttrack_bar_right_[0].rect().topRight(), currenttrack_bar_right_[step]);
    painter->drawPixmap(middle, currenttrack_bar_mid_[step]);

    // Draw the play icon
    QPoint play_pos(currenttrack_bar_left_[0].width() / 3 * 2, (row_height - currenttrack_play_.height()) / 2);
    painter->drawPixmap(opt.rect.topLeft() + play_pos, is_paused ? currenttrack_pause_ : currenttrack_play_);

    // Set the font
    opt.palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QApplication::palette().color(QPalette::Active, QPalette::HighlightedText));
    opt.palette.setColor(QPalette::Text, QApplication::palette().color(QPalette::HighlightedText));
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    opt.palette.setColor(QPalette::AlternateBase, Qt::transparent);
    opt.decorationSize = QSize(20, 20);

    // Draw the actual row data on top.  We cache this, because it's fairly expensive (1-2ms), and we do it many times per second.
    const bool cache_dirty = cached_current_row_rect_ != opt.rect || cached_current_row_row_ != index.row() || cached_current_row_.isNull();

    // We can't update the cache if we're not drawing the entire region,
    // QTreeView clips its drawing to only the columns in the region, so it wouldn't update the whole pixmap properly.
    const bool whole_region = current_paint_region_.boundingRect().width() == viewport()->width();

    if (!cache_dirty) {
      painter->drawPixmap(opt.rect, cached_current_row_);
    }
    else {
      if (whole_region) {
        const_cast<PlaylistView*>(this)->UpdateCachedCurrentRowPixmap(opt, index);
        painter->drawPixmap(opt.rect, cached_current_row_);
      }
      else {
        QTreeView::drawRow(painter, opt, index);
      }
    }
  }
  else {
    QTreeView::drawRow(painter, opt, index);
  }

}

void PlaylistView::UpdateCachedCurrentRowPixmap(QStyleOptionViewItem option, const QModelIndex &index) {

  cached_current_row_rect_ = option.rect;
  cached_current_row_row_ = index.row();

  option.rect.moveTo(0, 0);
  cached_current_row_ = QPixmap(option.rect.size());
  cached_current_row_.fill(Qt::transparent);

  QPainter p(&cached_current_row_);
  QTreeView::drawRow(&p, option, index);

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
  if (isVisible() && glow_enabled_)
    glow_timer_.start(1500 / kGlowIntensitySteps, this);

}

void PlaylistView::hideEvent(QHideEvent *) { glow_timer_.stop(); }

void PlaylistView::showEvent(QShowEvent *) {

  if (currently_glowing_ && glow_enabled_)
    glow_timer_.start(1500 / kGlowIntensitySteps, this);

  MaybeAutoscroll();

}

bool CompareSelectionRanges(const QItemSelectionRange &a, const QItemSelectionRange &b) {
  return b.bottom() < a.bottom();
}

void PlaylistView::keyPressEvent(QKeyEvent *event) {

  if (!model() || state() == QAbstractItemView::EditingState) {
    QTreeView::keyPressEvent(event);
  }
  else if (event == QKeySequence::Delete) {
    RemoveSelected(false);
    event->accept();
#ifdef Q_OS_MACOS
  }
  else if (event->key() == Qt::Key_Backspace) {
    RemoveSelected(false);
    event->accept();
#endif
  }
  else if (event == QKeySequence::Copy) {
    CopyCurrentSongToClipboard();
  }
  else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
    if (currentIndex().isValid()) emit PlayItem(currentIndex());
    event->accept();
  }
  else if (event->modifiers() != Qt::ControlModifier && event->key() == Qt::Key_Space) {
    emit PlayPause();
    event->accept();
  }
  else if (event->key() == Qt::Key_Left) {
    emit SeekBackward();
    event->accept();
  }
  else if (event->key() == Qt::Key_Right) {
    emit SeekForward();
    event->accept();
  }
  else if (event->modifiers() == Qt::NoModifier &&  ((event->key() >= Qt::Key_Exclam && event->key() <= Qt::Key_Z) || event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Escape)) {
    emit FocusOnFilterSignal(event);
    event->accept();
  }
  else {
    QTreeView::keyPressEvent(event);
  }

}

void PlaylistView::contextMenuEvent(QContextMenuEvent *e) {
  emit RightClicked(e->globalPos(), indexAt(e->pos()));
  e->accept();
}

void PlaylistView::RemoveSelected(bool deleting_from_disk) {

  int rows_removed = 0;
  QItemSelection selection(selectionModel()->selection());

  if (selection.isEmpty()) {
    return;
  }

  // Store the last selected row, which is the last in the list
  int last_row = selection.last().top();

  // Sort the selection so we remove the items at the *bottom* first, ensuring we don't have to mess around with changing row numbers
  std::sort(selection.begin(), selection.end(), CompareSelectionRanges);

  for (const QItemSelectionRange &range : selection) {
    if (range.top() < last_row) rows_removed += range.height();

    if (!deleting_from_disk) {
      model()->removeRows(range.top(), range.height(), range.topLeft());
    }
    else {
      model()->removeRows(range.top(), range.height(), QModelIndex());
    }
  }

  int new_row = last_row - rows_removed;
  // Index of the first column for the row to select
  QModelIndex new_index = model()->index(new_row, 0);

  // Select the new current item, we want always the item after the last selected
  if (new_index.isValid()) {
    // Workaround to update keyboard selected row, if it's not the first row (this also triggers selection)
    if (new_row != 0)
      keyPressEvent(new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier));
    // Update visual selection with the entire row
    selectionModel()->select(new_index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
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
    QModelIndex index = model()->index(0, col);
    if (index.flags() & Qt::ItemIsEditable) columns << h->visualIndex(col);
  }
  std::sort(columns.begin(), columns.end());
  return columns;

}

QModelIndex PlaylistView::NextEditableIndex(const QModelIndex &current) {

  QList<int> columns = GetEditableColumns();
  QHeaderView *h = header();
  int index = columns.indexOf(h->visualIndex(current.column()));

  if (index + 1 >= columns.size())
    return model()->index(current.row() + 1, h->logicalIndex(columns.first()));

  return model()->index(current.row(), h->logicalIndex(columns[index + 1]));

}

QModelIndex PlaylistView::PrevEditableIndex(const QModelIndex &current) {

  QList<int> columns = GetEditableColumns();
  QHeaderView *h = header();
  int index = columns.indexOf(h->visualIndex(current.column()));

  if (index - 1 < 0)
    return model()->index(current.row() - 1, h->logicalIndex(columns.last()));

  return model()->index(current.row(), h->logicalIndex(columns[index - 1]));

}

void PlaylistView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) {

  if (hint == QAbstractItemDelegate::NoHint) {
    QTreeView::closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
  }
  else if (hint == QAbstractItemDelegate::EditNextItem || hint == QAbstractItemDelegate::EditPreviousItem) {

    QModelIndex index;
    if (hint == QAbstractItemDelegate::EditNextItem)
      index = NextEditableIndex(currentIndex());
    else
      index = PrevEditableIndex(currentIndex());

    if (!index.isValid()) {
      QTreeView::closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
    }
    else {
      QTreeView::closeEditor(editor, QAbstractItemDelegate::NoHint);
      setCurrentIndex(index);
      edit(index);
    }
  }
  else {
    QTreeView::closeEditor(editor, hint);
  }

}

void PlaylistView::mouseMoveEvent(QMouseEvent *event) {

  if (!drag_over_) {
    QTreeView::mouseMoveEvent(event);
  }

}

void PlaylistView::leaveEvent(QEvent *e) {

  QTreeView::leaveEvent(e);

}

void PlaylistView::mousePressEvent(QMouseEvent *event) {

  if (editTriggers() & QAbstractItemView::NoEditTriggers) {
    QTreeView::mousePressEvent(event);
    return;
  }

  QModelIndex idx = indexAt(event->pos());
  if (event->button() == Qt::XButton1 && idx.isValid()) {
    app_->player()->Previous();
  }
  else if (event->button() == Qt::XButton2 && idx.isValid()) {
    app_->player()->Next();
  }
  else {
    QTreeView::mousePressEvent(event);
  }

  inhibit_autoscroll_ = true;
  inhibit_autoscroll_timer_->start();

}

void PlaylistView::scrollContentsBy(int dx, int dy) {

  if (dx) {
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

void PlaylistView::MaybeAutoscroll() {
  if (!inhibit_autoscroll_) JumpToCurrentlyPlayingTrack();
}

void PlaylistView::JumpToCurrentlyPlayingTrack() {

  Q_ASSERT(playlist_);

  // Usage of the "Jump to the currently playing track" action shall enable autoscroll
  inhibit_autoscroll_ = false;

  if (playlist_->current_row() == -1) return;

  QModelIndex current = playlist_->proxy()->mapFromSource(playlist_->index(playlist_->current_row(), 0));
  if (!current.isValid()) return;

  currently_autoscrolling_ = true;

  // Scroll to the item
  scrollTo(current, QAbstractItemView::PositionAtCenter);

  currently_autoscrolling_ = false;

}

void PlaylistView::JumpToLastPlayedTrack() {

  Q_ASSERT(playlist_);

  if (playlist_->last_played_row() == -1) return;

  QModelIndex last_played = playlist_->proxy()->mapFromSource(playlist_->index(playlist_->last_played_row(), 0));
  if (!last_played.isValid()) return;

  // Select last played song
  last_current_item_ = last_played;
  setCurrentIndex(last_current_item_);

  currently_autoscrolling_ = true;

  // Scroll to the item
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
  if (background_image_type_ == AppearanceSettingsPage::BackgroundImageType_Custom || background_image_type_ == AppearanceSettingsPage::BackgroundImageType_Album) {
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
              if (background_image_do_not_cut_){
                cached_scaled_background_image_ = QPixmap::fromImage(background_image_.scaled(pb_width, pb_height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
              }
              else {
                if (pb_height >= pb_width){
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
        if (!qFuzzyCompare(previous_background_image_opacity_, qreal(0.0))) {
          background_painter.setOpacity(1.0 - previous_background_image_opacity_);
        }
        switch (background_image_position_) {
          case AppearanceSettingsPage::BackgroundImagePosition_UpperLeft:
            current_background_image_x_ = 0;
            current_background_image_y_ = 0;
            break;
          case AppearanceSettingsPage::BackgroundImagePosition_UpperRight:
            current_background_image_x_ = (pb_width - cached_scaled_background_image_.width());
            current_background_image_y_ = 0;
            break;
          case AppearanceSettingsPage::BackgroundImagePosition_Middle:
            current_background_image_x_ = ((pb_width - cached_scaled_background_image_.width()) / 2);
            current_background_image_y_ = ((pb_height - cached_scaled_background_image_.height()) / 2);
            break;
          case AppearanceSettingsPage::BackgroundImagePosition_BottomLeft:
            current_background_image_x_ = 0;
            current_background_image_y_ = (pb_height - cached_scaled_background_image_.height());
            break;
          case AppearanceSettingsPage::BackgroundImagePosition_BottomRight:
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
      cached_tree_ = QPixmap(size());
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
      if (model()->rowCount() == 0)
        drop_pos = 1;
      else
        drop_pos = 1 + visualRect(model()->index(model()->rowCount() - 1, first_column)).bottom();
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

}

void PlaylistView::dragMoveEvent(QDragMoveEvent *event) {

  QTreeView::dragMoveEvent(event);

  QModelIndex index(indexAt(event->pos()));
  drop_indicator_row_ = index.isValid() ? index.row() : 0;

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

  QSettings s;

  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
#ifdef Q_OS_MACOS
  bool glow_effect = false;
#else
  bool glow_effect = true;
#endif
  glow_enabled_ = s.value("glow_effect", glow_effect).toBool();
  bool editmetadatainline = s.value("editmetadatainline", false).toBool();
  s.endGroup();

  s.beginGroup(Playlist::kSettingsGroup);
  bool stretch = s.value("stretch", true).toBool();
  column_alignment_ = s.value("column_alignments").value<ColumnAlignmentMap>();
  s.endGroup();

  s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  QVariant background_image_type_var = s.value(AppearanceSettingsPage::kBackgroundImageType);
  QVariant background_image_position_var = s.value(AppearanceSettingsPage::kBackgroundImagePosition);
  int background_image_maxsize = s.value(AppearanceSettingsPage::kBackgroundImageMaxSize).toInt();
  if (background_image_maxsize <= 10) background_image_maxsize = 9000;
  QString background_image_filename = s.value(AppearanceSettingsPage::kBackgroundImageFilename).toString();
  bool background_image_stretch = s.value(AppearanceSettingsPage::kBackgroundImageStretch, false).toBool();
  bool background_image_do_not_cut = s.value(AppearanceSettingsPage::kBackgroundImageDoNotCut, true).toBool();
  bool background_image_keep_aspect_ratio = s.value(AppearanceSettingsPage::kBackgroundImageKeepAspectRatio, true).toBool();
  int blur_radius = s.value(AppearanceSettingsPage::kBlurRadius, AppearanceSettingsPage::kDefaultBlurRadius).toInt();
  int opacity_level = s.value(AppearanceSettingsPage::kOpacityLevel, AppearanceSettingsPage::kDefaultOpacityLevel).toInt();
  s.endGroup();

  if (setting_initial_header_layout_) {

    header_->SetStretchEnabled(stretch);

    header_->SetColumnWidth(Playlist::Column_Track, 0.02);
    header_->SetColumnWidth(Playlist::Column_Title, 0.16);
    header_->SetColumnWidth(Playlist::Column_Artist, 0.12);
    header_->SetColumnWidth(Playlist::Column_Album, 0.12);
    header_->SetColumnWidth(Playlist::Column_Length, 0.03);
    header_->SetColumnWidth(Playlist::Column_Samplerate, 0.07);
    header_->SetColumnWidth(Playlist::Column_Bitdepth, 0.07);
    header_->SetColumnWidth(Playlist::Column_Bitrate, 0.07);
    header_->SetColumnWidth(Playlist::Column_Filetype, 0.06);
    header_->SetColumnWidth(Playlist::Column_Source, 0.06);

    setting_initial_header_layout_ = false;

  }

  if (currently_glowing_ && glow_enabled_ && isVisible()) StartGlowing();
  if (!glow_enabled_) StopGlowing();

  if (column_alignment_.isEmpty()) {
    column_alignment_ = DefaultColumnAlignment();
  }
  emit ColumnAlignmentChanged(column_alignment_);


  // Background:
  AppearanceSettingsPage::BackgroundImageType background_image_type(AppearanceSettingsPage::BackgroundImageType_Default);
  if (background_image_type_var.isValid()) {
    background_image_type = static_cast<AppearanceSettingsPage::BackgroundImageType>(background_image_type_var.toInt());
  }
  else {
    background_image_type = AppearanceSettingsPage::BackgroundImageType_Default;
  }

  AppearanceSettingsPage::BackgroundImagePosition background_image_position(AppearanceSettingsPage::BackgroundImagePosition_BottomRight);
  if (background_image_position_var.isValid()) {
    background_image_position = static_cast<AppearanceSettingsPage::BackgroundImagePosition>(background_image_position_var.toInt());
  }
  else {
    background_image_position = AppearanceSettingsPage::BackgroundImagePosition_BottomRight;
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

    if (background_image_type_ == AppearanceSettingsPage::BackgroundImageType_Custom) {
      set_background_image(QImage(background_image_filename));
    }
    else if (background_image_type_ == AppearanceSettingsPage::BackgroundImageType_Album) {
      set_background_image(current_song_cover_art_);
    }
    else {
      // User changed background image type to something that will not be painted through paintEvent: reset all background images.
      // This avoid to use old (deprecated) images for fading when selecting Album or Custom background image type later.
      set_background_image(QImage());
      cached_scaled_background_image_ = QPixmap();
      previous_background_image_ = QPixmap();
    }
    setProperty("default_background_enabled", background_image_type_ == AppearanceSettingsPage::BackgroundImageType_Default);
    emit BackgroundPropertyChanged();
    force_background_redraw_ = true;
  }

  if (editmetadatainline)
    setEditTriggers(editTriggers() | QAbstractItemView::SelectedClicked);
  else
    setEditTriggers(editTriggers() & ~QAbstractItemView::SelectedClicked);

}

void PlaylistView::SaveSettings() {

  if (!initialized_ || read_only_settings_) return;

  QSettings s;
  s.beginGroup(Playlist::kSettingsGroup);
  s.setValue("state", header_->SaveState());
  s.setValue("column_alignments", QVariant::fromValue(column_alignment_));
  s.endGroup();

}

void PlaylistView::StretchChanged(bool stretch) {

  if (!initialized_) return;
  setHorizontalScrollBarPolicy(stretch ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
  SaveGeometry();

}

bool PlaylistView::eventFilter(QObject *object, QEvent *event) {

  if (event->type() == QEvent::Enter && (object == horizontalScrollBar() || object == verticalScrollBar())) {
    return false;
  }
  return QObject::eventFilter(object, event);

}

void PlaylistView::rowsInserted(const QModelIndex &parent, int start, int end) {

  const bool at_end = end == model()->rowCount(parent) - 1;

  QTreeView::rowsInserted(parent, start, end);

  if (at_end) {
    // If the rows were inserted at the end of the playlist then let's scroll the view so the user can see.
    scrollTo(model()->index(start, 0, parent), QAbstractItemView::PositionAtTop);
  }

}

ColumnAlignmentMap PlaylistView::DefaultColumnAlignment() {

  ColumnAlignmentMap ret;

  ret[Playlist::Column_Year] =
  ret[Playlist::Column_OriginalYear] =
  ret[Playlist::Column_Track] =
  ret[Playlist::Column_Disc] =
  ret[Playlist::Column_Length] =
  ret[Playlist::Column_Samplerate] =
  ret[Playlist::Column_Bitdepth] =
  ret[Playlist::Column_Bitrate] =
  ret[Playlist::Column_Filesize] =
  ret[Playlist::Column_PlayCount] =
  ret[Playlist::Column_SkipCount] =
 (Qt::AlignRight | Qt::AlignVCenter);

  return ret;

}

void PlaylistView::SetColumnAlignment(int section, Qt::Alignment alignment) {

  if (section < 0) return;

  column_alignment_[section] = alignment;
  emit ColumnAlignmentChanged(column_alignment_);
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

    const QVariant data = model()->data(currentIndex().sibling(currentIndex().row(), i));
    if (data.type() == QVariant::String) {
      columns << data.toString();
    }
  }

  // Get the song's URL
  const QUrl url = model()->data(currentIndex().sibling(currentIndex().row(), Playlist::Column_Filename)).toUrl();

  QMimeData *mime_data = new QMimeData;
  mime_data->setUrls(QList<QUrl>() << url);
  mime_data->setText(columns.join(" - "));

  QApplication::clipboard()->setMimeData(mime_data);

}

void PlaylistView::SongChanged(const Song &song) {
  song_playing_ = song;
}

void PlaylistView::Playing() {}

void PlaylistView::Stopped() {

  if (song_playing_ == Song()) return;
  song_playing_ = Song();
  StopGlowing();
  AlbumCoverLoaded(Song(), QUrl(), QImage());

}

void PlaylistView::AlbumCoverLoaded(const Song &song, const QUrl &cover_url, const QImage &song_art) {

  if ((song != Song() && song_playing_ == Song()) || song_art == current_song_cover_art_) return;

  current_song_cover_art_ = song_art;
  if (background_image_type_ == AppearanceSettingsPage::BackgroundImageType_Album) {
    if (song.art_automatic().isEmpty() && song.art_manual().isEmpty()) {
      set_background_image(QImage());
    }
    else {
      set_background_image(current_song_cover_art_);
    }
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
    // Apply opacity filter
    uchar *bits = background_image_.bits();
    for (int i = 0 ; i < background_image_.height() * background_image_.bytesPerLine() ; i += 4) {
      bits[i + 3] = (opacity_level_ / 100.0) * 255;
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
    if (fade_animation_->state() == QTimeLine::Running) fade_animation_->stop();
    fade_animation_->start();
  }

}

void PlaylistView::FadePreviousBackgroundImage(qreal value) {

  previous_background_image_opacity_ = value;
  if (qFuzzyCompare(previous_background_image_opacity_, qreal(0.0))) {
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

void PlaylistView::ResetColumns() {

  QSettings s;
  s.beginGroup(Playlist::kSettingsGroup);
  s.remove("state");
  s.endGroup();
  state_loaded_ = false;
  read_only_settings_ = true;
  setting_initial_header_layout_ = true;
  ReloadSettings();
  LoadGeometry();
  read_only_settings_ = false;
  SetPlaylist(playlist_);

}
