// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectionview.h"

#include "collectionfilter.h"
#include "collectionmanager.h"
#include "collectionselection_android.h"

QWidget *CollectionView::CreateAndroidSelectionToolbar(QWidget *parent) {

  selection_ = new CollectionSelection(this, parent);
  QObject::connect(selection_, &CollectionSelection::AddToPlaylist, this, &CollectionView::AddToPlaylistSignal);
  QObject::connect(selection_, &CollectionSelection::Error, this, &CollectionView::Error);
  QObject::connect(selection_, &CollectionSelection::RescanSongs, this, [this](const QModelIndexList &indexes) {
    const auto *filter = qobject_cast<const CollectionFilter*>(model());
    if (!filter || !collection_) {
      Q_EMIT Error(tr("Could not rescan the selected music: the collection is not available."));
      return;
    }
    collection_->Rescan(filter->GetChildSongs(indexes));
  });
  return selection_->toolbar();

}

bool CollectionView::AndroidSelectionBack() {
  return selection_ && selection_->Back();
}
