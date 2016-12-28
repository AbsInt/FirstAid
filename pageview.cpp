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
 * constructors / destructor
 */

PageView::PageView()
        : m_document(nullptr)
        , m_currentPage(0)
        , m_zoom(1.0)
{
    m_dpiX=QApplication::desktop()->physicalDpiX();
    m_dpiY=QApplication::desktop()->physicalDpiY();

    // for now to have correct aspect ratio on windows
    m_dpiX=m_dpiY;
}



PageView::~PageView()
{
}



/*
 * public methods
 */

double
PageView::resX() const
{
    return m_dpiX*m_zoom;
}



double
PageView::resY() const
{
    return m_dpiY*m_zoom;
}



void
PageView::reset()
{
    m_currentPage=0;
}



int
PageView::currentPage() const
{
    return m_currentPage;
}



/*
 * public slots
 */

void
PageView::setDocument(Poppler::Document *document)
{
    m_document=document;
    m_currentPage=0;
    paint();
}



void
PageView::setZoom(qreal zoom)
{
    if (zoom != m_zoom) {
        m_zoom=zoom;
        paint();
    }
}



void
PageView::gotoDestination(const QString &destination)
{
    bool ok;
    int pageNumber=destination.toInt(&ok);

    if (ok)
        gotoPage(pageNumber-1);
    else if (m_document) {
        if (Poppler::LinkDestination *link=m_document->linkDestination(destination)) {
            gotoPage(link->pageNumber()-1);
            delete link;
        }
    }
}



/*
 * eof
 */
