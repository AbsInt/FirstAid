/*
 * Copyright (C) 2008, Pino Toscano <pino@kde.org>
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

#ifndef PDFVIEWER_H
#define PDFVIEWER_H

#include <QMainWindow>

#include "pageview.h"

class DocumentObserver;
class TocDock;
class QAction;
class QActionGroup;
class QLabel;
class QStackedWidget;
namespace Poppler
{
class Document;
}

class PdfViewer : public QMainWindow
{
    Q_OBJECT

    friend class DocumentObserver;

public:
    PdfViewer();
    ~PdfViewer();

    QSize sizeHint() const override;

    void loadDocument(const QString &file, bool forceReload = false);
    void closeDocument();

    void closeEvent(QCloseEvent *e) override;

public Q_SLOTS:
    void processCommand(const QString &command);

private Q_SLOTS:
    void slotOpenFileExternal();
    void slotPrint();
    void slotAbout();

    void slotSetZoom(qreal zoom);
    void slotSetZoomMode(PageView::ZoomMode mode);
    void slotGotoDestination(const QString &destination);
    void slotToggleContinous(bool on);
    void slotToggleFacingPages(bool on);
    void slotSetFocus();

    void slotCurrentPageChanged(int page);

private:
    void setPage(int page);
    int page() const;

    int m_currentPage;

    QAction *m_fileOpenExternalAct;
    QAction *m_filePrintAct;

    QList<DocumentObserver *> m_observers;

    Poppler::Document *m_doc;
    QString m_filePath;

    QStackedWidget *m_viewStack;
    TocDock *m_tocDock;

    QMenu *m_menu;
};

#endif
