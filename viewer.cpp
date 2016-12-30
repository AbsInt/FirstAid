/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2008, Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2009, Shawn Rutledge <shawn.t.rutledge@gmail.com>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
 * Copyright (C) 2016, Marc Langenbach <mlangen@absint.com>
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

#include "findbar.h"
#include "navigationtoolbar.h"
#include "continouspageview.h"
#include "searchengine.h"
#include "stdinreaderthread.h"
#include "toc.h"

#include <poppler-qt5.h>

#include <QDir>
#include <QSettings>
#include <QUrl>
#include <QDesktopServices>
#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QProgressDialog>
#include <QStackedWidget>
#include <QVBoxLayout>

PdfViewer::PdfViewer(const QString &file)
    : QMainWindow()
    , m_currentPage(0)
    , m_doc(0)
{
    setWindowTitle(tr("FirstAid"));
    setWindowIcon(QIcon(":/firstaid.png"));

    setStyleSheet(
        QString("QToolButton { border-width: 1px; border-radius: 6px; border-style: none; padding: 2px }"
                "QToolButton:hover { border-style: solid; border-color: gray; padding: 1px }"
                "QToolButton:focus { border-style: dotted; border-color: gray; padding: 1px }"
                "QToolButton:pressed { border-style: solid; border-color: gray; padding-left: 3px; padding-top: 3px; padding-right: 1px; padding-bottom: 1px }"
                "QToolButton:checked { border-style: solid; border-top-color: gray; border-left-color: gray; border-bottom-color: lightGray; border-right-color: lightGray; padding-left: 2px; padding-top: 2px; padding-right: 0px; "
                "padding-bottom: 0px }"
                "QToolButton::menu-indicator { image: url(empty.png) }"
                "QMenu { padding: 1px }"));

    // setup the menu action
    m_menu = new QMenu(this);
    m_fileOpenExternalAct = m_menu->addAction(QIcon(":/icons/acrobat.svg"), tr("&Open in external PDF viewer"), this, SLOT(slotOpenFileExternal()));
    m_fileOpenExternalAct->setShortcut(Qt::CTRL + Qt::Key_E);

    m_fileReloadAct = m_menu->addAction(QIcon(":/icons/view-refresh.svg"), tr("&Reload"), this, SLOT(slotReload()));
    m_fileReloadAct->setShortcut(QKeySequence::Refresh);
    m_menu->addSeparator();

    m_filePrintAct = m_menu->addAction(QIcon(":/icons/document-print.svg"), tr("&Print..."), this, SLOT(slotPrint()));
    m_filePrintAct->setShortcut(QKeySequence::Print);
    m_menu->addSeparator();

    QAction *act = m_menu->addAction(QIcon(":/icons/help-about.svg"), tr("&About"), this, SLOT(slotAbout()));
    m_menu->addSeparator();

    act = m_menu->addAction(QIcon(":/icons/application-exit.svg"), tr("&Quit"), qApp, SLOT(closeAllWindows()));
    act->setShortcut(Qt::CTRL + Qt::Key_Q);

    QWidget *w = new QWidget(this);
    QVBoxLayout *vbl = new QVBoxLayout(w);
    vbl->setContentsMargins(0, 0, 0, 0);

    m_view = new ContinousPageView(this);
    vbl->addWidget(m_view);
    connect(m_view, SIGNAL(currentPageChanged(int)), SLOT(slotCurrentPageChanged(int)));
    setFocusProxy(m_view);

    FindBar *fb = new FindBar(w);
    m_observers.append(fb);
    vbl->addWidget(fb);

    setCentralWidget(w);

    m_tocDock = new TocDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_tocDock);
    m_observers.append(m_tocDock);

    NavigationToolBar *navbar = new NavigationToolBar(m_tocDock->toggleViewAction(), m_menu, this);
    addToolBar(navbar);
    m_observers.append(navbar);

    SearchEngine *se = SearchEngine::globalInstance();
    m_observers << se;

    foreach (DocumentObserver *obs, m_observers)
        obs->m_viewer = this;

    connect(navbar, SIGNAL(zoomChanged(qreal)), SLOT(slotSetZoom(qreal)));
    connect(navbar, SIGNAL(zoomModeChanged(PageView::ZoomMode)), SLOT(slotSetZoomMode(PageView::ZoomMode)));
    connect(navbar, SIGNAL(toggleFacingPages(bool)), SLOT(slotToggleFacingPages(bool)));

    connect(m_tocDock, SIGNAL(gotoRequested(QString)), SLOT(slotGotoDestination(QString)));

    /**
     * auto-reload
     */
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &PdfViewer::slotReload);

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
    closeDocument();
}

QSize PdfViewer::sizeHint() const
{
    return QSize(500, 600);
}

void PdfViewer::loadDocument(QString file, bool forceReload)
{
    if (file == m_filePath && !forceReload) {
        raise();
        activateWindow();
        return;
    }

    Poppler::Document *newdoc = Poppler::Document::load(file);
    if (!newdoc || newdoc->isLocked()) {
        QMessageBox msgbox(QMessageBox::Critical, tr("Open Error"), tr("Cannot open:\n") + file, QMessageBox::Ok, this);
        msgbox.exec();
        return;
    }

    closeDocument();

    m_doc = newdoc;

    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderBackend(Poppler::Document::SplashBackend);

    m_view->setDocument(m_doc);

    // set file + watch
    m_filePath = file;
    m_fileWatcher.addPath(m_filePath);

    QSettings settings;
    settings.beginGroup("Files");
    setPage(settings.value(m_filePath, 0).toInt());
    settings.endGroup();

    foreach (DocumentObserver *obs, m_observers) {
        obs->documentLoaded();
        obs->pageChanged(page());
    }

    // update action state & co.
    updateOnDocumentChange();
}

void PdfViewer::closeDocument()
{
    if (!m_doc)
        return;

    QSettings settings;
    settings.beginGroup("Files");
    settings.setValue(m_filePath, page());
    settings.endGroup();

    m_view->setDocument(nullptr);

    foreach (DocumentObserver *obs, m_observers)
        obs->documentClosed();

    m_currentPage = 0;
    delete m_doc;
    m_doc = 0;

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
        slotGotoDestination(command.mid(5));

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

void PdfViewer::slotPrint()
{
    // let the user select the printer to use
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (!printDialog.exec())
        return;

    // determine range
    int fromPage=qMax(printer.fromPage(), 1);
    int toPage=qMin(printer.toPage(), m_doc->numPages());

    // provide some feedback
    QProgressDialog pd("Printing...", "Abort", fromPage, toPage, this);
    pd.setWindowModality(Qt::WindowModal);

    // gather some information
    QRectF printerPageRect=printer.pageRect(QPrinter::DevicePixel);

    // print given range
    QPainter painter;
    painter.begin(&printer);
    for (int pageNumber=fromPage; pageNumber<=toPage; pageNumber++) {
        pd.setValue(pageNumber);
        if (pd.wasCanceled())
            break;

        // get page
        Poppler::Page *page=m_doc->page(pageNumber-1);

        // compute resolution so that page fits within the printable area
        QSizeF pageSize=page->pageSizeF();
        qreal scaleX=qMin(1.0, printerPageRect.width()/(72*pageSize.width()/printer.resolution()));
        qreal scaleY=qMin(1.0, printerPageRect.height()/(72*pageSize.height()/printer.resolution()));
        qreal res=printer.resolution()*qMin(scaleX, scaleY);

        // render page to image
        QImage image=page->renderToImage(res, res);

        // free page
        delete page;

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
    const QString text("FirstAid is a simple PDF viewer based on libpoppler.");
    QMessageBox::about(this, QString::fromLatin1("About FirstAid"), text);
}

void PdfViewer::slotSetZoom(qreal zoom)
{
    m_view->setZoom(zoom);
}

void PdfViewer::slotSetZoomMode(PageView::ZoomMode mode)
{
    m_view->setZoomMode(mode);
}

void PdfViewer::slotGotoDestination(const QString &destination)
{
    m_view->gotoDestination(destination);
}

void PdfViewer::slotToggleFacingPages(bool on)
{
    m_view->setDoubleSideMode(on ? PageView::DoubleSidedNotFirst : PageView::None);
}

void PdfViewer::slotCurrentPageChanged(int page)
{
    foreach (DocumentObserver *obs, m_observers)
        obs->pageChanged(page);
}

void PdfViewer::setPage(int page)
{
    if (!m_doc || 0 > page || page >= m_doc->numPages())
        return;

    if (page == this->page())
        return;

    m_view->gotoPage(page);
}

int PdfViewer::page() const
{
    return m_view->currentPage();
}

void PdfViewer::updateOnDocumentChange()
{
    /**
     * action states
     */
    m_fileOpenExternalAct->setEnabled(m_doc);
    m_fileReloadAct->setEnabled(m_doc);
    m_filePrintAct->setEnabled(m_doc);

    /**
     * Window title update
     */
    if (m_doc) {
        if (m_doc->title().isEmpty())
            setWindowTitle(QFileInfo(m_filePath).fileName());
        else
            setWindowTitle(m_doc->title());
    } else {
        setWindowTitle(tr("FirstAid"));
    }
}

QMenu *PdfViewer::createPopupMenu()
{
    return new QMenu();
}
