/*
 * Copyright (C) 2008, Pino Toscano <pino@kde.org>
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
     * Application with widgets
     */
    QApplication app(argc, argv);

    /**
     * Basic info about the program
     */
    QCoreApplication::setOrganizationName("AbsInt");
    QCoreApplication::setOrganizationDomain("absint.com");
    QCoreApplication::setApplicationName("FirstAid");

    /**
     * define & parse our command line
     */
    QCommandLineParser parser;
    parser.setApplicationDescription("FirstAid - PDF Help Viewer");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("main", "PDF file to open"));
    parser.addOption(QCommandLineOption("stdin", QCoreApplication::translate("main", "Read commands from stdin.")));
    parser.process(app);

    /**
     * Construct our main window
     */
    PdfViewer viewer;
    viewer.show();

    /**
     * open file if any given
     */
    const QStringList args = parser.positionalArguments();
    if (!args.empty())
        viewer.loadDocument(args.at(0));

    /**
     * should we handle commands from stdin?
     */
    StdinReaderThread *stdinThread = nullptr;
    if (parser.isSet("stdin")) {
        stdinThread = new StdinReaderThread(&viewer);
        stdinThread->start();
    }

    /**
     * => start event loop
     */
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
