/*
  This file is part of KDSingleApplication.

  SPDX-FileCopyrightText: 2019-2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/
#ifndef KDSINGLEAPPLICATION_H
#define KDSINGLEAPPLICATION_H

#include <QtCore/QObject>

#include <memory>

#include "kdsingleapplication_lib.h"

class KDSingleApplicationPrivate;

class KDSINGLEAPPLICATION_EXPORT KDSingleApplication : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool isPrimaryInstance READ isPrimaryInstance CONSTANT)

public:
    explicit KDSingleApplication(QObject *parent = nullptr);
    explicit KDSingleApplication(const QString &name, QObject *parent = nullptr);
    ~KDSingleApplication();

    QString name() const;
    bool isPrimaryInstance() const;

public Q_SLOTS:
    // avoid default arguments and overloads, as they don't mix with connections
    bool sendMessage(const QByteArray &message);
    bool sendMessageWithTimeout(const QByteArray &message, int timeout);

Q_SIGNALS:
    void messageReceived(const QByteArray &message);

private:
    Q_DECLARE_PRIVATE(KDSingleApplication)
    std::unique_ptr<KDSingleApplicationPrivate> d_ptr;
};

#endif // KDSINGLEAPPLICATION_H
