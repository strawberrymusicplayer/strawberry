#ifndef COLLECTIONLETTERGROUPS_ANDROID_H
#define COLLECTIONLETTERGROUPS_ANDROID_H

#include <QObject>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>

class QAbstractItemModel;
class QModelIndex;
class QTreeView;
class QTimer;

// Dividers are siblings of their artists in CollectionModel, not parents.
class CollectionLetterGroups : public QObject {
 public:
  explicit CollectionLetterGroups(QTreeView *view);

  void SetEnabled(bool enabled);
  bool enabled() const { return enabled_; }
  bool IsCollapsed(const QModelIndex &index) const;

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 private:
  void BindModel();
  void ApplyRows();
  void Reveal(const QModelIndex &index);

  QTreeView *view_;
  QPointer<QAbstractItemModel> model_;
  QTimer *update_timer_;
  QList<QMetaObject::Connection> connections_;
  QSet<QString> collapsed_;
  bool enabled_ = false;
};

#endif  // COLLECTIONLETTERGROUPS_ANDROID_H
