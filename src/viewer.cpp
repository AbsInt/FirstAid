/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2008, Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2009, Shawn Rutledge <shawn.t.rutledge@gmail.com>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
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

#include "config.h"
#include "findbar.h"
#include "helpdialog.h"
#include "main.h"
#include "navigationtoolbar.h"
#include "pageview.h"
#include "searchengine.h"
#include "tocdock.h"

#include <poppler-qt6.h>

#include <QDir>
#include <QSettings>
#include <QUrl>
#include <QDesktopServices>
#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QProgressDialog>
#include <QStackedWidget>
#include <QTcpSocket>
#include <QThreadPool>
#include <QVBoxLayout>
#include <QWindow>

#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

PdfViewer *PdfViewer::s_instance = nullptr;

PdfViewer::PdfViewer(const QString &file, quint16 tcpPort)
    : QMainWindow()
{
    // register singleton
    s_instance = this;

    const QColor gray = appIsDarkThemed ? Qt::darkGray : Qt::gray;
    const QColor lightGray = appIsDarkThemed ? Qt::gray : Qt::lightGray;

    setStyleSheet(QStringLiteral("QToolButton { border-width: 1px; border-radius: 6px; border-style: none; padding: 2px }"
                                 "QToolButton:hover { border-style: solid; border-color: %1; padding: 1px }"
                                 "QToolButton:focus { border-style: dotted; border-color: %1; padding: 1px }"
                                 "QToolButton:pressed { border-style: solid; border-color: %1; padding-left: 3px; padding-top: 3px; padding-right: 1px; padding-bottom: 1px }"
                                 "QToolButton:checked { border-style: solid; border-top-color: %1; border-left-color: %1; border-bottom-color: %2; border-right-color: %2; padding-left: 2px; padding-top: 2px; padding-right: 0px; "
                                 "padding-bottom: 0px }"
                                 "QToolButton::menu-indicator { image: url(empty.png) }"
                                 "QMenu { padding: 1px }")
                      .arg(gray.name())
                      .arg(lightGray.name()));

    // allow gui to potentially accept drops at all
    setAcceptDrops(true);

    // setup the menu action
    QMenu *menu = new QMenu(this);

    QAction *fileOpen = menu->addAction(createIcon(QStringLiteral(":/icons/document-open.png")), tr("&Open..."), this, &PdfViewer::slotOpenFile);
    fileOpen->setShortcut(QKeySequence::Open);

    m_fileReloadAct = menu->addAction(createIcon(QStringLiteral(":/icons/view-refresh.png")), tr("&Reload"), this, &PdfViewer::slotReload);
    m_fileReloadAct->setShortcut(QKeySequence::Refresh);
    menu->addSeparator();

    m_fileOpenExternalAct = menu->addAction(createIcon(QStringLiteral(":/icons/acrobat.png")), tr("&Open in external PDF viewer"), this, &PdfViewer::slotOpenFileExternal);
    m_fileOpenExternalAct->setShortcut(Qt::CTRL | Qt::Key_E);

    m_filePrintAct = menu->addAction(createIcon(QStringLiteral(":/icons/document-print.png")), tr("&Print..."), this, &PdfViewer::slotPrint);
    m_filePrintAct->setShortcut(QKeySequence::Print);
    menu->addSeparator();

    QAction *act = menu->addAction(createIcon(QStringLiteral(":/icons/help-keybord-shortcuts.png")), tr("&Keyboard shortcuts..."), this, &PdfViewer::slotHelp);
    act->setShortcut(QKeySequence::HelpContents);

    act = menu->addAction(createIcon(QStringLiteral(":/icons/help-about.png")), tr("&About"), this, &PdfViewer::slotAbout);
    act->setShortcut(QKeySequence::WhatsThis);
    menu->addSeparator();

    act = menu->addAction(createIcon(QStringLiteral(":/icons/application-exit.png")), tr("&Quit"), qApp, QApplication::closeAllWindows);
    act->setShortcut(QKeySequence::Quit);

    QWidget *w = new QWidget(this);
    QVBoxLayout *vbl = new QVBoxLayout(w);
    vbl->setContentsMargins(0, 0, 0, 0);

    m_view = new PageView(this);
    vbl->addWidget(m_view);

    setFocusProxy(m_view);

    FindBar *fb = new FindBar(w);
    vbl->addWidget(fb);

    setCentralWidget(w);

    TocDock *tocDock = new TocDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, tocDock);

    NavigationToolBar *navbar = new NavigationToolBar(tocDock->toggleViewAction(), menu, this);
    addToolBar(navbar);

    connect(&m_document, &Document::documentChanged, &m_searchEngine, &SearchEngine::reset);

    /**
     * auto-reload
     * delay it by 1 second to allow files to be written
     */
    m_fileWatcherReloadTimer.setSingleShot(true);
    m_fileWatcherReloadTimer.setInterval(1000);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &PdfViewer::slotDelayedReload);
    connect(&m_fileWatcherReloadTimer, &QTimer::timeout, this, &PdfViewer::slotReload);

    // update action state & co
    updateOnDocumentChange();

    /**
     * open tcp socket if requested
     */
    if (tcpPort > 0) {
        QTcpSocket *tcpSocket = new QTcpSocket(this);
        connect(tcpSocket, &QTcpSocket::readyRead, this, &PdfViewer::receiveCommand);
        tcpSocket->connectToHost(QHostAddress::LocalHost, tcpPort);
        tcpSocket->waitForConnected();
    }

    /**
     * load document if one is passed
     * useful to have initial geometry based on content
     */
    if (!file.isEmpty())
        loadDocument(file);

    /**
     * set default size
     */
    resize(800, 600);

    /**
     * restore old geometry & dock widget state, will handle "bad geometries", see http://doc.qt.io/qt-5.7/qmainwindow.html#restoreState
     */
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("MainWindow/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("MainWindow/windowState")).toByteArray());
}

PdfViewer::~PdfViewer()
{
    // close document, doesn't delete the m_document and we need m_view here
    closeDocument();

    // close view early so no strange signals are handled
    delete m_view;

    // unregister singleton
    s_instance = nullptr;
}

void PdfViewer::loadDocument(QString file, bool forceReload)
{
    // absolute path in any case, to have full url for later external open!
    file = QFileInfo(file).absoluteFilePath();

    // try to canonicalize, might fail if not there
    if (!QFileInfo(file).canonicalFilePath().isEmpty())
        file = QFileInfo(file).canonicalFilePath();

    // bail out early if file does not exist
    if (!QFileInfo(file).exists()) {
        QMessageBox::critical(this, tr("File not found"), tr("File '%1' does not exist.").arg(file));
        return;
    }

    // we try to load the same document without forced reload => just activate the window and be done
    if (file == m_filePath && !forceReload) {
        raise();
        activateWindow();
        return;
    }

    // cleanup old document
    closeDocument();

    QProgressDialog *pd = new QProgressDialog(this);
    pd->setWindowModality(Qt::WindowModal);
    pd->setCancelButton(nullptr);
    pd->setMinimumDuration(2000);
    pd->setLabelText(QStringLiteral("Loading document..."));
    pd->setRange(0, 0);

    // try to load the document
    m_loadingFile = true;
    QtConcurrent::run([file]() { return Poppler::Document::load(file); }).then(this, [this, file, pd](std::unique_ptr<Poppler::Document> newdoc) {
        if (!newdoc || newdoc->isLocked()) {
            // delete progress dialog
            delete pd;

            // show message
            QMessageBox::critical(this, tr("Cannot open file"), tr("Cannot open file '%1'.").arg(file));
            return;
        }

        // pass loaded poppler document to our internal one
        m_document.setDocument(std::move(newdoc), pd);

        // delete progress dialog
        delete pd;

        // set file + watch
        m_filePath = file;
        m_fileWatcher.addPath(m_filePath);

        // update action state & co.
        updateOnDocumentChange();

        // determine last visible page for the current file
        QSettings settings;
        settings.beginGroup(QStringLiteral("Files"));
        int page = settings.value(QString::fromUtf8(m_filePath.toUtf8().toPercentEncoding()), 0).toInt();
        settings.endGroup();

        // queue goto page request as on startup there may be some signals still flying around
        QMainWindow::metaObject()->invokeMethod(m_view, "gotoPage", Qt::QueuedConnection, Q_ARG(int, page));

        // we are no longer loading
        m_loadingFile = false;

        // check of there are command to process
        QTimer::singleShot(0, this, [this]() { processCommands(); });
    });
}

void PdfViewer::closeDocument()
{
    if (!m_document.isValid())
        return;

    // drain the thread pool with the potential still running background renderers
    QThreadPool::globalInstance()->clear();
    QThreadPool::globalInstance()->waitForDone();

    QSettings settings;
    settings.beginGroup(QStringLiteral("Files"));
    settings.setValue(QString::fromUtf8(m_filePath.toUtf8().toPercentEncoding()), m_view->currentPage());
    settings.endGroup();

    m_document.setDocument(nullptr);

    // remove path
    m_fileWatcher.removePath(m_filePath);

    // update action state & co.
    updateOnDocumentChange();
}

void PdfViewer::receiveCommand()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    // just to be sure: if we packed x lines into one string, split it
    const auto lines = QString::fromUtf8(socket->readAll()).split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        // queue the command if not empty
        if (const auto trimmedLine = line.trimmed(); !trimmedLine.isEmpty())
            m_pendingCommands << trimmedLine;
    }

    // when not loading a document we can process the command
    if (!m_loadingFile)
        QTimer::singleShot(0, this, &PdfViewer::processCommands);
}

void PdfViewer::processCommands()
{
    // any command at all?
    if (m_pendingCommands.isEmpty())
        return;

    // shall we activate the window?
    bool activate = false;
    QString activateToken;

    // get next command
    const QString command = m_pendingCommands.takeFirst();

    if (command.startsWith(QLatin1String("open "))) {
        loadDocument(command.mid(5));
        activate = true;
    }

    else if (command.startsWith(QLatin1String("goto "))) {
        const QString target = command.mid(5);
        bool ok = false;
        const int pageNumber = target.toInt(&ok);
        if (ok)
            m_view->gotoPage(pageNumber - 1);
        else {
            for (const QString &t : target.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                bool valid = false;
                if (auto linkDest = (document()->linkDestination(t))) {
                    valid = linkDest->pageNumber() > 0;
                }

                if (valid) {
                    m_view->gotoDestinationName(t, true, false);
                    break;
                }
            }
        }

        activate = true;
    }

    else if (command.startsWith(QLatin1String("close")))
        QTimer::singleShot(0, qApp, &QApplication::quit);

    else if (command.startsWith(QLatin1String("activate"))) {
        activate = true;
        activateToken = command.mid(9);
    }

    // now check if we should activate the window
    if (activate) {
#if defined(Q_OS_WIN)
        const auto winHandle = (HWND)windowHandle()->winId();
        if (::IsIconic(winHandle)) {
            ::ShowWindow(winHandle, SW_RESTORE);
        }
        ::SetForegroundWindow(winHandle);
#else
        qputenv("XDG_ACTIVATION_TOKEN", activateToken.toUtf8());
#endif

        setWindowState(windowState() & ~Qt::WindowMinimized);
        raise();
        activateWindow();
    }

    // try to process other pending commands
    QTimer::singleShot(0, this, &PdfViewer::processCommands);
}

void PdfViewer::closeEvent(QCloseEvent *event)
{
    /**
     * save geometry & dock widgets state, like show in http://doc.qt.io/qt-5.7/qmainwindow.html#saveState
     */
    QSettings settings;
    settings.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("MainWindow/windowState"), saveState());
    QMainWindow::closeEvent(event);
}

void PdfViewer::slotOpenFile()
{
    /**
     * get file to open, if any
     */
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open PDF"), m_filePath, tr("PDFs (*.pdf)"));
    if (fileName.isEmpty())
        return;

    /**
     * only allow to open in THIS instance, if no file opened ATM
     * else we might open a different document in the online help mode => that would confuse the main application
     */
    if (!m_document.isValid()) {
        loadDocument(fileName);
        return;
    }

    /**
     * else: spawn new instance of FirstAid
     */
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList() << fileName);
}

void PdfViewer::slotOpenFileExternal()
{
    if (!m_filePath.isEmpty())
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_filePath)))
            QMessageBox::warning(this, tr("Error"), tr("Failed to open file in external PDF viewer."));
}

void PdfViewer::slotReload()
{
    if (!m_filePath.isEmpty())
        loadDocument(m_filePath, true /* force reload */);
}

void PdfViewer::slotDelayedReload()
{
    // restart singleshot timer => 1 second later we will reload
    m_fileWatcherReloadTimer.start();
}

void PdfViewer::slotPrint()
{
    const int currentPage = m_view->currentPage() + 1;

    // let the user select the printer to use
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    printDialog.setMinMax(1, m_document.numPages());
    printDialog.setFromTo(1, m_document.numPages());
    printDialog.setOption(QAbstractPrintDialog::PrintSelection, false);
    printDialog.setOption(QAbstractPrintDialog::PrintCurrentPage, currentPage >= 1 && currentPage <= m_document.numPages());
    printDialog.setOption(QAbstractPrintDialog::PrintCollateCopies, false);
    printDialog.setOption(QAbstractPrintDialog::PrintShowPageSize, false);
    if (!printDialog.exec())
        return;

    // determine range
    int fromPage = 0;
    int toPage = 0;

    switch (printDialog.printRange()) {
        case QAbstractPrintDialog::AllPages:
            fromPage = 1;
            toPage = m_document.numPages();
            break;

        case QAbstractPrintDialog::Selection:
            // should never happen
            return;

        case QAbstractPrintDialog::PageRange:
            fromPage = printer.fromPage();
            toPage = printer.toPage();
            break;

        case QAbstractPrintDialog::CurrentPage:
            fromPage = toPage = currentPage;
            break;
    }

    // some sanity checks
    fromPage = qMax(fromPage, 1);
    toPage = qMin(toPage, m_document.numPages());
    if (toPage == 0)
        toPage = m_document.numPages();

    // provide some feedback
    QProgressDialog pd(tr("Printing..."), tr("Abort"), fromPage, toPage, this);
    pd.setWindowModality(Qt::WindowModal);
    pd.setMinimumDuration(0);
    pd.show();

    // gather some information
    QRectF printerPageRect = printer.pageRect(QPrinter::DevicePixel);

    // print given range
    QPainter painter;
    painter.begin(&printer);
    for (int pageNumber = fromPage; pageNumber <= toPage; pageNumber++) {
        pd.setLabelText(tr("Printing page %1...").arg(pageNumber));
        pd.setValue(pageNumber);
        if (pd.wasCanceled())
            break;

        // get page
        Poppler::Page *page = m_document.page(pageNumber - 1);

        // compute resolution so that page fits within the printable area
        QSizeF pageSize = page->pageSizeF();
        qreal scaleX = qMin(1.0, printerPageRect.width() / (72 * pageSize.width() / printer.resolution()));
        qreal scaleY = qMin(1.0, printerPageRect.height() / (72 * pageSize.height() / printer.resolution()));
        qreal res = printer.resolution() * qMin(scaleX, scaleY);

        // render page to image
        QImage image = page->renderToImage(res, res);

        // print image
        painter.drawImage(-printerPageRect.topLeft(), image);

        // advance page
        if (pageNumber != printer.toPage())
            printer.newPage();
    }

    // flush
    painter.end();
}

void PdfViewer::slotHelp()
{
    HelpDialog hd(this);
    hd.exec();
}

void PdfViewer::slotAbout()
{
    /**
     * construct release info, if defined to something != ""
     */
    QString releaseInfo;
    if (!QString::fromUtf8(FIRSTAID_RELEASE_STRING).isEmpty())
        releaseInfo = tr("<p><b>Version %1 Build %2<br>Tag %3</b></p>").arg(QString::fromUtf8(FIRSTAID_RELEASE_STRING)).arg(QString::fromUtf8(FIRSTAID_BUILD_STRING)).arg(QString::fromUtf8(FIRSTAID_TAG_STRING));

    QMessageBox::about(this,
                       tr("About FirstAid"),
                       tr("<h1>FirstAid - PDF Help Viewer</h1>%1"
                          "<p>Based on the <a href=\"https://poppler.freedesktop.org/\">Poppler PDF rendering library</a>.</p>"
                          "<p>Licensed under the <a href=\"https://github.com/AbsInt/FirstAid/blob/master/COPYING\">GPLv2+</a>.</p>"
                          "<p>Sources available on <a href=\"https://github.com/AbsInt/FirstAid\">https://github.com/AbsInt/FirstAid</a>.</p>")
                           .arg(releaseInfo));
}

void PdfViewer::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile())
        loadDocument(urls.first().toLocalFile(), true /* force reload */);
}

void PdfViewer::dragEnterEvent(QDragEnterEvent *event)
{
    if (Qt::CopyAction == event->proposedAction()) {
        const QMimeData *mimeData = event->mimeData();
        if (mimeData->hasUrls()) {
            QList<QUrl> urls = mimeData->urls();
            if (1 == urls.count() && urls.first().isLocalFile())
                event->acceptProposedAction();
        }
    }
}

void PdfViewer::updateOnDocumentChange()
{
    /**
     * action states
     */
    m_fileOpenExternalAct->setEnabled(m_document.isValid());
    m_fileReloadAct->setEnabled(m_document.isValid());
    m_filePrintAct->setEnabled(m_document.isValid());

    /**
     * Window title update
     */
    if (m_filePath.isEmpty())
        setWindowTitle(tr("FirstAid"));
    else if (m_document.title().isEmpty())
        setWindowTitle(QFileInfo(m_filePath).fileName());
    else
        setWindowTitle(m_document.title());
}

QMenu *PdfViewer::createPopupMenu()
{
    return new QMenu();
}
