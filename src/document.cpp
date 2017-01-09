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

#include "document.h"
#include "viewer.h"

Document::Document()
{
}

Document::~Document()
{
    // free data by setting no document
    setDocument(nullptr);
}

void Document::setDocument(Poppler::Document *document)
{
    // reset old content
    for (int i = 0; i < m_links.length(); ++i)
        qDeleteAll(m_links.at(i));
    m_links.clear();
    qDeleteAll(m_pages);
    m_pages.clear();
    m_title.clear();

    // remember poppler document, free old one
    delete m_document;
    m_document = document;

    // passing a nullptr is valid as it only resets the object
    if (m_document) {
        // remember title
        m_title = m_document->title();

        // set render hints
        m_document->setRenderHint(Poppler::Document::TextAntialiasing, true);
        m_document->setRenderHint(Poppler::Document::Antialiasing, true);
        m_document->setRenderBackend(Poppler::Document::SplashBackend);

        // reserve space for vectors
        const int count = m_document->numPages();
        m_pages.reserve(count);
        m_pageRects.reserve(count);
        m_links.reserve(count);

        // create all poppler pages and collect links on them
        for (int i = 0; i < count; ++i) {
            // get the page and remember it
            Poppler::Page *page = m_document->page(i);
            m_pages.append(page);

            // extract links from the page
            m_links.append(page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink));
        }
    }

    /**
     * trigger relayout
     */
    relayout();

    /**
     * document changed for good
     */
    emit documentChanged();
}

const QDomDocument *Document::toc() const
{
    if (!m_document)
        return nullptr;

    return m_document->toc();
}

QList<int> Document::visiblePages(const QRectF &rect) const
{
    QList<int> pages;

    for (int c = 0; c < m_pageRects.size(); c++)
        if (m_pageRects.at(c).intersects(rect))
            pages << c;

    return pages;
}

QRectF Document::pageRect(int page) const
{
    Q_ASSERT(page >= 0 && page < m_pageRects.length());
    return m_pageRects.at(page);
}

int Document::pageForPoint(const QPointF &point) const
{
    for (int c = 0; c < m_pageRects.size(); c++)
        if (m_pageRects.at(c).marginsAdded(QMarginsF(m_spacing, m_spacing, m_spacing, 0)).contains(point))
            return c;

    return -1;
}

Poppler::Page *Document::page(int page) const
{
    return m_pages.value(page, nullptr);
}

const QList<Poppler::Annotation *> &Document::links(int page) const
{
    Q_ASSERT(page >= 0 && page < m_links.length());
    return m_links.at(page);
}

Poppler::LinkDestination *Document::linkDestination(const QString &destination) const
{
    return m_document->linkDestination(destination);
}

void Document::setDoubleSided(bool on)
{
    /**
     * if value changed => relaout
     */
    if (on != m_doubleSided) {
        m_doubleSided = on;
        relayout();
    }
}

void Document::relayout()
{
    m_pageRects.clear();
    m_layoutSize = QSizeF();

    if (m_document == nullptr)
        return;

    QPointF offset(m_spacing, m_spacing);

    if (doubleSided()) {
        int currentPage = 0;
        while (currentPage < numPages()) {
            Poppler::Page *p = page(currentPage);
            QRectF pageRect = QRectF(QPointF(), p->pageSizeF());

            // special handling for first page in documents with more than 2 pages
            if (currentPage == 0 && numPages() > 2) {
                pageRect.translate(pageRect.width() / 2, 0);
                m_pageRects << pageRect.translated(offset);
            } else {
                m_pageRects << pageRect.translated(offset);
                currentPage++;

                if (currentPage < numPages()) {
                    p = page(currentPage);
                    pageRect = QRectF(QPointF(), p->pageSizeF()).translated(offset + QPointF(pageRect.width(), 0));
                    m_pageRects << pageRect;
                }
            }

            currentPage++;
            offset += QPointF(0, pageRect.height() + m_spacing);
        }
    } else {
        for (int i = 0; i < numPages(); ++i) {
            Poppler::Page *p = page(i);

            QRectF pageRect = QRectF(QPointF(), p->pageSizeF()).translated(offset);
            m_pageRects << pageRect;

            offset += QPointF(0, pageRect.height() + m_spacing);
        }
    }

    /**
     * compute full layout size
     */
    QRectF boundingRect;
    for (int c = 0; c < m_pageRects.size(); c++)
        boundingRect = boundingRect.united(m_pageRects.at(c));
    m_layoutSize = boundingRect.adjusted(0, 0, 2 * m_spacing, 2 * m_spacing).size();

    /**
     * e.g. view needs to update
     */
    emit layoutChanged();
}
