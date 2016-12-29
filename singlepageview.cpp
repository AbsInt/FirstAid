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

#include <QtGui/QClipboard>
#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QRubberBand>
#include <QtWidgets/QScrollBar>



/*
 * helper class
 */

ImageLabel::ImageLabel(QWidget *parent)
          : QLabel(parent)
{
    m_rubberBand=new QRubberBand(QRubberBand::Rectangle, this);

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
ImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    qreal xPos=(event->x()-m_displayRect.x())/(qreal)m_displayRect.width();
    qreal yPos=(event->y()-m_displayRect.y())/(qreal)m_displayRect.height();
    QPointF p=QPointF(xPos, yPos);

    foreach (Poppler::Annotation *a, m_annotations) {
        if (a->boundary().contains(p)) {
            setCursor(Qt::PointingHandCursor);
            return;
        }
    }
    
    setCursor(Qt::ArrowCursor);

    if (!m_rubberBandOrigin.isNull())
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, event->pos()).normalized());
}



void
ImageLabel::mousePressEvent(QMouseEvent *event)
{
    qreal xPos=(event->x()-m_displayRect.x())/(qreal)m_displayRect.width();
    qreal yPos=(event->y()-m_displayRect.y())/(qreal)m_displayRect.height();
    QPointF p=QPointF(xPos, yPos);

    foreach (Poppler::Annotation *a, m_annotations) {
        if (a->boundary().contains(p)) {
            Poppler::Link *link=static_cast<Poppler::LinkAnnotation *>(a)->linkDestination();
            switch (link->linkType()) {
                case Poppler::Link::Goto:
                    emit gotoRequested(QString::number(static_cast<Poppler::LinkGoto *>(link)->destination().pageNumber()));
                    return;

                case Poppler::Link::Browse:
                    QDesktopServices::openUrl(QUrl(static_cast<Poppler::LinkBrowse *>(link)->url()));
                    return;

                default:
                    qDebug("Not yet handled link type %d.", link->linkType());
                    return;
            }

            break;
        }
    }

    m_rubberBandOrigin=event->pos();
    m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
    m_rubberBand->show();
}



void
ImageLabel::mouseReleaseEvent(QMouseEvent *)
{
    m_rubberBandOrigin=QPoint(0, 0);
    m_rubberBand->hide();

    emit copyRequested(m_rubberBand->geometry().intersected(m_displayRect).translated(-m_displayRect.topLeft()));
}



/*
 * constructors / destructor
 */

SinglePageView::SinglePageView(QWidget *parent)
              : QScrollArea(parent)
              , PageView()
{
    setMouseTracking(true);

    m_imageLabel=new ImageLabel(this);
    m_imageLabel->resize(0, 0);
    setWidget(m_imageLabel);

    connect(m_imageLabel, SIGNAL(gotoRequested(QString)), SLOT(slotGotoRequested(QString)));
    connect(m_imageLabel, SIGNAL(copyRequested(QRectF)), SLOT(slotCopyRequested(QRectF)));

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
    if (!m_document || page<0 || page>=m_document->numPages())
        return;

    m_currentPage=page;
    paint();

    emit currentPageChanged(m_currentPage);
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
        // mark further matches on page
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);

        foreach (QRectF rect, se->matchesFor(m_currentPage)) {
            QColor c=matchColor();
            if (m_currentPage==matchPage && rect==matchRect)
                c=highlightColor();

            QRectF r=fromPoints(rect);
            r.adjust(-3, -5, 3, 2);
            p.fillRect(r, c);
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
SinglePageView::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);

    setSize(viewport()->size()-QSize(1, 1));

    paint();
}




void
SinglePageView::keyPressEvent(QKeyEvent *event)
{
    QScrollBar *vsb=verticalScrollBar();

    if (m_document && event->key()==Qt::Key_PageDown && (!vsb->isVisible() || vsb->value()==vsb->maximum())) {
        if (m_currentPage < m_document->numPages()-1) {
            gotoPage(m_currentPage+1);
            verticalScrollBar()->setValue(0);
            return;
        }
    }

    if (m_document && event->key()==Qt::Key_PageUp && (!vsb->isVisible() || vsb->value()==0)) {
        if (m_currentPage > 0) {
            gotoPage(currentPage()-1);
            verticalScrollBar()->setValue(verticalScrollBar()->maximum());
            return;
        }
    }

    QScrollArea::keyPressEvent(event);
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
SinglePageView::slotCopyRequested(const QRectF &rect)
{
    if (rect.isNull())
        return;

    QMenu m(this);
    QAction *copyAction=m.addAction("Copy");

    if (copyAction != m.exec(QCursor::pos()))
        return;

    if (Poppler::Page *page=m_document->page(m_currentPage)) {
        QRectF r=toPoints(rect);
        QString text=page->text(r);
        delete page;

        QClipboard *clipboard=QGuiApplication::clipboard();
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
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
