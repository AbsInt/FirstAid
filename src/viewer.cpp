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
#include "navigationtoolbar.h"
#include "pageview.h"
#include "searchengine.h"
#include "stdinreaderthread.h"
#include "tocdock.h"

#include <poppler-qt5.h>

#include <QDir>
#include <QSettings>
#include <QUrl>
#include <QDesktopServices>
#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
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
#include <QVBoxLayout>

PdfViewer *PdfViewer::s_instance = nullptr;

PdfViewer::PdfViewer(const QString &file)
    : QMainWindow()
{
    // register singleton
    s_instance = this;

    setWindowIcon(QIcon(":/firstaid.svg"));

    setStyleSheet(
        QString("QToolButton { border-width: 1px; border-radius: 6px; border-style: none; padding: 2px }"
                "QToolButton:hover { border-style: solid; border-color: gray; padding: 1px }"
                "QToolButton:focus { border-style: dotted; border-color: gray; padding: 1px }"
                "QToolButton:pressed { border-style: solid; border-color: gray; padding-left: 3px; padding-top: 3px; padding-right: 1px; padding-bottom: 1px }"
                "QToolButton:checked { border-style: solid; border-top-color: gray; border-left-color: gray; border-bottom-color: lightGray; border-right-color: lightGray; padding-left: 2px; padding-top: 2px; padding-right: 0px; "
                "padding-bottom: 0px }"
                "QToolButton::menu-indicator { image: url(empty.png) }"
                "QMenu { padding: 1px }"));

    // allow gui to potentially accept drops at all
    setAcceptDrops(true);

    // setup the menu action
    QMenu *menu = new QMenu(this);

    QAction *fileOpen = menu->addAction(QIcon(":/icons/document-open.svg"), tr("&Open..."), this, SLOT(slotOpenFile()));
    fileOpen->setShortcut(QKeySequence::Open);

    m_fileReloadAct = menu->addAction(QIcon(":/icons/view-refresh.svg"), tr("&Reload"), this, SLOT(slotReload()));
    m_fileReloadAct->setShortcut(QKeySequence::Refresh);
    menu->addSeparator();

    m_fileOpenExternalAct = menu->addAction(QIcon(":/icons/acrobat.svg"), tr("&Open in external PDF viewer"), this, SLOT(slotOpenFileExternal()));
    m_fileOpenExternalAct->setShortcut(Qt::CTRL + Qt::Key_E);

    m_filePrintAct = menu->addAction(QIcon(":/icons/document-print.svg"), tr("&Print..."), this, SLOT(slotPrint()));
    m_filePrintAct->setShortcut(QKeySequence::Print);
    menu->addSeparator();

    QAction *act = menu->addAction(QIcon(":/icons/help-about.svg"), tr("&About"), this, SLOT(slotAbout()));
    menu->addSeparator();

    act = menu->addAction(QIcon(":/icons/application-exit.svg"), tr("&Quit"), qApp, SLOT(closeAllWindows()));
    act->setShortcut(Qt::CTRL + Qt::Key_Q);

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

    connect(&m_document, SIGNAL(documentChanged()), &m_searchEngine, SLOT(reset()));

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
     * load document if one is passed
     * useful to have initial geometry based on content
     */
    if (!file.isEmpty())
        loadDocument(file);

    /**
     * restore old geometry & dock widget state, will handle "bad geometries", see http://doc.qt.io/qt-5.7/qmainwindow.html#restoreState
     */
    QSettings settings;
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    restoreState(settings.value("MainWindow/windowState").toByteArray());
}

PdfViewer::~PdfViewer()
{
    // close view early so no strange signals are handled
    delete m_view;

    // close document
    closeDocument();

    // unregister singleton
    s_instance = nullptr;
}

QSize PdfViewer::sizeHint() const
{
    return QSize(500, 600);
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

    // try to load the document
    Poppler::Document *newdoc = Poppler::Document::load(file);
    if (!newdoc || newdoc->isLocked()) {
        delete newdoc;
        QMessageBox::critical(this, tr("Cannot open file"), tr("Cannot open file '%1'.").arg(file));
        return;
    }

    // pass loaded poppler document to our internal one
    m_document.setDocument(newdoc);

    // set file + watch
    m_filePath = file;
    m_fileWatcher.addPath(m_filePath);

    // update action state & co.
    updateOnDocumentChange();

    QSettings settings;
    settings.beginGroup("Files");
    m_view->gotoPage(settings.value(m_filePath, 0).toInt());
    settings.endGroup();
}

void PdfViewer::closeDocument()
{
    if (!m_document.isValid())
        return;

    QSettings settings;
    settings.beginGroup("Files");
    settings.setValue(m_filePath, m_view->currentPage());
    settings.endGroup();

    m_document.setDocument(nullptr);

    // remove path
    m_fileWatcher.removePath(m_filePath);
    m_filePath.clear();

    // update action state & co.
    updateOnDocumentChange();
}

void PdfViewer::processCommand(const QString &command)
{
    if (command.startsWith("open "))
        loadDocument(command.mid(5));

    else if (command.startsWith("goto "))
        m_view->gotoDestination(command.mid(5));

    else if (command.startsWith("close"))
        qApp->quit();
}

void PdfViewer::closeEvent(QCloseEvent *event)
{
    /**
     * save geometry & dock widgets state, like show in http://doc.qt.io/qt-5.7/qmainwindow.html#saveState
     */
    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/windowState", saveState());
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
            QMessageBox::warning(this, "Error", "Failed to open file in external PDF viewer.");
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
    // let the user select the printer to use
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (!printDialog.exec())
        return;

    // determine range
    int fromPage = qMax(printer.fromPage(), 1);
    int toPage = qMin(printer.toPage(), m_document.numPages());

    // provide some feedback
    QProgressDialog pd("Printing...", "Abort", fromPage, toPage, this);
    pd.setWindowModality(Qt::WindowModal);

    // gather some information
    QRectF printerPageRect = printer.pageRect(QPrinter::DevicePixel);

    // print given range
    QPainter painter;
    painter.begin(&printer);
    for (int pageNumber = fromPage; pageNumber <= toPage; pageNumber++) {
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
        painter.drawImage(printerPageRect.topLeft(), image);

        // advance page
        if (pageNumber != printer.toPage())
            printer.newPage();
    }

    // flush
    painter.end();
}

void PdfViewer::slotAbout()
{
    /**
     * construct release info, if defined to something != ""
     */
    QString releaseInfo;
    if (!QString(FIRSTAID_RELEASE_STRING).isEmpty())
        releaseInfo = tr("<p><b>Version %1 Build %2<br>Tag %3</b></p>").arg(FIRSTAID_RELEASE_STRING).arg(FIRSTAID_BUILD_STRING).arg(FIRSTAID_TAG_STRING);

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
