/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
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

#include "pageview.h"
#include "searchengine.h"

#include <poppler-qt5.h>

#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QLabel>

ImageLabel::ImageLabel(QWidget *parent)
          : QLabel(parent)
{
    setMouseTracking(true);
}

ImageLabel::~ImageLabel()
{
    qDeleteAll(m_annotations);
}

void ImageLabel::setAnnotations(const QList<Poppler::Annotation *> &annotations)
{
    qDeleteAll(m_annotations);

    m_annotations=annotations;
}

void ImageLabel::mouseMoveEvent(QMouseEvent *e)
{
    qreal xPos=e->x()/(qreal)width();
    qreal yPos=e->y()/(qreal)height();

    bool linkFound=false;

    foreach (Poppler::Annotation *a, m_annotations) {
        if (a->boundary().contains(QPointF(xPos, yPos))) {
            linkFound=true;
            break;
        }
    }
    
    setCursor(linkFound ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void ImageLabel::mousePressEvent(QMouseEvent *e)
{
    qreal xPos=e->x()/(qreal)width();
    qreal yPos=e->y()/(qreal)height();

    foreach (Poppler::Annotation *a, m_annotations) {
        if (a->boundary().contains(QPointF(xPos, yPos))) {
            Poppler::Link *link=static_cast<Poppler::LinkAnnotation *>(a)->linkDestination();
            switch (link->linkType()) {
                case Poppler::Link::Goto:
                    emit gotoRequested(QString::number(static_cast<Poppler::LinkGoto *>(link)->destination().pageNumber()));
                    break;

                case Poppler::Link::Browse:
                    QDesktopServices::openUrl(QUrl(static_cast<Poppler::LinkBrowse *>(link)->url()));
                    break;

                default:
                    qDebug("Not yet handled link type %d.", link->linkType());
            }

            break;
        }
    }
}



PageView::PageView(QWidget *parent)
    : QScrollArea(parent)
    , m_zoom(1.0)
    , m_dpiX(QApplication::desktop()->physicalDpiX())
    , m_dpiY(QApplication::desktop()->physicalDpiY())
{
    // test
    m_dpiX=m_dpiY;

    m_imageLabel = new ImageLabel(this);
    m_imageLabel->resize(0, 0);
    setWidget(m_imageLabel);

    connect(m_imageLabel, SIGNAL(gotoRequested(QString)), SIGNAL(gotoRequested(QString)));
    connect(SearchEngine::globalInstance(), SIGNAL(highlightMatch(int,QRectF)), SLOT(slotHighlightMatch(int,QRectF)));
    connect(SearchEngine::globalInstance(), SIGNAL(matchesFound(int,QList<QRectF>)), SLOT(slotMatchesFound(int,QList<QRectF>)));
}

PageView::~PageView()
{
}

void PageView::documentLoaded()
{
}

void PageView::documentClosed()
{
    m_imageLabel->clear();
    m_imageLabel->resize(0, 0);
}

void PageView::pageChanged(int page)
{
    Poppler::Page *popplerPage = document()->page(page);
    const double resX = m_dpiX * m_zoom;
    const double resY = m_dpiY * m_zoom;

    // get desired information
    QImage image = popplerPage->renderToImage(resX, resY, -1, -1, -1, -1, Poppler::Page::Rotate0);
    QList<Poppler::Annotation *> annotations=popplerPage->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink);

    int matchPage;
    QRectF match;
    SearchEngine::globalInstance()->currentMatch(matchPage, match);

    if (!image.isNull()) {
        m_imageLabel->resize(image.size());

        // match furthe matches on page
        double sx=resX/72.0;
        double sy=resY/72.0;
        QPainter p(&image);
        p.setPen(Qt::NoPen);

        foreach (QRectF rect, SearchEngine::globalInstance()->matchesFor(page)) {
            QColor matchColor=QColor(255, 255, 0, 64);
            if (page==matchPage && rect==match)
                matchColor=QColor(255, 128, 0, 128);

            QRectF r=QRectF(rect.left()*sx, rect.top()*sy, rect.width()*sx, rect.height()*sy);
            r.adjust(-3, -5, 3, 2);
            p.fillRect(r, matchColor);
        }

        m_imageLabel->setPixmap(QPixmap::fromImage(image));
    } 
    else {
        m_imageLabel->resize(0, 0);
        m_imageLabel->setPixmap(QPixmap());
    }

    m_imageLabel->setAnnotations(annotations);

    delete popplerPage;
}

void PageView::slotZoomChanged(qreal value)
{
    m_zoom = value;
    if (!document()) {
        return;
    }
    reloadPage();
}

void PageView::slotHighlightMatch(int page, const QRectF &)
{
    setPage(page);
}

void PageView::slotMatchesFound(int page, const QList<QRectF> &)
{
    if (page == this->page())
        pageChanged(page);
}

#include "pageview.moc"
