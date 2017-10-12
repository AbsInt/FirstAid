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
#include <QCommandLineParser>
#include <QtPlugin>
#include <QSocketNotifier>

#include <unistd.h>

int main(int argc, char *argv[])
{
/**
 * if you want static binaries, init plugins
 */
#ifdef AI_LINK_QT_STATIC_WINDOWS
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin)
#endif

#ifdef AI_LINK_QT_STATIC_APPLE
    Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QCocoaPrinterSupportPlugin)
#endif

#ifdef AI_LINK_QT_STATIC_LINUX
    Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
    Q_IMPORT_PLUGIN(QSvgIconPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QSvgPlugin)
    Q_IMPORT_PLUGIN(QCupsPrinterSupportPlugin)
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

    /**
     * define & parse our command line
     */
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("FirstAid - PDF Help Viewer"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QCoreApplication::translate("main", "PDF file to open"));
    parser.process(app);

    /**
     * Construct our main window, perhaps open document passed on command line
     */
    const QStringList args = parser.positionalArguments();
    PdfViewer viewer(args.empty() ? QString() : args.at(0));

    /**
     * we want to get info from stdin for commands
     */
    QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&notifier, SIGNAL(activated(int)), &viewer, SLOT(processCommand()));

    /**
     * => show widget + start event loop
     */
    viewer.show();
    const int returnValue = app.exec();

    /**
     * be done
     */
    return returnValue;
}
