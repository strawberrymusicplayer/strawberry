/* This file is part of Strawberry.
   Copyright 2011, David Sansome <me@davidsansome.com>
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <QtGlobal>

#include <iostream>

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QLocalSocket>

#include "core/logging.h"
#include "tagreaderworker.h"

int main(int argc, char **argv) {

  QCoreApplication a(argc, argv);
  QStringList args(a.arguments());

  if (args.count() != 2) {
    std::cerr << "This program is used internally by Strawberry to parse tags in music files\n"
                 "without exposing the whole application to crashes caused by malformed\n"
                 "files.  It is not meant to be run on its own.\n";
    return 1;
  }

  logging::Init();
  qLog(Info) << "TagReader worker connecting to" << args[1];

  // Connect to the parent process.
  QLocalSocket socket;
  socket.connectToServer(args[1]);
  if (!socket.waitForConnected(2000)) {
    std::cerr << "Failed to connect to the parent process.\n";
    return 1;
  }

  TagReaderWorker worker(&socket);

  return a.exec();
  
}
