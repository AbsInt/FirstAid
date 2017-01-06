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

    /*! Returns true if we are holding a valid Poppler document. */
    bool isValid() const;

    /*! Set Poppler document to use, any old data will be deleted. */
    void setDocument(Poppler::Document *document);

    /*! Returns document title */
    QString title() const;

    /*! Returns the table of contents of nullptr. */
    const QDomDocument *toc() const;

    /*! Returns the number of available pages. */
    int numPages() const;

    /*! Returns a rectangle descriping the page's position in the viewport. */
    QRectF pageRect(int page) const;

    /*! Returns a Poppler page for the given page number or nullptr. */
    Poppler::Page *page(int page) const;

    /*! Returns a list of links found on the given page number. */
    const QList<Poppler::Annotation *> &links(int page) const;

    /*! Returns a link destination for the given name or nullptr. */
    Poppler::LinkDestination *linkDestination(const QString &destination) const;

    /*! Performa a relayout of the current document. */
    void relayout(bool emitSignal=true);

signals:
    void documentChanged();
    void layoutChanged();

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
     * vector of page rectangles in the viewport, index == page
     */
    QVector<QRectF> m_pageRects;

    /**
     * vector of cached poppler pages, index == page
     */
    QVector<Poppler::Page *> m_pages;

    /**
     * vector of cached poppler annotation of type link, index == page
     */
    QVector<QList<Poppler::Annotation *>> m_links;
};
