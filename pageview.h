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

#ifndef PAGEVIEW_H
#define PAGEVIEW_H

#include <Qt>

namespace Poppler {
class Document;
}

class PageView
{
public:
    PageView();
    virtual ~PageView();

    double resX() const;
    double resY() const;

public:
    void setDocument(Poppler::Document *document);
    void setZoom(qreal zoom);

    virtual void reset();
    virtual int currentPage() const;
    virtual void gotoPage(int page)=0;
    virtual void gotoDestination(const QString &destination);

protected:
    virtual void paint()=0;

protected:
    Poppler::Document *m_document;
    int m_dpiX;
    int m_dpiY;
    int m_currentPage;
    qreal m_zoom;
};

#endif // #ifndef PAGEVIEW_H
