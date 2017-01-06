/*
 * Copyright (C) 2016, Jan Pohland <pohland@absint.com>
 * Copyright (C) 2016, Christoph Cullmann <cullmann@absint.com>
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

#pragma once

#include <poppler-qt5.h>

#include <QObject>

class Document : public QObject
{
    Q_OBJECT

public:
    Document();
    ~Document();

    /*! Returns true if we are holding a valid Poppler document */
    bool isValid() const;

    /*! Set Poppler document to use, any old data will be deleted. */
    void setDocument(Poppler::Document *document);

    /*! Set current page. */
    void setCurrentPage(int page);

    int currentPage() const;
    int numPages() const;
    Poppler::Page *page(int page);
    const QDomDocument *toc();
    Poppler::LinkDestination *linkDestination(const QString &destination);
    const QList<Poppler::Annotation *> &annotations(int page);

    QString title();

signals:
    void documentChanged();
    void pageChanged(int page);

private:
    void reset();

private:
    /**
     * current open poppler document
     */
    Poppler::Document *m_document = nullptr;

    /**
     * the current page - needed for synchronization with other views
     */
    int m_currentPage = -1;

    /**
     * list of cached poppler pages, index == page
     */
    QList<Poppler::Page *> m_pages;

    /**
     * list of cached poppler annotations, index == page
     */
    QList<QList<Poppler::Annotation *>> m_annotations;
};
