/*
  This file is part of KDSingleApplication.

  SPDX-FileCopyrightText: 2019-2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/
#ifndef KDSINGLEAPPLICATION_LIB_H
#define KDSINGLEAPPLICATION_LIB_H

#include <QtCore/QtGlobal>

#if defined(KDSINGLEAPPLICATION_STATIC_BUILD)
#define KDSINGLEAPPLICATION_EXPORT
#elif defined(KDSINGLEAPPLICATION_SHARED_BUILD)
#define KDSINGLEAPPLICATION_EXPORT Q_DECL_EXPORT
#else
#define KDSINGLEAPPLICATION_EXPORT Q_DECL_IMPORT
#endif

#endif // KDSINGLEAPPLICATION_LIB_H
