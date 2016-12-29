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

#include "stdinreaderthread.h"

#include "findbar.h"
#include "navigationtoolbar.h"
#include "singlepageview.h"
#include "searchengine.h"
#include "toc.h"

#include <poppler-qt5.h>

#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

PdfViewer::PdfViewer()
         : QMainWindow(), m_currentPage(0), m_doc(0)
{
    setWindowTitle(tr("FirstAid"));
    setWindowIcon(QIcon(":/firstaid.png"));

    setStyleSheet(QString("QToolButton { border-width: 1px; border-radius: 6px; border-style: none; padding: 2px }"
                          "QToolButton:hover { border-style: solid; border-color: gray; padding: 1px }"
                          "QToolButton:focus { border-style: dotted; border-color: gray; padding: 1px }"
                          "QToolButton:pressed { border-style: solid; border-color: gray; padding-left: 3px; padding-top: 3px; padding-right: 1px; padding-bottom: 1px }"
                          "QToolButton:checked { border-style: solid; border-top-color: gray; border-left-color: gray; border-bottom-color: lightGray; border-right-color: lightGray; padding-left: 2px; padding-top: 2px; padding-right: 0px; padding-bottom: 0px }"
                          "QMenu { padding: 1px }"));

    // menuBar()->hide();

    m_thread=new StdinReaderThread(this);
    m_thread->start();

    // setup the menus
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileOpenExternalAct = fileMenu->addAction(tr("&Open in external PDF viewer"), this, SLOT(slotOpenFileExternal()));
    m_fileOpenExternalAct->setShortcut(Qt::CTRL + Qt::Key_E);
    m_fileOpenExternalAct->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addSeparator();
    QAction *act = fileMenu->addAction(tr("&Quit"), qApp, SLOT(closeAllWindows()));
    act->setShortcut(Qt::CTRL + Qt::Key_Q);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    act = helpMenu->addAction(tr("&About"), this, SLOT(slotAbout()));
    act = helpMenu->addAction(tr("About &Qt"), this, SLOT(slotAboutQt()));

    QWidget *w=new QWidget(this);
    QVBoxLayout *vbl=new QVBoxLayout(w);
    vbl->setContentsMargins(0, 0, 0, 0);

    m_viewStack=new QStackedWidget(w);
    vbl->addWidget(m_viewStack);

    FindBar *fb=new FindBar(w);
    m_observers.append(fb);
    vbl->addWidget(fb);

    setCentralWidget(w);

    SinglePageView *spv=new SinglePageView(this);
    m_viewStack->addWidget(spv);
    connect(spv, SIGNAL(currentPageChanged(int)), SLOT(slotCurrentPageChanged(int)));

    QLabel *l=new QLabel("Continous View", this);
    l->setAlignment(Qt::AlignCenter);
    m_viewStack->addWidget(l);

    m_tocDock = new TocDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_tocDock);
    m_observers.append(m_tocDock);

    NavigationToolBar *navbar = new NavigationToolBar(m_tocDock->toggleViewAction(), this);
    addToolBar(navbar);
    m_observers.append(navbar);

    SearchEngine *se=SearchEngine::globalInstance();
    m_observers << se;

    Q_FOREACH(DocumentObserver *obs, m_observers)
        obs->m_viewer = this;

    connect(navbar, SIGNAL(zoomChanged(qreal)), SLOT(slotSetZoom(qreal)));
    connect(navbar, SIGNAL(zoomModeChanged(PageView::ZoomMode)), SLOT(slotSetZoomMode(PageView::ZoomMode)));
    connect(navbar, SIGNAL(toggleContinous(bool)), SLOT(slotToggleContinous(bool)));

    connect(m_tocDock, SIGNAL(gotoRequested(QString)), SLOT(slotGotoDestination(QString)));

    // restore old geometry
    QRect r=QApplication::desktop()->availableGeometry(this);
    QSettings settings;
    settings.beginGroup("MainWindow");
    resize(settings.value("size", 3*r.size()/4).toSize());
    move(settings.value("pos", QPoint(r.width()/8, r.height()/8)).toPoint());
    m_tocDock->setVisible(settings.value("tocVisible", false).toBool());
    settings.endGroup();
}

PdfViewer::~PdfViewer()
{
    closeDocument();

    m_thread->terminate();
    m_thread->wait();
}

QSize PdfViewer::sizeHint() const
{
    return QSize(500, 600);
}

void PdfViewer::loadDocument(const QString &file, bool forceReload)
{
    if (file==m_filePath && !forceReload) {
        raise();
        activateWindow();
        return;
    }

    Poppler::Document *newdoc = Poppler::Document::load(file);
    if (!newdoc) {
        QMessageBox msgbox(QMessageBox::Critical, tr("Open Error"), tr("Cannot open:\n") + file,
                           QMessageBox::Ok, this);
        msgbox.exec();
        return;
    }

    while (newdoc->isLocked()) {
        bool ok = true;
        QString password = QInputDialog::getText(this, tr("Document Password"),
                                                 tr("Please insert the password of the document:"),
                                                 QLineEdit::Password, QString(), &ok);
        if (!ok) {
            delete newdoc;
            return;
        }
        newdoc->unlock(password.toLatin1(), password.toLatin1());
    }

    closeDocument();

    m_doc = newdoc;

    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderBackend(Poppler::Document::SplashBackend);

    static_cast<SinglePageView *>(m_viewStack->widget(0))->setDocument(m_doc);

    Q_FOREACH(DocumentObserver *obs, m_observers) {
        obs->documentLoaded();
        obs->pageChanged(0);
    }

    m_fileOpenExternalAct->setEnabled(true);
    m_filePath=file;

    if (m_doc->title().isEmpty())
        setWindowTitle(QFileInfo(m_filePath).fileName());
    else
        setWindowTitle(m_doc->title());
}

void PdfViewer::closeDocument()
{
    if (!m_doc)
        return;

    static_cast<SinglePageView *>(m_viewStack->widget(0))->setDocument(nullptr);

    Q_FOREACH(DocumentObserver *obs, m_observers)
        obs->documentClosed();

    m_currentPage = 0;
    delete m_doc;
    m_doc = 0;

    m_fileOpenExternalAct->setEnabled(false);
    m_filePath.clear();

    setWindowTitle(tr("FirstAid"));
}

bool PdfViewer::event(QEvent *e)
{
    if (QEvent::User == e->type()) {
        StdinReadEvent *sre=static_cast<StdinReadEvent *>(e);
        QString command=sre->text().trimmed();

        if (command.startsWith("open "))
            loadDocument(command.mid(5));

        else if (command.startsWith("goto "))
            slotGotoDestination(command.mid(5));

        else if (command.startsWith("close"))
            qApp->quit();

        return true;
    }
    else
        return QMainWindow::event(e);
}

void PdfViewer::closeEvent(QCloseEvent *event)
{
    // save geometry
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.setValue("tocVisible", m_tocDock->isVisible());
    settings.endGroup();

    QMainWindow::closeEvent(event);
}

void PdfViewer::slotOpenFileExternal()
{
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_filePath)))
        QMessageBox::warning(this, "Error", "Failed to open file in external PDF viewer.");
}

void PdfViewer::slotAbout()
{
    const QString text("FirstAid is a simple PDF viewer based on libpoppler.");
    QMessageBox::about(this, QString::fromLatin1("About FirstAid"), text);
}

void PdfViewer::slotAboutQt()
{
    QMessageBox::aboutQt(this);
}

void PdfViewer::slotSetZoom(qreal zoom)
{
    static_cast<SinglePageView *>(m_viewStack->widget(0))->setZoom(zoom);
}

void PdfViewer::slotSetZoomMode(PageView::ZoomMode mode)
{
    static_cast<SinglePageView *>(m_viewStack->widget(0))->setZoomMode(mode);
}

void PdfViewer::slotGotoDestination(const QString &destination)
{
    static_cast<SinglePageView *>(m_viewStack->widget(0))->gotoDestination(destination);
}

void PdfViewer::slotToggleContinous(bool on)
{
    m_viewStack->setCurrentIndex(on);
}

void PdfViewer::slotCurrentPageChanged(int page)
{
    Q_FOREACH(DocumentObserver *obs, m_observers)
        obs->pageChanged(page);
}

void PdfViewer::setPage(int page)
{
    if (!m_doc || 0>page || page>=m_doc->numPages())
        return;

    if (page == this->page())
        return;

    static_cast<SinglePageView *>(m_viewStack->widget(0))->gotoPage(page);
}

int PdfViewer::page() const
{
    return static_cast<SinglePageView *>(m_viewStack->currentWidget())->currentPage();
}

#include "viewer.moc"
