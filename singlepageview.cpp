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

/*
 * includes
 */

#include "singlepageview.h"
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



/*
 * helper class
 */

ImageLabel::ImageLabel(QWidget *parent)
          : QLabel(parent)
{
    setMouseTracking(true);
}



ImageLabel::~ImageLabel()
{
    qDeleteAll(m_annotations);
}



void
ImageLabel::setDisplayRect(const QRect &rect)
{
    m_displayRect=rect;
}



void
ImageLabel::setAnnotations(const QList<Poppler::Annotation *> &annotations)
{
    qDeleteAll(m_annotations);

    m_annotations=annotations;
}



void
ImageLabel::mouseMoveEvent(QMouseEvent *e)
{
    qreal xPos=(e->x()-m_displayRect.x())/(qreal)m_displayRect.width();
    qreal yPos=(e->y()-m_displayRect.y())/(qreal)m_displayRect.height();
    QPointF p=QPointF(xPos, yPos);

    bool linkFound=false;

    foreach (Poppler::Annotation *a, m_annotations) {
        if (a->boundary().contains(p)) {
            linkFound=true;
            break;
        }
    }
    
    setCursor(linkFound ? Qt::PointingHandCursor : Qt::ArrowCursor);
}



void
ImageLabel::mousePressEvent(QMouseEvent *e)
{
    qreal xPos=(e->x()-m_displayRect.x())/(qreal)m_displayRect.width();
    qreal yPos=(e->y()-m_displayRect.y())/(qreal)m_displayRect.height();
    QPointF p=QPointF(xPos, yPos);

    foreach (Poppler::Annotation *a, m_annotations) {
        if (a->boundary().contains(p)) {
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



/*
 * constructors / destructor
 */

SinglePageView::SinglePageView(QWidget *parent)
              : QScrollArea(parent)
              , PageView()
{
    m_imageLabel=new ImageLabel(this);
    m_imageLabel->resize(0, 0);
    setWidget(m_imageLabel);

    connect(m_imageLabel, SIGNAL(gotoRequested(QString)), SLOT(slotGotoRequested(QString)));

    SearchEngine *se=SearchEngine::globalInstance();
    connect(se, SIGNAL(started()), SLOT(slotFindStarted()));
    connect(se, SIGNAL(highlightMatch(int,QRectF)), SLOT(slotHighlightMatch(int,QRectF)));
    connect(se, SIGNAL(matchesFound(int,QList<QRectF>)), SLOT(slotMatchesFound(int,QList<QRectF>)));

    reset();
}



SinglePageView::~SinglePageView()
{
}



/*
 * public methods
 */

void
SinglePageView::gotoPage(int page)
{
    m_currentPage=page;
    paint();
}



/*
 * protected methods
 */

void
SinglePageView::paint()
{
    QImage image;
    QList<Poppler::Annotation *> annotations;

    if (m_document) {
        if (Poppler::Page *page=m_document->page(m_currentPage)) {
            image=page->renderToImage(resX(), resY(), -1, -1, -1, -1, Poppler::Page::Rotate0);
            annotations=page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink);
            delete page;
        }
    }

    SearchEngine *se=SearchEngine::globalInstance();
    int matchPage;
    QRectF matchRect;
    se->currentMatch(matchPage, matchRect);

    if (image.isNull()) {
        m_imageLabel->resize(0, 0);
        m_imageLabel->setPixmap(QPixmap());
    }
    else {
        // match furthe matches on page
        double sx=resX()/72.0;
        double sy=resY()/72.0;
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        //p.setPen(Qt::NoPen);

        foreach (QRectF rect, se->matchesFor(m_currentPage)) {
            QColor matchColor=QColor(255, 255, 0, 64);
            if (m_currentPage==matchPage && rect==matchRect)
                matchColor=QColor(255, 128, 0, 128);

            QRectF r=QRectF(rect.left()*sx, rect.top()*sy, rect.width()*sx, rect.height()*sy);
            r.adjust(-3, -5, 3, 2);
            p.fillRect(r, matchColor);
        }
        p.end();

        QRect displayRect(QPoint(), image.size());

        QSize vs=viewport()->size();
        if (vs.width()>image.width() || vs.height()>image.height()) {
            int xOffset=qMax(0, vs.width()-image.width())/2;
            int yOffset=qMax(0, vs.height()-image.height())/2;
            QPoint offset(xOffset, yOffset);

            QImage centeredImage(image.size().expandedTo(viewport()->size()), QImage::Format_ARGB32_Premultiplied);
            centeredImage.fill(Qt::gray);

            QPainter p(&centeredImage);
            p.setRenderHint(QPainter::Antialiasing);
            p.drawImage(offset, image);
            p.setPen(Qt::darkGray);
            p.drawRect(QRect(offset, image.size()).adjusted(-1, -1, 1, 1));
            p.end();

            image=centeredImage;
            displayRect.moveTo(offset);
        }

        m_imageLabel->resize(image.size());
        m_imageLabel->setPixmap(QPixmap::fromImage(image));
        m_imageLabel->setDisplayRect(displayRect);
    } 

    m_imageLabel->setAnnotations(annotations);
}



void
SinglePageView::resizeEvent(QResizeEvent *resizeEvent)
{
    QScrollArea::resizeEvent(resizeEvent);
    paint();
}



/*
 * protected slots
 */

void
SinglePageView::slotGotoRequested(const QString &destination)
{
    gotoDestination(destination);
}



void
SinglePageView::slotFindStarted()
{
    paint();
}



void
SinglePageView::slotHighlightMatch(int page, const QRectF &)
{
    gotoPage(page);
}



void
SinglePageView::slotMatchesFound(int page, const QList<QRectF> &)
{
    if (page == m_currentPage)
        paint();
}



#include "singlepageview.moc"

/*
 * eof
 */
