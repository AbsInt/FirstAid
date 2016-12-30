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
#include <QFileSystemWatcher>

#include "pageview.h"

class ContinousPageView;
class DocumentObserver;
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
    PdfViewer(const QString &file);
    ~PdfViewer();

    QSize sizeHint() const override;

    void loadDocument(QString file, bool forceReload = false);
    void closeDocument();

    void closeEvent(QCloseEvent *e) override;

    /**
     * we want own menu, not default one
     */
    QMenu *createPopupMenu() override;

public Q_SLOTS:
    void processCommand(const QString &command);

private Q_SLOTS:
    void slotOpenFileExternal();
    void slotReload();
    void slotPrint();
    void slotAbout();

    void slotSetZoom(qreal zoom);
    void slotSetZoomMode(PageView::ZoomMode mode);
    void slotGotoDestination(const QString &destination);
    void slotToggleFacingPages(bool on);

    void slotCurrentPageChanged(int page);

private:
    void setPage(int page);
    int page() const;

    /**
     * Update e.g. actions on document change
     */
    void updateOnDocumentChange();

    QAction *m_fileOpenExternalAct;
    QAction *m_fileReloadAct;
    QAction *m_filePrintAct;

    ContinousPageView *m_view;
    QList<DocumentObserver *> m_observers;

    Poppler::Document *m_doc;
    QString m_filePath;

    /**
     * watcher to auto-reload
     */
    QFileSystemWatcher m_fileWatcher;
};

#endif
