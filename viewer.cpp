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

#include "navigationtoolbar.h"
#include "pageview.h"
#include "toc.h"

#include <poppler-qt5.h>

#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>

PdfViewer::PdfViewer()
    : QMainWindow(), m_currentPage(0), m_doc(0)
{
    setWindowTitle(tr("FirstAid"));
    setWindowIcon(QIcon(":/firstaid.png"));

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

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    m_settingsTextAAAct = settingsMenu->addAction(tr("Text Antialias"));
    m_settingsTextAAAct->setCheckable(true);
    connect(m_settingsTextAAAct, SIGNAL(toggled(bool)), this, SLOT(slotToggleTextAA(bool)));
    m_settingsGfxAAAct = settingsMenu->addAction(tr("Graphics Antialias"));
    m_settingsGfxAAAct->setCheckable(true);
    connect(m_settingsGfxAAAct, SIGNAL(toggled(bool)), this, SLOT(slotToggleGfxAA(bool)));
    QMenu *settingsRenderMenu = settingsMenu->addMenu(tr("Render Backend"));
    m_settingsRenderBackendGrp = new QActionGroup(settingsRenderMenu);
    m_settingsRenderBackendGrp->setExclusive(true);
    act = settingsRenderMenu->addAction(tr("Splash"));
    act->setCheckable(true);
    act->setChecked(true);
    act->setData(qVariantFromValue(0));
    m_settingsRenderBackendGrp->addAction(act);
    act = settingsRenderMenu->addAction(tr("Arthur"));
    act->setCheckable(true);
    act->setData(qVariantFromValue(1));
    m_settingsRenderBackendGrp->addAction(act);
    connect(m_settingsRenderBackendGrp, SIGNAL(triggered(QAction*)),
            this, SLOT(slotRenderBackend(QAction*)));

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    act = helpMenu->addAction(tr("&About"), this, SLOT(slotAbout()));
    act = helpMenu->addAction(tr("About &Qt"), this, SLOT(slotAboutQt()));

    NavigationToolBar *navbar = new NavigationToolBar(this);
    addToolBar(navbar);
    m_observers.append(navbar);

    PageView *view = new PageView(this);
    setCentralWidget(view);
    m_observers.append(view);

    TocDock *tocDock = new TocDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, tocDock);
    tocDock->hide();
    viewMenu->addAction(tocDock->toggleViewAction());
    m_observers.append(tocDock);

    Q_FOREACH(DocumentObserver *obs, m_observers)
        obs->m_viewer = this;

    connect(navbar, SIGNAL(zoomChanged(qreal)), view, SLOT(slotZoomChanged(qreal)));
    connect(navbar, SIGNAL(rotationChanged(int)), view, SLOT(slotRotationChanged(int)));
    connect(tocDock, SIGNAL(gotoRequested(QString)), SLOT(slotGoto(QString)));

    // activate AA by default
    m_settingsTextAAAct->setChecked(true);
    m_settingsGfxAAAct->setChecked(true);
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

void PdfViewer::loadDocument(const QString &file)
{
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

    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, m_settingsTextAAAct->isChecked());
    m_doc->setRenderHint(Poppler::Document::Antialiasing, m_settingsGfxAAAct->isChecked());
    m_doc->setRenderBackend((Poppler::Document::RenderBackend)m_settingsRenderBackendGrp->checkedAction()->data().toInt());

    Q_FOREACH(DocumentObserver *obs, m_observers) {
        obs->documentLoaded();
        obs->pageChanged(0);
    }

    m_fileOpenExternalAct->setEnabled(true);
    m_filePath=file;

    setWindowTitle(QFileInfo(file).fileName());
}

void PdfViewer::closeDocument()
{
    if (!m_doc)
        return;

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

        if (command.startsWith("open:"))
            loadDocument(command.mid(5));

        else if (command.startsWith("goto:"))
            slotGoto(command.mid(5));

        else if (command.startsWith("close"))
            qApp->quit();

        return true;
    }
    else
        return QMainWindow::event(e);
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

void PdfViewer::slotToggleTextAA(bool value)
{
    if (!m_doc) {
        return;
    }

    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, value);

    Q_FOREACH(DocumentObserver *obs, m_observers) {
        obs->pageChanged(m_currentPage);
    }
}

void PdfViewer::slotToggleGfxAA(bool value)
{
    if (!m_doc) {
        return;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing, value);

    Q_FOREACH(DocumentObserver *obs, m_observers) {
        obs->pageChanged(m_currentPage);
    }
}

void PdfViewer::slotRenderBackend(QAction *act)
{
    if (!m_doc || !act) {
        return;
    }

    m_doc->setRenderBackend((Poppler::Document::RenderBackend)act->data().toInt());

    Q_FOREACH(DocumentObserver *obs, m_observers) {
        obs->pageChanged(m_currentPage);
    }
}

void PdfViewer::slotGoto(const QString &dest)
{
    bool ok;
    int pageNumber=dest.toInt(&ok);
    if (ok)
        setPage(pageNumber-1);
    else if (Poppler::LinkDestination *link=m_doc->linkDestination(dest)) {
        setPage(link->pageNumber()-1);
        delete link;
    }
}

void PdfViewer::setPage(int page)
{
    if (!m_doc || 0>page || page>=m_doc->numPages())
        return;

    Q_FOREACH(DocumentObserver *obs, m_observers)
        obs->pageChanged(page);

    m_currentPage = page;
}

int PdfViewer::page() const
{
    return m_currentPage;
}

#include "viewer.moc"
