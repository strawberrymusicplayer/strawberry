# KDSingleApplication

`KDSingleApplication` is a helper class for single-instance policy applications
written by [KDAB](https://www.kdab.com).

## Usage

Currently the documentation is woefully lacking, but see the examples or tests
for inspiration. Basically it involves:

1. Create a `Q(Core|Gui)Application` object.
2. Create a `KDSingleApplication` object.
3. Check if the current instance is *primary* (or "master") or
   *secondary* (or "slave") by calling `isPrimaryInstance`:
    * the *primary* instance needs to listen from messages coming from the
      secondary instances, by connecting a slot to the `messageReceived` signal;
    * the *secondary* instances can send messages to the primary instance
      by calling `sendMessage`.

## Licensing

KDSingleApplication is (C) 2019-2023, Klarälvdalens Datakonsult AB, and is available
under the terms of the [MIT license](LICENSES/MIT.txt).

Contact KDAB at <info@kdab.com> if you need different licensing options.

## Get Involved

KDAB will happily accept external contributions.

Please submit your contributions or issue reports from our GitHub space at
<https://github.com/KDAB/KDSingleApplication>.

## About KDAB

KDSingleApplication is supported and maintained by Klarälvdalens Datakonsult AB (KDAB).

The KDAB Group is the global No.1 software consultancy for Qt, C++ and
OpenGL applications across desktop, embedded and mobile platforms.

The KDAB Group provides consulting and mentoring for developing Qt applications
from scratch and in porting from all popular and legacy frameworks to Qt.
We continue to help develop parts of Qt and are one of the major contributors
to the Qt Project. We can give advanced or standard trainings anywhere
around the globe on Qt as well as C++, OpenGL, 3D and more.

Please visit <https://www.kdab.com> to meet the people who write code like this.

Stay up-to-date with KDAB product announcements:

* [KDAB Newsletter](https://news.kdab.com)
* [KDAB Blogs](https://www.kdab.com/category/blogs)
* [KDAB on Twitter](https://twitter.com/KDABQt)
