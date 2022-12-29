/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#undef QT_NO_DEBUG_OUTPUT
#undef QT_NO_WARNING_OUTPUT
#undef QT_NO_INFO_OUTPUT

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <iostream>
#include <utility>
#include <QProcess>
#include <QDir>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QVariantMap>
#include <QStack>
#include <QDirIterator>
#include <QLibraryInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include "shared.h"

#ifdef Q_OS_DARWIN
#include <CoreFoundation/CoreFoundation.h>
#endif

bool runStripEnabled = true;
bool alwaysOwerwriteEnabled = false;
bool runCodesign = false;
QStringList librarySearchPath;
QString codesignIdentiy;
QString extraEntitlements;
bool hardenedRuntime = false;
bool secureTimestamp = false;
bool appstoreCompliant = false;
int logLevel = 1;
bool deployFramework = false;

using std::cout;
using std::endl;

bool operator==(const FrameworkInfo &a, const FrameworkInfo &b)
{
    return ((a.frameworkPath == b.frameworkPath) && (a.binaryPath == b.binaryPath));
}

QDebug operator<<(QDebug debug, const FrameworkInfo &info)
{
    debug << "Framework name" << info.frameworkName << "\n";
    debug << "Framework directory" << info.frameworkDirectory << "\n";
    debug << "Framework path" << info.frameworkPath << "\n";
    debug << "Binary directory" << info.binaryDirectory << "\n";
    debug << "Binary name" << info.binaryName << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Version" << info.version << "\n";
    debug << "Install name" << info.installName << "\n";
    debug << "Deployed install name" << info.deployedInstallName << "\n";
    debug << "Source file Path" << info.sourceFilePath << "\n";
    debug << "Framework Destination Directory (relative to bundle)" << info.frameworkDestinationDirectory << "\n";
    debug << "Binary Destination Directory (relative to bundle)" << info.binaryDestinationDirectory << "\n";

    return debug;
}

const QString bundleFrameworkDirectory = "Contents/Frameworks";

inline QDebug operator<<(QDebug debug, const ApplicationBundleInfo &info)
{
    debug << "Application bundle path" << info.path << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Additional libraries" << info.libraryPaths << "\n";
    return debug;
}

bool copyFilePrintStatus(const QString &from, const QString &to)
{
    if (QFile::exists(to)) {
        if (alwaysOwerwriteEnabled) {
            QFile(to).remove();
        } else {
            qDebug() << "File exists, skip copy:" << to;
            return false;
        }
    }

    if (QFile::copy(from, to)) {
        QFile dest(to);
        dest.setPermissions(dest.permissions() | QFile::WriteOwner | QFile::WriteUser);
        LogNormal() << " copied:" << from;
        LogNormal() << " to" << to;

        // The source file might not have write permissions set. Set the
        // write permission on the target file to make sure we can use
        // install_name_tool on it later.
        QFile toFile(to);
        if (toFile.permissions() & QFile::WriteOwner)
            return true;

        if (!toFile.setPermissions(toFile.permissions() | QFile::WriteOwner)) {
            LogError() << "Failed to set u+w permissions on target file: " << to;
            return false;
        }

        return true;
    } else {
        LogError() << "file copy failed from" << from;
        LogError() << " to" << to;
        return false;
    }
}

bool linkFilePrintStatus(const QString &file, const QString &link)
{
    if (QFile::exists(link)) {
        if (QFile(link).symLinkTarget().isEmpty())
            LogError() << link << "exists but it's a file.";
        else
            LogNormal() << "Symlink exists, skipping:" << link;
        return false;
    } else if (QFile::link(file, link)) {
        LogNormal() << " symlink" << link;
        LogNormal() << " points to" << file;
        return true;
    } else {
        LogError() << "failed to symlink" << link;
        LogError() << " to" << file;
        return false;
    }
}

void patch_debugInInfoPlist(const QString &infoPlistPath)
{
    // Older versions of qmake may have the "_debug" binary as
    // the value for CFBundleExecutable. Remove it.
    QFile infoPlist(infoPlistPath);
    infoPlist.open(QIODevice::ReadOnly);
    QByteArray contents = infoPlist.readAll();
    infoPlist.close();
    infoPlist.open(QIODevice::WriteOnly | QIODevice::Truncate);
    contents.replace("_debug", ""); // surely there are no legit uses of "_debug" in an Info.plist
    infoPlist.write(contents);
}

OtoolInfo findDependencyInfo(const QString &binaryPath)
{
    OtoolInfo info;
    info.binaryPath = binaryPath;

    LogDebug() << "Using otool:";
    LogDebug() << " inspecting" << binaryPath;
    QProcess otool;
    otool.start("otool", QStringList() << "-L" << binaryPath);
    otool.waitForFinished();

    if (otool.exitStatus() != QProcess::NormalExit || otool.exitCode() != 0) {
        LogError() << otool.readAllStandardError();
        return info;
    }

    static const QRegularExpression regexp(QStringLiteral("^\\t(.+) \\(compatibility version (\\d+\\.\\d+\\.\\d+), current version (\\d+\\.\\d+\\.\\d+)(, weak|, reexport)?\\)$"));

    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n", Qt::SkipEmptyParts);
    if (outputLines.size() < 2) {
        LogError() << "Could not parse otool output:" << output;
        return info;
    }

    outputLines.removeFirst(); // remove line containing the binary path
    if (binaryPath.contains(".framework/") || binaryPath.endsWith(".dylib")) {
        const auto match = regexp.match(outputLines.first());
        if (match.hasMatch()) {
            QString installname = match.captured(1);
            if (QFileInfo(binaryPath).fileName() == QFileInfo(installname).fileName()) {
                info.installName = installname;
                info.compatibilityVersion = QVersionNumber::fromString(match.captured(2));
                info.currentVersion = QVersionNumber::fromString(match.captured(3));
                outputLines.removeFirst();
            } else {
                info.installName = binaryPath;
            }
        } else {
            LogError() << "Could not parse otool output line:" << outputLines.first();
            outputLines.removeFirst();
        }
    }

    for (const QString &outputLine : outputLines) {
        const auto match = regexp.match(outputLine);
        if (match.hasMatch()) {
            if (match.captured(1) == info.installName)
                continue; // Another arch reference to the same binary
            DylibInfo dylib;
            dylib.binaryPath = match.captured(1);
            dylib.compatibilityVersion = QVersionNumber::fromString(match.captured(2));
            dylib.currentVersion = QVersionNumber::fromString(match.captured(3));
            info.dependencies << dylib;
        } else {
            LogError() << "Could not parse otool output line:" << outputLine;
        }
    }

    return info;
}

FrameworkInfo parseOtoolLibraryLine(const QString &line, const QString &appBundlePath, const QList<QString> &rpaths, bool useDebugLibs)
{
    FrameworkInfo info;
    QString trimmed = line.trimmed();

    if (trimmed.isEmpty())
        return info;

    // Don't deploy system libraries.
    if (trimmed.startsWith("/System/Library/") ||
        (trimmed.startsWith("/usr/lib/") && trimmed.contains("libQt") == false) // exception for libQtuitools and libQtlucene
        || trimmed.startsWith("@executable_path") || trimmed.startsWith("@loader_path"))
        return info;

    // Resolve rpath relative libraries.
    if (trimmed.startsWith("@rpath/")) {
        QString rpathRelativePath = trimmed.mid(QStringLiteral("@rpath/").length());
        bool foundInsideBundle = false;
        for (const QString &rpath : std::as_const(rpaths)) {
            QString path = QDir::cleanPath(rpath + "/" + rpathRelativePath);
            // Skip paths already inside the bundle.
            if (!appBundlePath.isEmpty()) {
                if (QDir::isAbsolutePath(appBundlePath)) {
                    if (path.startsWith(QDir::cleanPath(appBundlePath) + "/")) {
                        foundInsideBundle = true;
                        continue;
                    }
                } else {
                    if (path.startsWith(QDir::cleanPath(QDir::currentPath() + "/" + appBundlePath) + "/")) {
                        foundInsideBundle = true;
                        continue;
                    }
                }
            }
            // Try again with substituted rpath.
            FrameworkInfo resolvedInfo = parseOtoolLibraryLine(path, appBundlePath, rpaths, useDebugLibs);
            if (!resolvedInfo.frameworkName.isEmpty() && QFile::exists(resolvedInfo.frameworkPath)) {
                resolvedInfo.rpathUsed = rpath;
                resolvedInfo.installName = trimmed;
                return resolvedInfo;
            }
        }
        if (!rpaths.isEmpty() && !foundInsideBundle) {
            LogError() << "Cannot resolve rpath" << trimmed;
            LogError() << " using" << rpaths;
        }
        return info;
    }

    enum State {QtPath, FrameworkName, DylibName, Version, FrameworkBinary, End};
    State state = QtPath;
    int part = 0;
    QString name;
    QString qtPath;
    QString suffix = useDebugLibs ? "_debug" : "";

    // Split the line into [Qt-path]/lib/qt[Module].framework/Versions/[Version]/
    QStringList parts = trimmed.split("/");
    while (part < parts.count()) {
        const QString currentPart = parts.at(part).simplified();
        ++part;
        if (currentPart == "")
            continue;

        if (state == QtPath) {
            // Check for library name part
            if (part < parts.count() && parts.at(part).contains(".dylib")) {
                info.frameworkDirectory += "/" + QString(qtPath + currentPart + "/").simplified();
                state = DylibName;
                continue;
            } else if (part < parts.count() && parts.at(part).endsWith(".framework")) {
                info.frameworkDirectory += "/" + QString(qtPath + "lib/").simplified();
                state = FrameworkName;
                continue;
            } else if (trimmed.startsWith("/") == false) {      // If the line does not contain a full path, the app is using a binary Qt package.
                QStringList partsCopy = parts;
                partsCopy.removeLast();
                for (QString &path : librarySearchPath) {
                    if (!path.endsWith("/"))
                        path += '/';
                    QString nameInPath = path + parts.join(QLatin1Char('/'));
                    if (QFile::exists(nameInPath)) {
                        info.frameworkDirectory = path + partsCopy.join(QLatin1Char('/'));
                        break;
                    }
                }
                if (currentPart.contains(".framework")) {
                    if (info.frameworkDirectory.isEmpty())
                        info.frameworkDirectory = "/Library/Frameworks/" + partsCopy.join(QLatin1Char('/'));
                    if (!info.frameworkDirectory.endsWith("/"))
                        info.frameworkDirectory += "/";
                    state = FrameworkName;
                    --part;
                    continue;
                } else if (currentPart.contains(".dylib")) {
                    if (info.frameworkDirectory.isEmpty())
                        info.frameworkDirectory = "/usr/lib/" + partsCopy.join(QLatin1Char('/'));
                    if (!info.frameworkDirectory.endsWith("/"))
                        info.frameworkDirectory += "/";
                    state = DylibName;
                    --part;
                    continue;
                }
            }
            qtPath += (currentPart + "/");

        } if (state == FrameworkName) {
            // remove ".framework"
            name = currentPart;
            name.chop(QString(".framework").length());
            info.isDylib = false;
            info.frameworkName = currentPart;
            state = Version;
            ++part;
            continue;
        } if (state == DylibName) {
            name = currentPart;
            info.isDylib = true;
            info.frameworkName = name;
            info.binaryName = name.contains(suffix) ? name : name.left(name.indexOf('.')) + suffix + name.mid(name.indexOf('.'));
            info.deployedInstallName = "@executable_path/../Frameworks/" + info.binaryName;
            info.frameworkPath = info.frameworkDirectory + info.binaryName;
            info.sourceFilePath = info.frameworkPath;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/";
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory;
            info.binaryDirectory = info.frameworkDirectory;
            info.binaryPath = info.frameworkPath;
            state = End;
            ++part;
            continue;
        } else if (state == Version) {
            info.version = currentPart;
            info.binaryDirectory = "Versions/" + info.version;
            info.frameworkPath = info.frameworkDirectory + info.frameworkName;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/" + info.frameworkName;
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory + "/" + info.binaryDirectory;
            state = FrameworkBinary;
        } else if (state == FrameworkBinary) {
            info.binaryName = currentPart.contains(suffix) ? currentPart : currentPart + suffix;
            info.binaryPath = "/" + info.binaryDirectory + "/" + info.binaryName;
            info.deployedInstallName = "@executable_path/../Frameworks/" + info.frameworkName + info.binaryPath;
            info.sourceFilePath = info.frameworkPath + info.binaryPath;
            state = End;
        } else if (state == End) {
            break;
        }
    }

    if (!info.sourceFilePath.isEmpty() && QFile::exists(info.sourceFilePath)) {
        info.installName = findDependencyInfo(info.sourceFilePath).installName;
        if (info.installName.startsWith("@rpath/"))
            info.deployedInstallName = info.installName;
    }

    return info;
}

QString findAppBinary(const QString &appBundlePath)
{
    QString binaryPath;

#ifdef Q_OS_DARWIN
    CFStringRef bundlePath = appBundlePath.toCFString();
    CFURLRef bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, bundlePath,
                                                       kCFURLPOSIXPathStyle, true);
    CFRelease(bundlePath);
    CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);
    if (bundle) {
        CFURLRef executableURL = CFBundleCopyExecutableURL(bundle);
        if (executableURL) {
            CFURLRef absoluteExecutableURL = CFURLCopyAbsoluteURL(executableURL);
            if (absoluteExecutableURL) {
                CFStringRef executablePath = CFURLCopyFileSystemPath(absoluteExecutableURL,
                                                                     kCFURLPOSIXPathStyle);
                if (executablePath) {
                    binaryPath = QString::fromCFString(executablePath);
                    CFRelease(executablePath);
                }
                CFRelease(absoluteExecutableURL);
            }
            CFRelease(executableURL);
        }
        CFRelease(bundle);
    }
    CFRelease(bundleURL);
#endif

    if (QFile::exists(binaryPath))
        return binaryPath;
    LogError() << "Could not find bundle binary for" << appBundlePath;
    return QString();
}

QStringList findAppFrameworkNames(const QString &appBundlePath)
{
    QStringList frameworks;

    // populate the frameworks list with QtFoo.framework etc,
    // as found in /Contents/Frameworks/
    QString searchPath = appBundlePath + "/Contents/Frameworks/";
    QDirIterator iter(searchPath, QStringList() << QString::fromLatin1("*.framework"),
                      QDir::Dirs | QDir::NoSymLinks);
    while (iter.hasNext()) {
        iter.next();
        frameworks << iter.fileInfo().fileName();
    }

    return frameworks;
}

QStringList findAppFrameworkPaths(const QString &appBundlePath)
{
    QStringList frameworks;
    QString searchPath = appBundlePath + "/Contents/Frameworks/";
    QDirIterator iter(searchPath, QStringList() << QString::fromLatin1("*.framework"),
                      QDir::Dirs | QDir::NoSymLinks);
    while (iter.hasNext()) {
        iter.next();
        frameworks << iter.fileInfo().filePath();
    }

    return frameworks;
}

QStringList findAppLibraries(const QString &appBundlePath)
{
    QStringList result;
    // dylibs
    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*.dylib"),
            QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        iter.next();
        result << iter.fileInfo().filePath();
    }
    return result;
}

QStringList findAppBundleFiles(const QString &appBundlePath, bool absolutePath = false)
{
    QStringList result;

    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        if (iter.fileInfo().isSymLink())
            continue;
        result << (absolutePath ? iter.fileInfo().absoluteFilePath() : iter.fileInfo().filePath());
    }

    return result;
}

QString findEntitlementsFile(const QString& path)
{
    QDirIterator iter(path, QStringList() << QString::fromLatin1("*.entitlements"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        if (iter.fileInfo().isSymLink())
            continue;

        //return the first entitlements file - only one is used for signing anyway
        return iter.fileInfo().absoluteFilePath();
    }

    return QString();
}

QList<FrameworkInfo> getQtFrameworks(const QList<DylibInfo> &dependencies, const QString &appBundlePath, const QList<QString> &rpaths, bool useDebugLibs)
{
    QList<FrameworkInfo> libraries;
    for (const DylibInfo &dylibInfo : dependencies) {
        FrameworkInfo info = parseOtoolLibraryLine(dylibInfo.binaryPath, appBundlePath, rpaths, useDebugLibs);
        if (info.frameworkName.isEmpty() == false) {
            LogDebug() << "Adding framework:";
            LogDebug() << info;
            libraries.append(info);
        }
    }
    return libraries;
}

QString resolveDyldPrefix(const QString &path, const QString &loaderPath, const QString &executablePath)
{
    if (path.startsWith("@")) {
        if (path.startsWith(QStringLiteral("@executable_path/"))) {
            // path relative to bundle executable dir
            if (QDir::isAbsolutePath(executablePath)) {
                return QDir::cleanPath(QFileInfo(executablePath).path() + path.mid(QStringLiteral("@executable_path").length()));
            } else {
                return QDir::cleanPath(QDir::currentPath() + "/" +
                                       QFileInfo(executablePath).path() + path.mid(QStringLiteral("@executable_path").length()));
            }
        } else if (path.startsWith(QStringLiteral("@loader_path"))) {
            // path relative to loader dir
            if (QDir::isAbsolutePath(loaderPath)) {
                return QDir::cleanPath(QFileInfo(loaderPath).path() + path.mid(QStringLiteral("@loader_path").length()));
            } else {
                return QDir::cleanPath(QDir::currentPath() + "/" +
                                       QFileInfo(loaderPath).path() + path.mid(QStringLiteral("@loader_path").length()));
            }
        } else {
          LogError() << "Unexpected prefix" << path;
        }
    }
    return path;
}

QList<QString> getBinaryRPaths(const QString &path, bool resolve = true, QString executablePath = QString())
{
    QList<QString> rpaths;

    QProcess otool;
    otool.start("otool", QStringList() << "-l" << path);
    otool.waitForFinished();

    if (otool.exitCode() != 0) {
        LogError() << otool.readAllStandardError();
    }

    if (resolve && executablePath.isEmpty()) {
      executablePath = path;
    }

    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n");

    for (auto i = outputLines.cbegin(), end = outputLines.cend(); i != end; ++i) {
        if (i->contains("cmd LC_RPATH") && ++i != end &&
            i->contains("cmdsize") && ++i != end) {
            const QString &rpathCmd = *i;
            int pathStart = rpathCmd.indexOf("path ");
            int pathEnd = rpathCmd.indexOf(" (");
            if (pathStart >= 0 && pathEnd >= 0 && pathStart < pathEnd) {
                QString rpath = rpathCmd.mid(pathStart + 5, pathEnd - pathStart - 5);
                if (resolve) {
                    rpaths << resolveDyldPrefix(rpath, path, executablePath);
                } else {
                    rpaths << rpath;
                }
            }
        }
    }

    return rpaths;
}

QList<FrameworkInfo> getQtFrameworks(const QString &path, const QString &appBundlePath, const QList<QString> &rpaths, bool useDebugLibs)
{
    const OtoolInfo info = findDependencyInfo(path);
    QList<QString> allRPaths = rpaths + getBinaryRPaths(path);
    allRPaths.removeDuplicates();
    return getQtFrameworks(info.dependencies, appBundlePath, allRPaths, useDebugLibs);
}

QList<FrameworkInfo> getQtFrameworksForPaths(const QStringList &paths, const QString &appBundlePath, const QList<QString> &rpaths, bool useDebugLibs)
{
    QList<FrameworkInfo> result;
    QSet<QString> existing;
    for (const QString &path : paths) {
        for (const FrameworkInfo &info : getQtFrameworks(path, appBundlePath, rpaths, useDebugLibs)) {
            if (!existing.contains(info.frameworkPath)) { // avoid duplicates
                existing.insert(info.frameworkPath);
                result << info;
            }
        }
    }
    return result;
}

QStringList getBinaryDependencies(const QString executablePath,
                                  const QString &path,
                                  const QList<QString> &additionalBinariesContainingRpaths)
{
    QStringList binaries;

    const auto dependencies = findDependencyInfo(path).dependencies;

    bool rpathsLoaded = false;
    QList<QString> rpaths;

    // return bundle-local dependencies. (those starting with @executable_path)
    for (const DylibInfo &info : dependencies) {
        QString trimmedLine = info.binaryPath;
        if (trimmedLine.startsWith("@executable_path/")) {
            QString binary = QDir::cleanPath(executablePath + trimmedLine.mid(QStringLiteral("@executable_path/").length()));
            if (binary != path)
                binaries.append(binary);
        } else if (trimmedLine.startsWith("@rpath/")) {
            if (!rpathsLoaded) {
                rpaths = getBinaryRPaths(path, true, executablePath);
                for (const QString &binaryPath : additionalBinariesContainingRpaths) {
                    rpaths += getBinaryRPaths(binaryPath, true);
                }
                rpaths.removeDuplicates();
                rpathsLoaded = true;
            }
            bool resolved = false;
            for (const QString &rpath : std::as_const(rpaths)) {
                QString binary = QDir::cleanPath(rpath + "/" + trimmedLine.mid(QStringLiteral("@rpath/").length()));
                LogDebug() << "Checking for" << binary;
                if (QFile::exists(binary)) {
                    binaries.append(binary);
                    resolved = true;
                    break;
                }
            }
            if (!resolved && !rpaths.isEmpty()) {
                LogError() << "Cannot resolve rpath" << trimmedLine;
                LogError() << " using" << rpaths;
            }
        }
    }

    return binaries;
}

// copies everything _inside_ sourcePath to destinationPath
bool recursiveCopy(const QString &sourcePath, const QString &destinationPath)
{
    if (!QDir(sourcePath).exists())
        return false;
    QDir().mkpath(destinationPath);

    LogNormal() << "copy:" << sourcePath << destinationPath;

    QStringList files = QDir(sourcePath).entryList(QStringList() << "*", QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &file : files) {
        const QString fileSourcePath = sourcePath + "/" + file;
        const QString fileDestinationPath = destinationPath + "/" + file;
        copyFilePrintStatus(fileSourcePath, fileDestinationPath);
    }

    QStringList subdirs = QDir(sourcePath).entryList(QStringList() << "*", QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dir : subdirs) {
        recursiveCopy(sourcePath + "/" + dir, destinationPath + "/" + dir);
    }
    return true;
}

void recursiveCopyAndDeploy(const QString &appBundlePath, const QList<QString> &rpaths, const QString &sourcePath, const QString &destinationPath)
{
    QDir().mkpath(destinationPath);

    LogNormal() << "copy:" << sourcePath << destinationPath;
    const bool isDwarfPath = sourcePath.endsWith("DWARF");

    QStringList files = QDir(sourcePath).entryList(QStringList() << QStringLiteral("*"), QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &file : files) {
        const QString fileSourcePath = sourcePath + QLatin1Char('/') + file;

        if (file.endsWith("_debug.dylib")) {
            continue; // Skip debug versions
        } else if (!isDwarfPath && file.endsWith(QStringLiteral(".dylib"))) {
            // App store code signing rules forbids code binaries in Contents/Resources/,
            // which poses a problem for deploying mixed .qml/.dylib Qt Quick imports.
            // Solve this by placing the dylibs in Contents/PlugIns/quick, and then
            // creting a symlink to there from the Qt Quick import in Contents/Resources/.
            //
            // Example:
            // MyApp.app/Contents/Resources/qml/QtQuick/Controls/libqtquickcontrolsplugin.dylib ->
            // ../../../../PlugIns/quick/libqtquickcontrolsplugin.dylib
            //

            // The .dylib destination path:
            QString fileDestinationDir = appBundlePath + QStringLiteral("/Contents/PlugIns/quick/");
            QDir().mkpath(fileDestinationDir);
            QString fileDestinationPath = fileDestinationDir + file;

            // The .dylib symlink destination path:
            QString linkDestinationPath = destinationPath + QLatin1Char('/') + file;

            // The (relative) link; with a correct number of "../"'s.
            QString linkPath = QStringLiteral("PlugIns/quick/") + file;
            int cdupCount = linkDestinationPath.count(QStringLiteral("/")) - appBundlePath.count(QStringLiteral("/"));
            for (int i = 0; i < cdupCount - 2; ++i)
                linkPath.prepend("../");

            if (copyFilePrintStatus(fileSourcePath, fileDestinationPath)) {
                linkFilePrintStatus(linkPath, linkDestinationPath);

                runStrip(fileDestinationPath);
                bool useDebugLibs = false;
                bool useLoaderPath = false;
                QList<FrameworkInfo> frameworks = getQtFrameworks(fileDestinationPath, appBundlePath, rpaths, useDebugLibs);
                deployQtFrameworks(frameworks, appBundlePath, QStringList(fileDestinationPath), useDebugLibs, useLoaderPath);
            }
        } else {
            QString fileDestinationPath = destinationPath + QLatin1Char('/') + file;
            copyFilePrintStatus(fileSourcePath, fileDestinationPath);
        }
    }

    QStringList subdirs = QDir(sourcePath).entryList(QStringList() << QStringLiteral("*"), QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dir : subdirs) {
        recursiveCopyAndDeploy(appBundlePath, rpaths, sourcePath + QLatin1Char('/') + dir, destinationPath + QLatin1Char('/') + dir);
    }
}

QString copyDylib(const FrameworkInfo &framework, const QString path)
{
    if (!QFile::exists(framework.sourceFilePath)) {
        LogError() << "no file at" << framework.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Frameworks/libfoo.dylib
    QString dylibDestinationDirectory = path + QLatin1Char('/') + framework.frameworkDestinationDirectory;
    QString dylibDestinationBinaryPath = dylibDestinationDirectory + QLatin1Char('/') + framework.binaryName;

    // Create destination directory
    if (!QDir().mkpath(dylibDestinationDirectory)) {
        LogError() << "could not create destination directory" << dylibDestinationDirectory;
        return QString();
    }

    // Return if the dylib has already been deployed
    if (QFileInfo::exists(dylibDestinationBinaryPath) && !alwaysOwerwriteEnabled)
        return dylibDestinationBinaryPath;

    // Copy dylib binary
    copyFilePrintStatus(framework.sourceFilePath, dylibDestinationBinaryPath);
    return dylibDestinationBinaryPath;
}

QString copyFramework(const FrameworkInfo &framework, const QString path)
{
    if (!QFile::exists(framework.sourceFilePath)) {
        LogError() << "no file at" << framework.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Frameworks/Foo.framework/Versions/5/QtFoo
    QString frameworkDestinationDirectory = path + QLatin1Char('/') + framework.frameworkDestinationDirectory;
    QString frameworkBinaryDestinationDirectory = frameworkDestinationDirectory + QLatin1Char('/') + framework.binaryDirectory;
    QString frameworkDestinationBinaryPath = frameworkBinaryDestinationDirectory + QLatin1Char('/') + framework.binaryName;

    // Return if the framework has aleardy been deployed
    if (QDir(frameworkDestinationDirectory).exists() && !alwaysOwerwriteEnabled)
        return QString();

    // Create destination directory
    if (!QDir().mkpath(frameworkBinaryDestinationDirectory)) {
        LogError() << "could not create destination directory" << frameworkBinaryDestinationDirectory;
        return QString();
    }

    // Now copy the framework. Some parts should be left out (headers/, .prl files).
    // Some parts should be included (Resources/, symlink structure). We want this
    // function to make as few assumptions about the framework as possible while at
    // the same time producing a codesign-compatible framework.

    // Copy framework binary
    copyFilePrintStatus(framework.sourceFilePath, frameworkDestinationBinaryPath);

    // Copy Resources/, Libraries/ and Helpers/
    const QString resourcesSourcePath = framework.frameworkPath + "/Resources";
    const QString resourcesDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Resources";
    recursiveCopy(resourcesSourcePath, resourcesDestianationPath);
    const QString librariesSourcePath = framework.frameworkPath + "/Libraries";
    const QString librariesDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Libraries";
    bool createdLibraries = recursiveCopy(librariesSourcePath, librariesDestianationPath);
    const QString helpersSourcePath = framework.frameworkPath + "/Helpers";
    const QString helpersDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Helpers";
    bool createdHelpers = recursiveCopy(helpersSourcePath, helpersDestianationPath);

    // Create symlink structure. Links at the framework root point to Versions/Current/
    // which again points to the actual version:
    // QtFoo.framework/QtFoo -> Versions/Current/QtFoo
    // QtFoo.framework/Resources -> Versions/Current/Resources
    // QtFoo.framework/Versions/Current -> 5
    linkFilePrintStatus("Versions/Current/" + framework.binaryName, frameworkDestinationDirectory + "/" + framework.binaryName);
    linkFilePrintStatus("Versions/Current/Resources", frameworkDestinationDirectory + "/Resources");
    if (createdLibraries)
        linkFilePrintStatus("Versions/Current/Libraries", frameworkDestinationDirectory + "/Libraries");
    if (createdHelpers)
        linkFilePrintStatus("Versions/Current/Helpers", frameworkDestinationDirectory + "/Helpers");
    linkFilePrintStatus(framework.version, frameworkDestinationDirectory + "/Versions/Current");

    // Correct Info.plist location for frameworks produced by older versions of qmake
    // Contents/Info.plist should be Versions/5/Resources/Info.plist
    const QString legacyInfoPlistPath = framework.frameworkPath + "/Contents/Info.plist";
    const QString correctInfoPlistPath = frameworkDestinationDirectory + "/Resources/Info.plist";
    if (QFile::exists(legacyInfoPlistPath)) {
        copyFilePrintStatus(legacyInfoPlistPath, correctInfoPlistPath);
        patch_debugInInfoPlist(correctInfoPlistPath);
    }
    return frameworkDestinationBinaryPath;
}

void runInstallNameTool(QStringList options)
{
    QProcess installNametool;
    qDebug() << "install_name_tool " + options.join(" ");
    installNametool.start("install_name_tool", options);
    installNametool.waitForFinished();
    if (installNametool.exitCode() != 0) {
        LogError() << installNametool.readAllStandardError();
        LogError() << installNametool.readAllStandardOutput();
    }
}

void changeIdentification(const QString &id, const QString &binaryPath)
{
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " change identification in" << binaryPath;
    LogDebug() << " to" << id;
    runInstallNameTool(QStringList() << "-id" << id << binaryPath);
}

void changeInstallName(const QString &bundlePath, const FrameworkInfo &framework, const QStringList &binaryPaths, bool useLoaderPath)
{
    const QString absBundlePath = QFileInfo(bundlePath).absoluteFilePath();
    for (const QString &binary : binaryPaths) {
        QString deployedInstallName;
        if (useLoaderPath) {
            deployedInstallName = QLatin1String("@loader_path/")
                    + QFileInfo(binary).absoluteDir().relativeFilePath(absBundlePath + QLatin1Char('/') + framework.binaryDestinationDirectory + QLatin1Char('/') + framework.binaryName);
        } else {
            deployedInstallName = framework.deployedInstallName;
        }
        if (!framework.sourceFilePath.isEmpty()) {
            changeInstallName(framework.sourceFilePath, deployedInstallName, binary);
        }
        if (!framework.installName.isEmpty() && framework.installName != framework.sourceFilePath) {
            changeInstallName(framework.installName, deployedInstallName, binary);
        }
        // Workaround for the case when the library ID name is a symlink, while the dependencies
        // specified using the canonical path to the library (QTBUG-56814)
        QFileInfo fileInfo= QFileInfo(framework.installName);
        QString canonicalInstallName = fileInfo.canonicalFilePath();
        if (!canonicalInstallName.isEmpty() && canonicalInstallName != framework.installName) {
            changeInstallName(canonicalInstallName, deployedInstallName, binary);
            // some libraries' inner dependencies (such as ffmpeg, nettle) use symbol link (QTBUG-100093)
            QString innerDependency = fileInfo.canonicalPath() + "/" + fileInfo.fileName();
            if (innerDependency != canonicalInstallName && innerDependency != framework.installName) {
                changeInstallName(innerDependency, deployedInstallName, binary);
            }
        }
    }
}

void addRPath(const QString &rpath, const QString &binaryPath)
{
    runInstallNameTool(QStringList() << "-add_rpath" << rpath << binaryPath);
}

void deployRPaths(const QString &bundlePath, const QList<QString> &rpaths, const QString &binaryPath, bool useLoaderPath)
{
    const QString absFrameworksPath = QFileInfo(bundlePath).absoluteFilePath()
            + QLatin1String("/Contents/Frameworks");
    const QString relativeFrameworkPath = QFileInfo(binaryPath).absoluteDir().relativeFilePath(absFrameworksPath);
    const QString loaderPathToFrameworks = QLatin1String("@loader_path/") + relativeFrameworkPath;
    bool rpathToFrameworksFound = false;
    QStringList args;
    QList<QString> binaryRPaths = getBinaryRPaths(binaryPath, false);
    for (const QString &rpath : std::as_const(binaryRPaths)) {
        if (rpath == "@executable_path/../Frameworks" ||
                rpath == loaderPathToFrameworks) {
            rpathToFrameworksFound = true;
            continue;
        }
        if (rpaths.contains(resolveDyldPrefix(rpath, binaryPath, binaryPath))) {
            args << "-delete_rpath" << rpath;
        }
    }
    if (!args.length()) {
        return;
    }
    if (!rpathToFrameworksFound) {
        if (!useLoaderPath) {
            args << "-add_rpath" << "@executable_path/../Frameworks";
        } else {
            args << "-add_rpath" << loaderPathToFrameworks;
        }
    }
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " change rpaths in" << binaryPath;
    LogDebug() << " using" << args;
    runInstallNameTool(QStringList() << args << binaryPath);
}

void deployRPaths(const QString &bundlePath, const QList<QString> &rpaths, const QStringList &binaryPaths, bool useLoaderPath)
{
    for (const QString &binary : binaryPaths) {
        deployRPaths(bundlePath, rpaths, binary, useLoaderPath);
    }
}

void changeInstallName(const QString &oldName, const QString &newName, const QString &binaryPath)
{
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " in" << binaryPath;
    LogDebug() << " change reference" << oldName;
    LogDebug() << " to" << newName;
    runInstallNameTool(QStringList() << "-change" << oldName << newName << binaryPath);
}

void runStrip(const QString &binaryPath)
{
    if (runStripEnabled == false)
        return;

    LogDebug() << "Using strip:";
    LogDebug() << " stripped" << binaryPath;
    QProcess strip;
    strip.start("strip", QStringList() << "-x" << binaryPath);
    strip.waitForFinished();
    if (strip.exitCode() != 0) {
        LogError() << strip.readAllStandardError();
        LogError() << strip.readAllStandardOutput();
    }
}

void stripAppBinary(const QString &bundlePath)
{
    runStrip(findAppBinary(bundlePath));
}

bool DeploymentInfo::containsModule(const QString &module, const QString &libInFix) const
{
    // Check for framework first
    if (deployedFrameworks.contains(QLatin1String("Qt") + module + libInFix +
                                    QLatin1String(".framework"))) {
        return true;
    }
    // Check for dylib
    const QRegularExpression dylibRegExp(QLatin1String("libQt[0-9]+") + module +
                                         libInFix + QLatin1String(".[0-9]+.dylib"));
    return deployedFrameworks.filter(dylibRegExp).size() > 0;
}

/*
    Deploys the the listed frameworks listed into an app bundle.
    The frameworks are searched for dependencies, which are also deployed.
    (deploying Qt3Support will also deploy QtNetwork and QtSql for example.)
    Returns a DeploymentInfo structure containing the Qt path used and a
    a list of actually deployed frameworks.
*/
DeploymentInfo deployQtFrameworks(QList<FrameworkInfo> frameworks,
        const QString &bundlePath, const QStringList &binaryPaths, bool useDebugLibs,
                                  bool useLoaderPath)
{
    LogNormal();
    LogNormal() << "Deploying Qt frameworks found inside:" << binaryPaths;
    QStringList copiedFrameworks;
    DeploymentInfo deploymentInfo;
    deploymentInfo.useLoaderPath = useLoaderPath;
    deploymentInfo.isFramework = bundlePath.contains(".framework");
    deploymentInfo.isDebug = false;
    QList<QString> rpathsUsed;

    while (frameworks.isEmpty() == false) {
        const FrameworkInfo framework = frameworks.takeFirst();
        copiedFrameworks.append(framework.frameworkName);

        // If a single dependency has the _debug suffix, we treat that as
        // the whole deployment being a debug deployment, including deploying
        // the debug version of plugins.
        if (framework.isDebugLibrary())
            deploymentInfo.isDebug = true;

        if (deploymentInfo.qtPath.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            deploymentInfo.qtPath = QLibraryInfo::path(QLibraryInfo::PrefixPath);
#else
            deploymentInfo.qtPath = QLibraryInfo::location(QLibraryInfo::PrefixPath);
#endif
        }

        if (framework.frameworkDirectory.startsWith(bundlePath)) {
            LogError()  << framework.frameworkName << "already deployed, skipping.";
            continue;
        }

        if (!framework.rpathUsed.isEmpty() && !rpathsUsed.contains(framework.rpathUsed)) {
            rpathsUsed.append(framework.rpathUsed);
        }

        // Copy the framework/dylib to the app bundle.
        const QString deployedBinaryPath = framework.isDylib ? copyDylib(framework, bundlePath)
                                                             : copyFramework(framework, bundlePath);

        // Install_name_tool the new id into the binaries
        changeInstallName(bundlePath, framework, binaryPaths, useLoaderPath);

        // Skip the rest if already was deployed.
        if (deployedBinaryPath.isNull())
            continue;

        runStrip(deployedBinaryPath);

        // Install_name_tool it a new id.
        if (!framework.rpathUsed.length()) {
            changeIdentification(framework.deployedInstallName, deployedBinaryPath);
        }

        // Check for framework dependencies
        QList<FrameworkInfo> dependencies = getQtFrameworks(deployedBinaryPath, bundlePath, rpathsUsed, useDebugLibs);

        for (const FrameworkInfo &dependency : dependencies) {
            if (dependency.rpathUsed.isEmpty()) {
                changeInstallName(bundlePath, dependency, QStringList() << deployedBinaryPath, useLoaderPath);
            } else {
                rpathsUsed.append(dependency.rpathUsed);
            }

            // Deploy framework if necessary.
            if (copiedFrameworks.contains(dependency.frameworkName) == false && frameworks.contains(dependency) == false) {
                frameworks.append(dependency);
            }
        }
    }
    deploymentInfo.deployedFrameworks = copiedFrameworks;
    deployRPaths(bundlePath, rpathsUsed, binaryPaths, useLoaderPath);
    deploymentInfo.rpathsUsed += rpathsUsed;
    deploymentInfo.rpathsUsed.removeDuplicates();
    return deploymentInfo;
}

DeploymentInfo deployQtFrameworks(const QString &appBundlePath, const QStringList &additionalExecutables, bool useDebugLibs)
{
   ApplicationBundleInfo applicationBundle;
   applicationBundle.path = appBundlePath;
   applicationBundle.binaryPath = findAppBinary(appBundlePath);
   applicationBundle.libraryPaths = findAppLibraries(appBundlePath);
   QStringList allBinaryPaths = QStringList() << applicationBundle.binaryPath << applicationBundle.libraryPaths
                                                 << additionalExecutables;
   QList<QString> allLibraryPaths = getBinaryRPaths(applicationBundle.binaryPath, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
   allLibraryPaths.append(QLibraryInfo::path(QLibraryInfo::LibrariesPath));
#else
   allLibraryPaths.append(QLibraryInfo::location(QLibraryInfo::LibrariesPath));
#endif
   allLibraryPaths.removeDuplicates();

   QList<FrameworkInfo> frameworks = getQtFrameworksForPaths(allBinaryPaths, appBundlePath, allLibraryPaths, useDebugLibs);
   if (frameworks.isEmpty() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << "Could not find any external Qt frameworks to deploy in" << appBundlePath;
        LogWarning() << "Perhaps macdeployqt was already used on" << appBundlePath << "?";
        LogWarning() << "If so, you will need to rebuild" << appBundlePath << "before trying again.";
        return DeploymentInfo();
   } else {
       return deployQtFrameworks(frameworks, applicationBundle.path, allBinaryPaths, useDebugLibs, !additionalExecutables.isEmpty());
   }
}

QString getLibInfix(const QStringList &deployedFrameworks)
{
    QString libInfix;
    for (const QString &framework : deployedFrameworks) {
        if (framework.startsWith(QStringLiteral("QtCore")) && framework.endsWith(QStringLiteral(".framework")) && !framework.contains(QStringLiteral("5Compat"))) {
            Q_ASSERT(framework.length() >= 16);
            // 16 == "QtCore" + ".framework"
            const int lengthOfLibInfix = framework.length() - 16;
            if (lengthOfLibInfix)
                libInfix = framework.mid(6, lengthOfLibInfix);
            break;
        }
    }
    return libInfix;
}

void deployPlugins(const ApplicationBundleInfo &appBundleInfo, const QString &pluginSourcePath,
        const QString pluginDestinationPath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    LogNormal() << "Deploying plugins from" << pluginSourcePath;

    if (!pluginSourcePath.contains(deploymentInfo.pluginPath))
        return;

    // Plugin white list:
    QStringList pluginList;

    const auto addPlugins = [&pluginSourcePath,&pluginList,useDebugLibs](const QString &subDirectory,
            const std::function<bool(QString)> &predicate = std::function<bool(QString)>()) {
        const QStringList libs = QDir(pluginSourcePath + QLatin1Char('/') + subDirectory)
                .entryList({QStringLiteral("*.dylib")});
        for (const QString &lib : libs) {
            if (lib.endsWith(QStringLiteral("_debug.dylib")) != useDebugLibs)
                continue;
            if (!predicate || predicate(lib))
                pluginList.append(subDirectory + QLatin1Char('/') + lib);
        }
    };

    // Platform plugin:
    addPlugins(QStringLiteral("platforms"), [](const QString &lib) {
        // Ignore minimal and offscreen platform plugins
        if (!lib.contains(QStringLiteral("cocoa")))
            return false;
        return true;
    });

    // Cocoa print support
    addPlugins(QStringLiteral("printsupport"));

    // Styles
    addPlugins(QStringLiteral("styles"));

    // Check if Qt was configured with -libinfix
    const QString libInfix = getLibInfix(deploymentInfo.deployedFrameworks);

    // Network
    if (deploymentInfo.containsModule("Network", libInfix))
        addPlugins(QStringLiteral("tls"));

    // All image formats (svg if QtSvg is used)
    const bool usesSvg = deploymentInfo.containsModule("Svg", libInfix);
    addPlugins(QStringLiteral("imageformats"), [usesSvg](const QString &lib) {
        if (lib.contains(QStringLiteral("qsvg")) && !usesSvg)
            return false;
        return true;
    });

    addPlugins(QStringLiteral("iconengines"));

    // Platforminputcontext plugins if QtGui is in use
    if (deploymentInfo.containsModule("Gui", libInfix)) {
        addPlugins(QStringLiteral("platforminputcontexts"), [&addPlugins](const QString &lib) {
            // Deploy the virtual keyboard plugins if we have deployed virtualkeyboard
            if (lib.startsWith(QStringLiteral("libqtvirtualkeyboard")))
                addPlugins(QStringLiteral("virtualkeyboard"));
            return true;
        });
    }

    // Sql plugins if QtSql is in use
    if (deploymentInfo.containsModule("Sql", libInfix)) {
        addPlugins(QStringLiteral("sqldrivers"), [](const QString &lib) {
            if (lib.startsWith(QStringLiteral("libqsqlodbc")) || lib.startsWith(QStringLiteral("libqsqlpsql"))) {
                LogWarning() << "Plugin" << lib << "uses private API and is not Mac App store compliant.";
                if (appstoreCompliant) {
                    LogWarning() << "Skip plugin" << lib;
                    return false;
                }
            }
            return true;
        });
    }

    // WebView plugins if QtWebView is in use
    if (deploymentInfo.containsModule("WebView", libInfix)) {
        addPlugins(QStringLiteral("webview"), [](const QString &lib) {
            if (lib.startsWith(QStringLiteral("libqtwebview_webengine"))) {
                LogWarning() << "Plugin" << lib << "uses QtWebEngine and is not Mac App store compliant.";
                if (appstoreCompliant) {
                    LogWarning() << "Skip plugin" << lib;
                    return false;
                }
            }
            return true;
        });
    }

    static const std::map<QString, std::vector<QString>> map {
        {QStringLiteral("Multimedia"), {QStringLiteral("mediaservice"), QStringLiteral("audio")}},
        {QStringLiteral("3DRender"), {QStringLiteral("sceneparsers"), QStringLiteral("geometryloaders"), QStringLiteral("renderers")}},
        {QStringLiteral("3DQuickRender"), {QStringLiteral("renderplugins")}},
        {QStringLiteral("Positioning"), {QStringLiteral("position")}},
        {QStringLiteral("Location"), {QStringLiteral("geoservices")}},
        {QStringLiteral("TextToSpeech"), {QStringLiteral("texttospeech")}}
    };

    for (const auto &it : map) {
        if (deploymentInfo.containsModule(it.first, libInfix)) {
            for (const auto &pluginType : it.second) {
                addPlugins(pluginType);
            }
        }
    }

    for (const QString &plugin : pluginList) {
        QString sourcePath = pluginSourcePath + "/" + plugin;
        const QString destinationPath = pluginDestinationPath + "/" + plugin;
        QDir dir;
        dir.mkpath(QFileInfo(destinationPath).path());

        if (copyFilePrintStatus(sourcePath, destinationPath)) {
            runStrip(destinationPath);
            QList<FrameworkInfo> frameworks = getQtFrameworks(destinationPath, appBundleInfo.path, deploymentInfo.rpathsUsed, useDebugLibs);
            deployQtFrameworks(frameworks, appBundleInfo.path, QStringList() << destinationPath, useDebugLibs, deploymentInfo.useLoaderPath);
        }
    }

    // GIO modules
    {
      QString giomodule_path = qgetenv("GIO_EXTRA_MODULES");
      if (giomodule_path.isEmpty()) {
        if (QDir().exists("/usr/local/lib/gio/modules")) {
          giomodule_path = "/usr/local/lib/gio/modules";
        }
        else if (QDir().exists("/opt/local/lib/gio/modules")) {
          giomodule_path = "/opt/local/lib/gio/modules";
        }
        else {
          qFatal("Missing GIO_EXTRA_MODULES");
        }
      }

      const QStringList giomodules = QStringList() << "libgiognutls.so" << "libgioopenssl.so";
      bool have_giomodule = false;
      for (const QString &giomodule : giomodules) {
        const QString sourcePath = giomodule_path + "/" + giomodule;
        QFileInfo fileinfo(sourcePath);
        if (!fileinfo.exists()) {
          LogError() << "Missing GIO module" << fileinfo.baseName();
          continue;
        }
        have_giomodule = true;
        const QString destinationPath = appBundleInfo.path + "/Contents/PlugIns/gio-modules/" + giomodule;
        QDir dir;
        if (dir.mkpath(QFileInfo(destinationPath).path()) && copyFilePrintStatus(sourcePath, destinationPath)) {
          runStrip(destinationPath);
          QList<FrameworkInfo> frameworks = getQtFrameworks(destinationPath, appBundleInfo.path, deploymentInfo.rpathsUsed, useDebugLibs);
          deployQtFrameworks(frameworks, appBundleInfo.path, QStringList() << destinationPath, useDebugLibs, deploymentInfo.useLoaderPath);
        }
      }

      if (!have_giomodule) {
        qFatal("Missing GIO modules.");
      }

    }

    // gst-plugin-scanner
    {
      QString sourcePath = qgetenv("GST_PLUGIN_SCANNER");
      if (sourcePath.isEmpty()) {
        if (QFileInfo::exists("/usr/local/opt/gstreamer/libexec/gstreamer-1.0/gst-plugin-scanner")) {
          sourcePath = "/usr/local/opt/gstreamer/libexec/gstreamer-1.0/gst-plugin-scanner";
        }
        else if (QFileInfo::exists("/opt/local/libexec/gstreamer-1.0/gst-plugin-scanner")) {
          sourcePath = "/opt/local/libexec/gstreamer-1.0/gst-plugin-scanner";
        }
        else {
          qFatal("Missing GST_PLUGIN_SCANNER.");
        }
      }
      const QString destinationPath = appBundleInfo.path + "/" + "Contents/PlugIns/gst-plugin-scanner";
      QDir dir;
      if (dir.mkpath(QFileInfo(destinationPath).path()) && copyFilePrintStatus(sourcePath, destinationPath)) {
        runStrip(destinationPath);
        QList<FrameworkInfo> frameworks = getQtFrameworks(destinationPath, appBundleInfo.path, deploymentInfo.rpathsUsed, useDebugLibs);
        deployQtFrameworks(frameworks, appBundleInfo.path, QStringList() << destinationPath, useDebugLibs, deploymentInfo.useLoaderPath);
      }
    }

    // GStreamer plugins.
    QStringList gstreamer_plugins = QStringList()
                                                  << "libgstaes.dylib"
                                                  << "libgstaiff.dylib"
                                                  << "libgstapetag.dylib"
                                                  << "libgstapp.dylib"
                                                  << "libgstasf.dylib"
                                                  << "libgstasfmux.dylib"
                                                  << "libgstaudioconvert.dylib"
                                                  << "libgstaudiofx.dylib"
                                                  << "libgstaudiomixer.dylib"
                                                  << "libgstaudioparsers.dylib"
                                                  << "libgstaudiorate.dylib"
                                                  << "libgstaudioresample.dylib"
                                                  << "libgstaudiotestsrc.dylib"
                                                  << "libgstautodetect.dylib"
                                                  << "libgstbs2b.dylib"
                                                  << "libgstcdio.dylib"
                                                  << "libgstcoreelements.dylib"
                                                  << "libgstdash.dylib"
                                                  << "libgstequalizer.dylib"
                                                  << "libgstfaac.dylib"
                                                  << "libgstfaad.dylib"
                                                  << "libgstfdkaac.dylib"
                                                  << "libgstflac.dylib"
                                                  << "libgstgio.dylib"
                                                  //<< "libgstgme.dylib"
                                                  << "libgsthls.dylib"
                                                  << "libgsticydemux.dylib"
                                                  << "libgstid3demux.dylib"
                                                  << "libgstid3tag.dylib"
                                                  << "libgstisomp4.dylib"
                                                  << "libgstlame.dylib"
                                                  << "libgstlibav.dylib"
                                                  << "libgstmpg123.dylib"
                                                  << "libgstmusepack.dylib"
                                                  << "libgstogg.dylib"
                                                  << "libgstopenmpt.dylib"
                                                  << "libgstopus.dylib"
                                                  << "libgstopusparse.dylib"
                                                  << "libgstosxaudio.dylib"
                                                  << "libgstpbtypes.dylib"
                                                  << "libgstplayback.dylib"
                                                  << "libgstreplaygain.dylib"
                                                  << "libgstrtp.dylib"
                                                  << "libgstrtsp.dylib"
                                                  << "libgstsoup.dylib"
                                                  << "libgstspectrum.dylib"
                                                  << "libgstspeex.dylib"
                                                  << "libgsttaglib.dylib"
                                                  << "libgsttcp.dylib"
                                                  << "libgsttwolame.dylib"
                                                  << "libgsttypefindfunctions.dylib"
                                                  << "libgstudp.dylib"
                                                  << "libgstvolume.dylib"
                                                  << "libgstvorbis.dylib"
                                                  << "libgstwavenc.dylib"
                                                  << "libgstwavpack.dylib"
                                                  << "libgstwavparse.dylib"
                                                  << "libgstxingmux.dylib";

    QString gstreamer_plugins_dir = qgetenv("GST_PLUGIN_PATH");
    if (gstreamer_plugins_dir.isEmpty()) {
      if (QDir().exists("/usr/local/lib/gstreamer-1.0")) {
        gstreamer_plugins_dir = "/usr/local/lib/gstreamer-1.0";
      }
      else if (QDir().exists("/opt/local/lib/gstreamer-1.0")) {
        gstreamer_plugins_dir = "/opt/local/lib/gstreamer-1.0";
      }
      else {
        qFatal("Missing GST_PLUGIN_PATH.");
      }
    }

    QStringList missing_gst_plugins;

    for (const QString &plugin : gstreamer_plugins) {
        QFileInfo info(gstreamer_plugins_dir + "/" + plugin);
        if (!info.exists()) {
            info.setFile(gstreamer_plugins_dir + "/" + info.baseName() + QString(".so"));
            if (!info.exists()) {
                LogError() << "Missing gstreamer plugin" << info.baseName();
                missing_gst_plugins << info.baseName();
                continue;
            }
        }
        const QString &sourcePath = info.filePath();
        const QString destinationPath = appBundleInfo.path + "/Contents/PlugIns/gstreamer/" + info.fileName();
        if (QDir().mkpath(QFileInfo(destinationPath).path()) && copyFilePrintStatus(sourcePath, destinationPath)) {
            runStrip(destinationPath);
            QList<FrameworkInfo> frameworks = getQtFrameworks(destinationPath, appBundleInfo.path, deploymentInfo.rpathsUsed, useDebugLibs);
            deployQtFrameworks(frameworks, appBundleInfo.path, QStringList() << destinationPath, useDebugLibs, deploymentInfo.useLoaderPath);
        }
    }

    if (!missing_gst_plugins.isEmpty()) {
      LogError() << "Missing gstreamer plugins" << missing_gst_plugins;
    }

}

void createQtConf(const QString &appBundlePath)
{
    // Set Plugins and imports paths. These are relative to App.app/Contents.
    QByteArray contents = "[Paths]\n"
                          "Plugins = PlugIns\n"
                          "Imports = Resources/qml\n"
                          "Qml2Imports = Resources/qml\n";

    QString filePath = appBundlePath + "/Contents/Resources/";
    QString fileName = filePath + "qt.conf";

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << fileName << "already exists, will not overwrite.";
        LogWarning() << "To make sure the plugins are loaded from the correct location,";
        LogWarning() << "please make sure qt.conf contains the following lines:";
        LogWarning() << "[Paths]";
        LogWarning() << "  Plugins = PlugIns";
        return;
    }

    qtconf.open(QIODevice::WriteOnly);
    if (qtconf.write(contents) != -1) {
        LogNormal() << "Created configuration file:" << fileName;
        LogNormal() << "This file sets the plugin search path to" << appBundlePath + "/Contents/PlugIns";
    }
}

void deployPlugins(const QString &appBundlePath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    ApplicationBundleInfo applicationBundle;
    applicationBundle.path = appBundlePath;
    applicationBundle.binaryPath = findAppBinary(appBundlePath);

    const QString pluginDestinationPath = appBundlePath + "/" + "Contents/PlugIns";
    deployPlugins(applicationBundle, deploymentInfo.pluginPath, pluginDestinationPath, deploymentInfo, useDebugLibs);
}

void deployQmlImport(const QString &appBundlePath, const QList<QString> &rpaths, const QString &importSourcePath, const QString &importName)
{
    QString importDestinationPath = appBundlePath + "/Contents/Resources/qml/" + importName;

    // Skip already deployed imports. This can happen in cases like "QtQuick.Controls.Styles",
    // where deploying QtQuick.Controls will also deploy the "Styles" sub-import.
    if (QDir().exists(importDestinationPath))
        return;

    recursiveCopyAndDeploy(appBundlePath, rpaths, importSourcePath, importDestinationPath);
}

static bool importLessThan(const QVariant &v1, const QVariant &v2)
{
    QVariantMap import1 = v1.toMap();
    QVariantMap import2 = v2.toMap();
    QString path1 = import1["path"].toString();
    QString path2 = import2["path"].toString();
    return path1 < path2;
}

// Scan qml files in qmldirs for import statements, deploy used imports from Qml2ImportsPath to Contents/Resources/qml.
bool deployQmlImports(const QString &appBundlePath, DeploymentInfo deploymentInfo, QStringList &qmlDirs, QStringList &qmlImportPaths)
{
    LogNormal() << "";
    LogNormal() << "Deploying QML imports ";
    LogNormal() << "Application QML file path(s) is" << qmlDirs;
    LogNormal() << "QML module search path(s) is" << qmlImportPaths;

    // Use qmlimportscanner from QLibraryInfo::LibraryExecutablesPath
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString qmlImportScannerPath = QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::LibraryExecutablesPath) + "/qmlimportscanner");
#else
    QString qmlImportScannerPath = QDir::cleanPath(QLibraryInfo::location(QLibraryInfo::LibraryExecutablesPath) + "/qmlimportscanner");
#endif

    // Fallback: Look relative to the macdeployqt binary
    if (!QFile::exists(qmlImportScannerPath))
        qmlImportScannerPath = QCoreApplication::applicationDirPath() + "/qmlimportscanner";

    // Verify that we found a qmlimportscanner binary
    if (!QFile::exists(qmlImportScannerPath)) {
        LogError() << "qmlimportscanner not found at" << qmlImportScannerPath;
        LogError() << "Rebuild qtdeclarative/tools/qmlimportscanner";
        return false;
    }

    // build argument list for qmlimportsanner: "-rootPath foo/ -rootPath bar/ -importPath path/to/qt/qml"
    // ("rootPath" points to a directory containing app qml, "importPath" is where the Qt imports are installed)
    QStringList argumentList;
    for (const QString &qmlDir : qmlDirs) {
        argumentList.append("-rootPath");
        argumentList.append(qmlDir);
    }
    for (const QString &importPath : qmlImportPaths)
        argumentList << "-importPath" << importPath;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString qmlImportsPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
#else
    QString qmlImportsPath = QLibraryInfo::location(QLibraryInfo::QmlImportsPath);
#endif
    argumentList.append( "-importPath");
    argumentList.append(qmlImportsPath);

    // run qmlimportscanner
    QProcess qmlImportScanner;
    qmlImportScanner.start(qmlImportScannerPath, argumentList);
    if (!qmlImportScanner.waitForStarted()) {
        LogError() << "Could not start qmlimpoortscanner. Process error is" << qmlImportScanner.errorString();
        return false;
    }
    qmlImportScanner.waitForFinished(-1);

    // log qmlimportscanner errors
    qmlImportScanner.setReadChannel(QProcess::StandardError);
    QByteArray errors = qmlImportScanner.readAll();
    if (!errors.isEmpty()) {
        LogWarning() << "QML file parse error (deployment will continue):";
        LogWarning() << errors;
    }

    // parse qmlimportscanner json
    qmlImportScanner.setReadChannel(QProcess::StandardOutput);
    QByteArray json = qmlImportScanner.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        LogError() << "qmlimportscanner output error. Expected json array, got:";
        LogError() << json;
        return false;
    }

    // sort imports to deploy a module before its sub-modules (otherwise
    // deployQmlImports can consider the module deployed if it has already
    // deployed one of its sub-module)
    QVariantList array = doc.array().toVariantList();
    std::sort(array.begin(), array.end(), importLessThan);

    // deploy each import
    for (const QVariant &importValue : array) {
        QVariantMap import = importValue.toMap();
        QString name = import["name"].toString();
        QString path = import["path"].toString();
        QString type = import["type"].toString();

        LogNormal() << "Deploying QML import" << name;

        // Skip imports with missing info - path will be empty if the import is not found.
        if (name.isEmpty() || path.isEmpty()) {
            LogNormal() << "  Skip import: name or path is empty";
            LogNormal() << "";
            continue;
        }

        // Deploy module imports only, skip directory (local/remote) and js imports. These
        // should be deployed as a part of the application build.
        if (type != QStringLiteral("module")) {
            LogNormal() << "  Skip non-module import";
            LogNormal() << "";
            continue;
        }

        // Create the destination path from the name
        // and version (grabbed from the source path)
        // ### let qmlimportscanner provide this.
        name.replace(QLatin1Char('.'), QLatin1Char('/'));
        int secondTolast = path.length() - 2;
        QString version = path.mid(secondTolast);
        if (version.startsWith(QLatin1Char('.')))
            name.append(version);

        deployQmlImport(appBundlePath, deploymentInfo.rpathsUsed, path, name);
        LogNormal() << "";
    }
    return true;
}

void codesignFile(const QString &identity, const QString &filePath)
{
    if (!runCodesign)
        return;

    QString codeSignLogMessage = "codesign";
    if (hardenedRuntime)
        codeSignLogMessage += ", enable hardened runtime";
    if (secureTimestamp)
        codeSignLogMessage += ", include secure timestamp";
    LogNormal() << codeSignLogMessage << filePath;

    QStringList codeSignOptions = { "--preserve-metadata=identifier,entitlements", "--force", "-s",
                                    identity, filePath };
    if (hardenedRuntime)
        codeSignOptions << "-o" << "runtime";

    if (secureTimestamp)
        codeSignOptions << "--timestamp";

    if (!extraEntitlements.isEmpty())
        codeSignOptions << "--entitlements" << extraEntitlements;

    QProcess codesign;
    codesign.start("codesign", codeSignOptions);
    codesign.waitForFinished(-1);

    QByteArray err = codesign.readAllStandardError();
    if (codesign.exitCode() > 0) {
        LogError() << "Codesign signing error:";
        LogError() << err;
    } else if (!err.isEmpty()) {
        LogDebug() << err;
    }
}

QSet<QString> codesignBundle(const QString &identity,
                             const QString &appBundlePath,
                             QList<QString> additionalBinariesContainingRpaths)
{
    // Code sign all binaries in the app bundle. This needs to
    // be done inside-out, e.g sign framework dependencies
    // before the main app binary. The codesign tool itself has
    // a "--deep" option to do this, but usage when signing is
    // not recommended: "Signing with --deep is for emergency
    // repairs and temporary adjustments only."

    LogNormal() << "";
    LogNormal() << "Signing" << appBundlePath << "with identity" << identity;

    QStack<QString> pendingBinaries;
    QSet<QString> pendingBinariesSet;
    QSet<QString> signedBinaries;

    // Create the root code-binary set. This set consists of the application
    // executable(s) and the plugins.
    QString appBundleAbsolutePath = QFileInfo(appBundlePath).absoluteFilePath();
    QString rootBinariesPath = appBundleAbsolutePath + "/Contents/MacOS/";
    QStringList foundRootBinaries = QDir(rootBinariesPath).entryList(QStringList() << "*", QDir::Files);
    for (const QString &binary : foundRootBinaries) {
        QString binaryPath = rootBinariesPath + binary;
        pendingBinaries.push(binaryPath);
        pendingBinariesSet.insert(binaryPath);
        additionalBinariesContainingRpaths.append(binaryPath);
    }

    bool getAbsoltuePath = true;
    QStringList foundPluginBinaries = findAppBundleFiles(appBundlePath + "/Contents/PlugIns/", getAbsoltuePath);
    for (const QString &binary : foundPluginBinaries) {
         pendingBinaries.push(binary);
         pendingBinariesSet.insert(binary);
    }

    // Add frameworks for processing.
    QStringList frameworkPaths = findAppFrameworkPaths(appBundlePath);
    for (const QString &frameworkPath : frameworkPaths) {

        // Prioritise first to sign any additional inner bundles found in the Helpers folder (e.g
        // used by QtWebEngine).
        QDirIterator helpersIterator(frameworkPath, QStringList() << QString::fromLatin1("Helpers"), QDir::Dirs | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (helpersIterator.hasNext()) {
            helpersIterator.next();
            QString helpersPath = helpersIterator.filePath();
            QStringList innerBundleNames = QDir(helpersPath).entryList(QStringList() << "*.app", QDir::Dirs);
            for (const QString &innerBundleName : innerBundleNames)
                signedBinaries += codesignBundle(identity,
                                                 helpersPath + "/" + innerBundleName,
                                                 additionalBinariesContainingRpaths);
        }

        // Also make sure to sign any libraries that will not be found by otool because they
        // are not linked and won't be seen as a dependency.
        QDirIterator librariesIterator(frameworkPath, QStringList() << QString::fromLatin1("Libraries"), QDir::Dirs | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (librariesIterator.hasNext()) {
            librariesIterator.next();
            QString librariesPath = librariesIterator.filePath();
            QStringList bundleFiles = findAppBundleFiles(librariesPath, getAbsoltuePath);
            for (const QString &binary : bundleFiles) {
                pendingBinaries.push(binary);
                pendingBinariesSet.insert(binary);
            }
        }
    }

    // Sign all binaries; use otool to find and sign dependencies first.
    while (!pendingBinaries.isEmpty()) {
        QString binary = pendingBinaries.pop();
        if (signedBinaries.contains(binary))
            continue;

        // Check if there are unsigned dependencies, sign these first.
        QStringList dependencies = getBinaryDependencies(rootBinariesPath, binary,
                                                         additionalBinariesContainingRpaths);
        dependencies = QSet<QString>(dependencies.begin(), dependencies.end())
            .subtract(signedBinaries)
            .subtract(pendingBinariesSet)
            .values();

        if (!dependencies.isEmpty()) {
            pendingBinaries.push(binary);
            pendingBinariesSet.insert(binary);
            int dependenciesSkipped = 0;
            for (const QString &dependency : std::as_const(dependencies)) {
                // Skip dependencies that are outside the current app bundle, because this might
                // cause a codesign error if the current bundle is part of the dependency (e.g.
                // a bundle is part of a framework helper, and depends on that framework).
                // The dependencies will be taken care of after the current bundle is signed.
                if (!dependency.startsWith(appBundleAbsolutePath)) {
                    ++dependenciesSkipped;
                    LogNormal() << "Skipping outside dependency: " << dependency;
                    continue;
                }
                pendingBinaries.push(dependency);
                pendingBinariesSet.insert(dependency);
            }

            // If all dependencies were skipped, make sure the binary is actually signed, instead
            // of going into an infinite loop.
            if (dependenciesSkipped == dependencies.size()) {
                pendingBinaries.pop();
            } else {
                continue;
            }
        }

        // Look for an entitlements file in the bundle to include when signing
        extraEntitlements = findEntitlementsFile(appBundleAbsolutePath + "/Contents/Resources/");

        // All dependencies are signed, now sign this binary.
        codesignFile(identity, binary);
        signedBinaries.insert(binary);
        pendingBinariesSet.remove(binary);
    }

    LogNormal() << "Finished codesigning " << appBundlePath << "with identity" << identity;

    // Verify code signature
    QProcess codesign;
    codesign.start("codesign", QStringList() << "--deep" << "-v" << appBundlePath);
    codesign.waitForFinished(-1);
    QByteArray err = codesign.readAllStandardError();
    if (codesign.exitCode() > 0) {
        LogError() << "codesign verification error:";
        LogError() << err;
    } else if (!err.isEmpty()) {
        LogDebug() << err;
    }

    return signedBinaries;
}

void codesign(const QString &identity, const QString &appBundlePath) {
    codesignBundle(identity, appBundlePath, QList<QString>());
}

void createDiskImage(const QString &appBundlePath, const QString &filesystemType)
{
    QString appBaseName = appBundlePath;
    appBaseName.chop(4); // remove ".app" from end

    QString dmgName = appBaseName + ".dmg";

    QFile dmg(dmgName);

    if (dmg.exists() && alwaysOwerwriteEnabled)
        dmg.remove();

    if (dmg.exists()) {
        LogNormal() << "Disk image already exists, skipping .dmg creation for" << dmg.fileName();
    } else {
        LogNormal() << "Creating disk image (.dmg) for" << appBundlePath;
    }

    LogNormal() << "Image will use" << filesystemType;

    // More dmg options can be found in the hdiutil man page.
    QStringList options = QStringList()
            << "create" << dmgName
            << "-srcfolder" << appBundlePath
            << "-format" << "UDZO"
            << "-fs" << filesystemType
            << "-volname" << appBaseName;

    QProcess hdutil;
    hdutil.start("hdiutil", options);
    hdutil.waitForFinished(-1);
    if (hdutil.exitCode() != 0) {
        LogError() << "Bundle creation error:" << hdutil.readAllStandardError();
    }
}

void fixupFramework(const QString &frameworkName)
{
    // Expected framework name looks like "Foo.framework"
    QStringList parts = frameworkName.split(".");
    if (parts.count() < 2) {
        LogError() << "fixupFramework: Unexpected framework name" << frameworkName;
        return;
    }

    // Assume framework binary path is Foo.framework/Foo
    QString frameworkBinary = frameworkName + QStringLiteral("/") + parts[0];

    // Xcode expects to find Foo.framework/Versions/A when code
    // signing, while qmake typically generates numeric versions.
    // Create symlink to the actual version in the framework.
    linkFilePrintStatus("Current", frameworkName + "/Versions/A");

    // Set up @rpath structure.
    changeIdentification("@rpath/" + frameworkBinary, frameworkBinary);
    addRPath("@loader_path/../../Contents/Frameworks/", frameworkBinary);
}

