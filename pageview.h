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

#pragma once

#include <QColor>
#include <QRectF>

namespace Poppler
{
class Document;
}

class PageView
{
public:
    enum ZoomMode { FitWidth, FitPage, Absolute };
    enum DoubleSideMode { None, DoubleSided, DoubleSidedNotFirst };

    static QColor matchColor();
    static QColor highlightColor();

    PageView();
    virtual ~PageView();

    qreal resX() const;
    qreal resY() const;

    QRectF fromPoints(const QRectF &rect) const;
    QRectF toPoints(const QRectF &rect) const;

public:
    void setSize(const QSize &size);

    virtual void setDocument(Poppler::Document *document);
    virtual void setDoubleSideMode(DoubleSideMode mode);
    virtual void setZoomMode(ZoomMode mode);
    virtual void setZoom(qreal zoom);
    virtual void reset();
    virtual int currentPage() const;
    virtual void gotoPage(int page) = 0;
    virtual void gotoDestination(const QString &destination);

protected:
    virtual void paint() = 0;

protected:
    Poppler::Document *m_document;
    int m_dpiX;
    int m_dpiY;
    int m_currentPage;
    ZoomMode m_zoomMode;
    qreal m_zoom;
    DoubleSideMode m_doubleSideMode;
    QSize m_size;
};
