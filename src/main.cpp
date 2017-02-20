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

#include "stdinreaderthread.h"
#include "viewer.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QtPlugin>

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
    parser.addOption(QCommandLineOption(QStringLiteral("stdin"), QCoreApplication::translate("main", "Read commands from stdin.")));
    parser.process(app);

    /**
     * Construct our main window, perhaps open document passed on command line
     */
    const QStringList args = parser.positionalArguments();
    PdfViewer viewer(args.empty() ? QString() : args.at(0));

    /**
     * should we handle commands from stdin?
     */
    StdinReaderThread *stdinThread = nullptr;
    if (parser.isSet(QStringLiteral("stdin"))) {
        /**
         * create our own thread for blocking read stdin, tell it to send the events to the viewer instance
         */
        stdinThread = new StdinReaderThread(&viewer);

        /**
         * delay start until even loop runs, we otherwise miss things like first goto command
         */
        QTimer::singleShot(0, stdinThread, SLOT(start()));
    }

    /**
     * => show widget + start event loop
     */
    viewer.show();
    const int returnValue = app.exec();

    /**
     * we might need to end our stdin thread!
     */
    if (stdinThread) {
        /**
         * terminate / wait for done + delete
         * this is not nice, should be improved!
         */
        stdinThread->terminate();
        stdinThread->wait();
        delete stdinThread;
    }

    /**
     * be done
     */
    return returnValue;
}
