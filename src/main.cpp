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

#include "viewer.h"

#include <QApplication>
#include <QBitmap>
#include <QCommandLineParser>
#include <QPixmap>
#include <QTimer>
#include <QtPlugin>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QSocketNotifier>
#include <unistd.h>
#endif

#ifdef AI_LINK_QT_STATIC_WINDOWS
#pragma warning(disable : 4101) // 'qt_static_plugin_QSvgIconPlugin' : unreferenced local variable
#pragma warning(disable : 4930) // 'const QStaticPlugin qt_static_plugin_QSvgIconPlugin(void)': prototyped function not called (was a variable definition intended?)
#endif

int main(int argc, char *argv[])
{
/**
 * if you want static binaries, init plugins
 */
#ifdef AI_LINK_QT_STATIC_WINDOWS
    // import here all plugins we need, patches to Qt5GuiConfig.cmake ensure we link against enough stuff
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin) // for GUI batch mode testing in MF/TF
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QGifPlugin)
    Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin)
#endif

#ifdef AI_LINK_QT_STATIC_APPLE
    // import here all plugins we need, patches to Qt5GuiConfig.cmake ensure we link against enough stuff
    Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QGifPlugin)
    Q_IMPORT_PLUGIN(QCocoaPrinterSupportPlugin)
#endif

#ifdef AI_LINK_QT_STATIC_LINUX
    // import here all plugins we need, patches to Qt5GuiConfig.cmake ensure we link against enough stuff
    Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
    Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin) // for GUI batch mode testing in MF/TF
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QGifPlugin)
    Q_IMPORT_PLUGIN(QCupsPrinterSupportPlugin)
    Q_IMPORT_PLUGIN(QComposePlatformInputContextPlugin) // ensure compose keys work, bug 25999
#endif

    /**
     * Use high dpi pixmaps
     */
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    /**
     * Enables high-DPI scaling in Qt on supported platforms (see also High DPI Displays).
     * Supported platforms are X11, Windows and Android.
     * new in Qt 5.6
     */
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);

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
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/firstaid.svg")));

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
        QObject::connect(&stdinPollTimer, SIGNAL(timeout()), &viewer, SLOT(processCommand()));
        stdinPollTimer.start(500);
        QTimer::singleShot(0, &viewer, SLOT(processCommand()));
#else
        QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
        QObject::connect(&notifier, SIGNAL(activated(int)), &viewer, SLOT(processCommand()));
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

    if (auto guiApp = qobject_cast<QGuiApplication *>(QCoreApplication::instance()); guiApp && guiApp->palette().color(QPalette::Base).lightness() < 125) {
        QPixmap px = icon.pixmap(128, 128);
        QPixmap pxr(px.size());
        pxr.fill(Qt::lightGray);
        pxr.setMask(px.createMaskFromColor(Qt::transparent));
        icon = QIcon(pxr);
    }

    return icon;
}
