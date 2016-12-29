/*
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

/*
 * includes
 */

#include "pageview.h"

#include <poppler-qt5.h>

#include <QApplication>
#include <QDesktopWidget>

/*
 * class methods
 */

QColor PageView::matchColor()
{
    return QColor(255, 255, 0, 64);
}

QColor PageView::highlightColor()
{
    return QColor(255, 128, 0, 128);
}

/*
 * constructors / destructor
 */

PageView::PageView()
    : m_document(nullptr)
    , m_currentPage(0)
    , m_zoom(1.0)
    , m_doubleSideMode(None)
{
    m_dpiX = QApplication::desktop()->physicalDpiX();
    m_dpiY = QApplication::desktop()->physicalDpiY();

    // for now to have correct aspect ratio on windows
    m_dpiX = m_dpiY;
}

PageView::~PageView()
{
}

/*
 * public methods
 */

qreal PageView::resX() const
{
    return m_dpiX * m_zoom;
}

qreal PageView::resY() const
{
    return m_dpiY * m_zoom;
}

QRectF PageView::fromPoints(const QRectF &rect) const
{
    return QRectF(rect.left() / 72.0 * resX(), rect.top() / 72.0 * resY(), rect.width() / 72.0 * resX(), rect.height() / 72.0 * resY());
}

QRectF PageView::toPoints(const QRectF &rect) const
{
    return QRectF(rect.left() * 72.0 / resX(), rect.top() * 72.0 / resY(), rect.width() * 72.0 / resX(), rect.height() * 72.0 / resY());
}

void PageView::reset()
{
    m_currentPage = 0;
}

int PageView::currentPage() const
{
    return m_currentPage;
}

/*
 * public slots
 */

void PageView::setDocument(Poppler::Document *document)
{
    m_document = document;
    m_currentPage = 0;
    paint();
}

void PageView::setZoomMode(ZoomMode mode)
{
    if (mode != m_zoomMode) {
        m_zoomMode = mode;
        setSize(m_size);
    }
}

void PageView::setZoom(qreal zoom)
{
    m_zoomMode = Absolute;

    if (zoom != m_zoom) {
        m_zoom = zoom;
        paint();
    }
}

void PageView::setDoubleSideMode(DoubleSideMode mode)
{
    if (mode != m_doubleSideMode) {
        m_doubleSideMode = mode;
        setSize(m_size);
        paint();
    }
}

void PageView::setSize(const QSize &size)
{
    m_size = size;

    if (!m_document)
        return;

    if (FitWidth == m_zoomMode) {
        Poppler::Page *page = m_document->page(m_currentPage);
        QSizeF pageSize = page->pageSize();
        delete page;

        if (DoubleSided == m_doubleSideMode || (DoubleSidedNotFirst == m_doubleSideMode && m_currentPage > 0)) {
            if (Poppler::Page *page = m_document->page(m_currentPage + 1)) {
                pageSize.setWidth(pageSize.width() + page->pageSize().width());
                delete page;
            }
        }

        m_zoom = m_size.width() / (m_dpiX * pageSize.width() / 72.0);
        paint();
    } else if (FitPage == m_zoomMode) {
        Poppler::Page *page = m_document->page(m_currentPage);
        QSizeF pageSize = page->pageSize();
        delete page;

        if (DoubleSided == m_doubleSideMode || (DoubleSidedNotFirst == m_doubleSideMode && m_currentPage > 0)) {
            if (Poppler::Page *page = m_document->page(m_currentPage + 1)) {
                pageSize.setWidth(pageSize.width() + page->pageSize().width());
                delete page;
            }
        }

        qreal zx = m_size.width() / (m_dpiX * pageSize.width() / 72.0);
        qreal zy = m_size.height() / (m_dpiY * pageSize.height() / 72.0);

        m_zoom = qMin(zx, zy);
        paint();
    }
}

void PageView::gotoDestination(const QString &destination)
{
    bool ok;
    int pageNumber = destination.toInt(&ok);

    if (ok)
        gotoPage(pageNumber - 1);
    else if (m_document) {
        if (Poppler::LinkDestination *link = m_document->linkDestination(destination)) {
            gotoPage(link->pageNumber() - 1);
            delete link;
        }
    }
}

/*
 * eof
 */
