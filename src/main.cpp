/*
 * Copyright (C) 2016, Marc Langenbach <mlangen@absint.com>
 * Copyright (C) 2016, Christoph Cullmann <cullmann@absint.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "viewer.h"

#include <QApplication>
#include <QBitmap>
#include <QCommandLineParser>
#include <QDir>
#include <QPixmap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QSocketNotifier>
#include <unistd.h>
#endif

static void setApplicationIcon(const QString &pngIcon)
{
    // Wayland icon handling
    if (qApp->platformName() == QStringLiteral("wayland")) {
        // use name we have in prefix
        const QString applicationName = QStringLiteral("com.absint.firstaid");

        // create a desktop file to note down our icon
        if (const auto appDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation); !appDir.isEmpty()) {
            QDir().mkpath(appDir);
            QSaveFile desktopFile(appDir + QStringLiteral("/") + applicationName + QStringLiteral(".desktop"));
            if (!QFile::exists(desktopFile.fileName()) && desktopFile.open(QSaveFile::WriteOnly)) {
                desktopFile.write(QStringLiteral("[Desktop Entry]\nNoDisplay=true\nIcon=%1\n").arg(applicationName).toUtf8().constData());
                desktopFile.commit();
            }
        }

        // create a matching icon
        if (const auto dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation); !dataDir.isEmpty()) {
            const QString iconDir = dataDir + QStringLiteral("/icons");
            QDir().mkpath(iconDir);
            QSaveFile iconFile(iconDir + QStringLiteral("/") + applicationName + QStringLiteral(".png"));
            if (!QFile::exists(iconFile.fileName()) && iconFile.open(QSaveFile::WriteOnly)) {
                QFile sourceIcon(pngIcon);
                sourceIcon.open(QFile::ReadOnly);
                iconFile.write(sourceIcon.readAll());
                iconFile.commit();
            }
        }
        qApp->setDesktopFileName(applicationName);
    }

    // all other systems just need this
    // do this always to have the icon available via QApplication::windowIcon
    QApplication::setWindowIcon(QIcon(pngIcon));
}

bool appIsDarkThemed = false;

int main(int argc, char *argv[])
{
    // clean env before we construct application to avoid wrong plugin loading!
    qunsetenv("QT_PLUGIN_PATH");

    /**
     * allow fractional scaling
     * new in Qt 5.14
     * do that only on Windows, leads to artifacts on unices
     * see bug 27948
     */
#if defined(Q_OS_WIN)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    /**
     * Application with widgets
     */
    QApplication app(argc, argv);

    /**
     * Basic info about the program
     */
    QCoreApplication::setOrganizationName(QStringLiteral("AbsInt"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("absint.com"));
    QCoreApplication::setApplicationName(QStringLiteral("FirstAid"));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(FIRSTAID_RELEASE_STRING));
    setApplicationIcon(QStringLiteral(":/firstaid.png"));

    /**
     * define & parse our command line
     */
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("FirstAid - PDF Help Viewer"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QCoreApplication::translate("main", "PDF file to open"));
    parser.addOption(QCommandLineOption(QStringLiteral("stdin"), QCoreApplication::translate("main", "Read commands from stdin.")));
    parser.process(app);

    /**
     * Try to detect dark themed applications
     */

    appIsDarkThemed = QPalette().color(QPalette::Base).lightness() < 128;

    QApplication::setStyle(QStringLiteral("fusion"));
    if (appIsDarkThemed)
        QIcon::setThemeName(QStringLiteral("breeze-dark"));

    /**
     * Construct our main window, perhaps open document passed on command line
     */
    const QStringList args = parser.positionalArguments();
    PdfViewer viewer(args.empty() ? QString() : args.at(0));

    /**
     * we want to get info from stdin for commands?
     */
    if (parser.isSet(QStringLiteral("stdin"))) {
#ifdef Q_OS_WIN
        QTimer stdinPollTimer;
        QObject::connect(&stdinPollTimer, &QTimer::timeout, &viewer, &PdfViewer::processCommand);
        stdinPollTimer.start(500);
        QTimer::singleShot(0, &viewer, &PdfViewer::processCommand);
#else
        QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
        QObject::connect(&notifier, &QSocketNotifier::activated, &viewer, &PdfViewer::processCommand);
#endif

        /**
         * => show widget + start event loop
         */
        viewer.show();
        return app.exec();
    }

    /**
     * else: no input reading on stdin, just viewer
     */
    viewer.show();
    return app.exec();
}

QIcon createIcon(const QString &iconName)
{
    QIcon icon(iconName);

    if (appIsDarkThemed) {
        QPixmap px = icon.pixmap(128, 128);
        QPixmap pxr(px.size());
        pxr.fill(Qt::lightGray);
        pxr.setMask(px.createMaskFromColor(Qt::transparent));
        icon = QIcon(pxr);
    }

    return icon;
}
