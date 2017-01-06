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
    reset();
}

bool Document::isValid() const
{
    return m_document != nullptr;
}

void Document::setDocument(Poppler::Document *document)
{
    // reset old content
    reset();

    // remember poppler document
    m_document = document;

    // passing a nullptr is valid as it only resets the object
    if (m_document) {
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

    relayout(false);

    emit documentChanged();
}

QString Document::title() const
{
    if (!m_document)
        return QString();

    return m_document->title();
}

const QDomDocument *Document::toc() const
{
    if (!m_document)
        return nullptr;

    return m_document->toc();
}

QSizeF Document::layoutSize() const
{
    if (m_pageRects.isEmpty())
        return QSizeF();

    const int spacing = 10;

    QRectF boundingRect = m_pageRects.first();
    boundingRect = boundingRect.united(m_pageRects.last());

    if (numPages() > 2)
        boundingRect = boundingRect.united(m_pageRects.at(1));

    return boundingRect.adjusted(0, 0, spacing, spacing).size();
}

int Document::numPages() const
{
    return m_pages.size();
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
        if (m_pageRects.at(c).contains(point))
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

void Document::relayout(bool emitSignal)
{
    m_pageRects.clear();

    if (m_document == nullptr)
        return;

    // TODO: for now we assume all pages have the same size
    PageView *view = PdfViewer::view();
    qreal spacing = 10;

    QPointF offset(spacing, spacing);

    if (view->doubleSided()) {
        int currentPage = 0;
        while (currentPage < numPages()) {
            Poppler::Page *p = page(currentPage);
            QRectF pageRect = QRectF(QPointF(), p->pageSizeF());

            // special handling for first page in documents with more than 2 pages
            if (currentPage == 0 && numPages() >2) {
                pageRect.translate(pageRect.width() / 2, 0);
                m_pageRects << pageRect.translated(offset);
            }
            else {
                m_pageRects << pageRect.translated(offset);
                currentPage++;

                if (currentPage < numPages()) {
                    p = page(currentPage);
                    pageRect = QRectF(QPointF(), p->pageSizeF()).translated(offset+QPointF(pageRect.width(), 0));
                    m_pageRects << pageRect;
                }
            }

            currentPage++;
            offset += QPointF(0, pageRect.height() + spacing);
        }
    }
    else {
        for (int i = 0; i < numPages(); ++i) {
            Poppler::Page *p = page(i);

            QRectF pageRect = QRectF(QPointF(), p->pageSizeF()).translated(offset);
            m_pageRects << pageRect;

            offset += QPointF(0, pageRect.height() + spacing);
        }
    }

    if (emitSignal)
        emit layoutChanged();
}

void Document::reset()
{
    for (int i = 0; i < m_links.length(); ++i)
        qDeleteAll(m_links.at(i));
    m_links.clear();
    qDeleteAll(m_pages);
    m_pages.clear();
    m_currentPage = -1;
    delete m_document;
    m_document = nullptr;
}
