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
    // free data
    reset();
}

void Document::setDocument(std::unique_ptr<Poppler::Document> &&document)
{
    // reset old content
    reset();

    // remember new poppler document
    m_document = std::move(document);

    // passing a nullptr is valid as it only resets the object
    if (m_document) {
        // remember title
        m_title = m_document->title();

        // set render hints
        m_document->setRenderHint(Poppler::Document::TextAntialiasing, true);
        m_document->setRenderHint(Poppler::Document::Antialiasing, true);
        m_document->setRenderBackend(Poppler::Document::SplashBackend);

        // create all poppler pages and collect links on them
        for (int i = 0; i < m_document->numPages(); ++i) {
            // skip invalid pages
            std::unique_ptr<Poppler::Page> page = m_document->page(i);
            if (!page)
                continue;

            // extract links from the page
            auto links = page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink << Poppler::Annotation::AText << Poppler::Annotation::ACaret);
            m_links.emplace_back(std::move(links));

            // remember the page
            m_pages.emplace_back(std::move(page));
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

QVector<Poppler::OutlineItem> Document::toc() const
{
    if (!m_document)
        return QVector<Poppler::OutlineItem>();

    return m_document->outline();
}

QList<int> Document::visiblePages(const QRectF &rect) const
{
    QList<int> pages;

    for (int c = 0; c < m_pageRects.size(); c++)
        if (m_pageRects.at(c).intersects(rect))
            pages << c;

    return pages;
}

QRectF Document::pageRect(int page, bool addMargins) const
{
    Q_ASSERT(page >= 0 && page < m_pageRects.size());
    return addMargins ? m_pageRects.at(page).marginsAdded(QMarginsF(m_spacing, m_spacing, 0, 0)) : m_pageRects.at(page);
}

int Document::pageForPoint(const QPointF &point) const
{
    for (int c = 0; c < m_pageRects.size(); c++)
        if (m_pageRects.at(c).marginsAdded(QMarginsF(m_spacing, m_spacing, m_spacing, 0)).contains(point))
            return c;

    return -1;
}

int Document::pageForRect(const QRectF &rect) const
{
    int foundPage = -1;
    qreal currentArea = 0;

    for (int page = 0; page < numPages(); page++) {
        QRectF r = rect.intersected(pageRect(page));
        if (currentArea < r.width() * r.height()) {
            currentArea = r.width() * r.height();
            foundPage = page;
        }
    }

    return foundPage;
}

Poppler::Page *Document::page(int page) const
{
    if (page >= 0 && size_t(page) < m_pages.size())
        return m_pages.at(page).get();

    return nullptr;
}

const std::vector<std::unique_ptr<Poppler::Annotation>> &Document::links(int page) const
{
    Q_ASSERT(page >= 0 && size_t(page) < m_links.size());
    return m_links.at(page);
}

std::unique_ptr<Poppler::LinkDestination> Document::linkDestination(const QString &destination) const
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
        qreal leftMaxWidth = 0.0;

        int currentPage = 0;
        while (currentPage < numPages()) {
            Poppler::Page *p = page(currentPage);
            QRectF pageRect = QRectF(QPointF(), p->pageSizeF());
            qreal pageHeight = pageRect.height();

            // special handling for first page in documents with more than 2 pages
            if (currentPage == 0 && numPages() > 2) {
                pageRect.translate(pageRect.width() / 2, 0);
                m_pageRects << pageRect.translated(offset);
            } else {
                m_pageRects << pageRect.translated(offset);
                leftMaxWidth = qMax(leftMaxWidth, pageRect.width());
                currentPage++;

                // check right side, center last page if single
                if (currentPage < numPages()) {
                    p = page(currentPage);
                    pageRect = QRectF(QPointF(), p->pageSizeF()).translated(offset + QPointF(pageRect.width(), 0));

                    // ensure pages are vertically centered
                    if (pageRect.height() > m_pageRects.last().height()) {
                        m_pageRects[m_pageRects.count() - 1].translate(0, (pageRect.height() - m_pageRects.last().height()) / 2.0);
                        pageHeight = pageRect.height();
                    } else if (pageRect.height() < m_pageRects.last().height())
                        pageRect.translate(0, (m_pageRects.last().height() - pageRect.height()) / 2.0);

                    m_pageRects << pageRect;
                } else
                    m_pageRects[m_pageRects.count() - 1].translate(pageRect.width() / 2, 0);
            }

            currentPage++;
            offset += QPointF(0, pageHeight + m_spacing);
        }

        // now we can center the page rects according to the maximum width
        currentPage = 0;

        // special handling for first page in documents with more than 2 pages
        if (numPages() > 2) {
            m_pageRects[0].translate(leftMaxWidth - m_pageRects.first().width(), 0);
            currentPage++;
        }

        while (currentPage < numPages()) {
            int leftPageWidth = m_pageRects.at(currentPage).width();

            if (leftPageWidth < leftMaxWidth) {
                m_pageRects[currentPage].translate(leftMaxWidth - leftPageWidth, 0);

                if (currentPage + 1 < numPages())
                    m_pageRects[currentPage + 1].translate(leftMaxWidth - leftPageWidth, 0);
            }

            // advance to the next left side page
            currentPage += 2;
        }
    } else {
        // layout pages from top to bottom and determine maximum width
        qreal maxWidth = 0.0;

        for (int i = 0; i < numPages(); ++i) {
            Poppler::Page *p = page(i);

            QRectF pageRect = QRectF(QPointF(), p->pageSizeF()).translated(offset);
            m_pageRects << pageRect;

            offset += QPointF(0, pageRect.height() + m_spacing);

            maxWidth = qMax(maxWidth, pageRect.width());
        }

        // now we can center the page rects according to the maximum width
        for (int i = 0; i < numPages(); ++i) {
            if (m_pageRects.at(i).width() < maxWidth)
                m_pageRects[i].translate((maxWidth - m_pageRects.at(i).width()) / 2.0, 0);
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

void Document::reset()
{
    m_links.clear();
    m_pages.clear();
    m_title.clear();
    m_document.reset();
}
