/*
 * Copyright (C) 2008, Pino Toscano <pino@kde.org>
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

#pragma once

#include "document.h"
#include "pageview.h"
#include "searchengine.h"

#include <QFileSystemWatcher>
#include <QMainWindow>
#include <QTimer>

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
    /**
     * Construct viewer image, is a singleton.
     * @param file file to open, if not empty
     */
    PdfViewer(const QString &file);

    /**
     * Destruct the viewer window.
     */
    ~PdfViewer();

    /**
     * Access to global instance.
     * @return global instance
     */
    static PdfViewer *instance()
    {
        Q_ASSERT(s_instance);
        return s_instance;
    }

    /**
     * Access to global search instance.
     * @return global search instance
     */
    static SearchEngine *searchEngine()
    {
        Q_ASSERT(s_instance);
        return &s_instance->m_searchEngine;
    }

    /**
     * Access to global document.
     * @return document instance
     */
    static Document *document()
    {
        Q_ASSERT(s_instance);
        return &s_instance->m_document;
    }

    /**
     * Access to global view.
     * @return view instance
     */
    static PageView *view()
    {
        Q_ASSERT(s_instance);
        return s_instance->m_view;
    }

    QSize sizeHint() const override;

    void loadDocument(QString file, bool forceReload = false);
    void closeDocument();

    void closeEvent(QCloseEvent *e) override;

    /**
     * we want own menu, not default one
     */
    QMenu *createPopupMenu() override;

public slots:
    void processCommand(const QString &command);

private slots:
    void slotOpenFile();
    void slotOpenFileExternal();
    void slotReload();
    void slotDelayedReload();
    void slotPrint();
    void slotAbout();

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    /**
     * Update e.g. actions on document change
     */
    void updateOnDocumentChange();

private:
    /**
     *
     */
    static PdfViewer *s_instance;

    /**
     * full path to current open document, else empty
     */
    QString m_filePath;

    /**
     * watcher to auto-reload, does watch m_filePath
     */
    QFileSystemWatcher m_fileWatcher;

    /**
     * timer to trigger reload delayed for file watcher
     */
    QTimer m_fileWatcherReloadTimer;

    /**
     * registered observers for the document
     */
    QList<DocumentObserver *> m_observers;

    /**
     * action: open file in external PDF viewer
     */
    QAction *m_fileOpenExternalAct = nullptr;

    /**
     * action: reload file
     */
    QAction *m_fileReloadAct = nullptr;

    /**
     * action: print file
     */
    QAction *m_filePrintAct = nullptr;

    /**
     * document
     */
    Document m_document;

    /**
     * search engine
     */
    SearchEngine m_searchEngine;

    /**
     * PDF view, renders the pages
     */
    PageView *m_view = nullptr;
};
