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

#include "continouspageview.h"
#include "searchengine.h"

#include <poppler-qt5.h>

#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QDesktopWidget>
#include <QLabel>
#include <QRubberBand>
#include <QScrollBar>

#include <QDebug>

/*
 * constructors / destructor
 */

ContinousPageView::ContinousPageView(QWidget *parent)
    : PageView(parent)
{
    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    setMouseTracking(true);

    m_imageCache.setMaxCost(8);

    SearchEngine *se = SearchEngine::globalInstance();
    connect(se, SIGNAL(started()), SLOT(slotFindStarted()));
    connect(se, SIGNAL(highlightMatch(int, QRectF)), SLOT(slotHighlightMatch(int, QRectF)));
    connect(se, SIGNAL(matchesFound(int, QList<QRectF>)), SLOT(slotMatchesFound(int, QList<QRectF>)));

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrolled()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrolled()));

    horizontalScrollBar()->setSingleStep(10);
    verticalScrollBar()->setSingleStep(10);

    reset();
}

ContinousPageView::~ContinousPageView()
{
}

void ContinousPageView::setDocument(Poppler::Document *document)
{
    // loaded

    PageView::setDocument(document);
    resizeEvent(nullptr);
}

void ContinousPageView::setDoubleSideMode(PageView::DoubleSideMode mode)
{
    m_imageCache.clear();
    PageView::setDoubleSideMode(mode);
}

void ContinousPageView::setZoomMode(PageView::ZoomMode mode)
{
    m_imageCache.clear();
    PageView::setZoomMode(mode);
}

void ContinousPageView::setZoom(qreal zoom)
{
    m_imageCache.clear();
    PageView::setZoom(zoom);
}

void ContinousPageView::scrolled()
{
    int pageSize = pageHeight();

    // compute page
    int value = verticalScrollBar()->value();
    int page = value / (pageSize + 2*PAGEFRAME);

    // set page
    value -= (page * (pageSize + 2*PAGEFRAME));
    // scroll(page,value);
    m_offset = QPoint(horizontalScrollBar()->value(),value);

    if (m_doubleSideMode)
        page = page * 2 - 1;
    if(page<0)
        page=0;

    if (page < m_document->numPages())
    {
        m_currentPage = page;
        emit currentPageChanged(m_currentPage);
    }
    viewport()->update();
}

/*
 * public methods
 */

void ContinousPageView::gotoPage(int page,int offset)
{
    if (!m_document || page < 0 || page >= m_document->numPages())
        return;

    m_offset = QPoint(m_offset.x(),offset);
    m_currentPage = page;
    updateScrollBars();
    viewport()->update();

    emit currentPageChanged(m_currentPage);
}

/*
 * protected methods
 */

void ContinousPageView::paintEvent(QPaintEvent * /*resizeEvent*/)
{
    if (!m_document)
        return;

    m_pageRects.clear();
    m_pageHeight = 0;
    int currentPage = m_currentPage;

    //show previous page in doubleside mode
    if (m_doubleSideMode && (currentPage>1) && !(currentPage%2))
        --currentPage;

    QSize vs = viewport()->size();

    QPainter p(viewport());
    p.fillRect(0,0,vs.width(),vs.height(),Qt::gray);

    SearchEngine *se = SearchEngine::globalInstance();
    int matchPage;
    QRectF matchRect;
    se->currentMatch(matchPage, matchRect);

    QPoint pageStart = -m_offset + QPoint(0,PAGEFRAME);

    while (pageStart.y() < 0 || vs.height() > (pageStart.y() + 2*PAGEFRAME)) {
        // draw another page

        FirstAidPage* cachedPage =  getPage(currentPage);

        if (!cachedPage)
            break;

        QImage* image = cachedPage->m_image;

        if (!image || image->isNull())
            break;

        int doubleSideOffset = 0;
        if (m_doubleSideMode)
        {
            doubleSideOffset = image->width()/2;
            if (currentPage%2)
                doubleSideOffset *= -1;
        }

        pageStart.setX(qMax(0, vs.width() - image->width()) / 2 -m_offset.x() + doubleSideOffset + ((m_zoomMode==Absolute)?PAGEFRAME:0));

        // match further matches on page
        double sx = resX() / 72.0;
        double sy = resY() / 72.0;

        QPainter sp(image);
        sp.setPen(Qt::NoPen);

        foreach (QRectF rect, se->matchesFor(currentPage)) {
            QColor matchColor = QColor(255, 255, 0, 64);
            if (currentPage == matchPage && rect == matchRect)
                matchColor = QColor(255, 128, 0, 128);

            QRectF r = QRectF(rect.left() * sx, rect.top() * sy, rect.width() * sx, rect.height() * sy);
            r.adjust(-3, -5, 3, 2);
            sp.fillRect(r, matchColor);
        }
        sp.end();

        p.drawImage(pageStart, *image);
        m_pageRects.append(QPair<int,QRect>(currentPage,QRect(pageStart,image->size())));
        m_pageHeight = image->height();

        p.setPen(Qt::darkGray);
        p.drawRect(QRect(pageStart, image->size()).adjusted(-1, -1, 1, 1));

        //insert image into cache
        m_imageCache.insert(currentPage,cachedPage);

        // set next page
        ++currentPage;

        if (!m_doubleSideMode || (currentPage%2))
            pageStart.setY(pageStart.y() + image->height() + 2*PAGEFRAME);
    }
    p.end();

    updateScrollBars();

}

void ContinousPageView::resizeEvent(QResizeEvent * /*resizeEvent*/)
{
    m_imageCache.clear();
    setSize(viewport()->size() - QSize(1, 1));
    viewport()->update();
}

void ContinousPageView::keyPressEvent(QKeyEvent *event)
{
    #if 0
    QScrollBar *vsb = verticalScrollBar();

    if (m_document && event->key() == Qt::Key_PageDown && (!vsb->isVisible() || vsb->value() == vsb->maximum())) {
        if (m_currentPage < m_document->numPages() - 1) {
            gotoPage(m_currentPage + 1);
            verticalScrollBar()->setValue(0);
            return;
        }
    }

    if (m_document && event->key() == Qt::Key_PageUp && (!vsb->isVisible() || vsb->value() == 0)) {
        if (m_currentPage > 0) {
            gotoPage(currentPage() - 1);
            verticalScrollBar()->setValue(verticalScrollBar()->maximum());
            return;
        }
    }
    #endif

    if (m_document) {
        if (event->key() == Qt::Key_PageDown) {
            if (m_currentPage < m_document->numPages() - 1) {
                gotoPage(m_currentPage + 1);
                return;
            }
        }

        if (event->key() == Qt::Key_PageUp) {
            if (m_currentPage > 0) {
                gotoPage(currentPage() - 1);
                return;
            }
        }
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void ContinousPageView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_document)
        return;

    for (int i = 0; i < m_pageRects.length(); ++i)
    {
        QPair<int,QRect> currentPage = m_pageRects.at(i);
        QRect displayRect = currentPage.second;

        qreal xPos = (event->x() - displayRect.x()) / (qreal)displayRect.width();
        qreal yPos = (event->y() - displayRect.y()) / (qreal)displayRect.height();
        QPointF p = QPointF(xPos, yPos);

        FirstAidPage * cachedPage = getPage(currentPage.first);

        foreach (Poppler::Annotation *a, cachedPage->m_annotations) {
            if (a->boundary().contains(p)) {
                setCursor(Qt::PointingHandCursor);
                //insert image into cache
                m_imageCache.insert(currentPage.first,cachedPage);
                return;
            }
        }

        setCursor(Qt::ArrowCursor);

        if (m_rubberBandOrigin.first>=0)
            m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, event->pos()).normalized());

        m_imageCache.insert(currentPage.first,cachedPage);
    }
}

void ContinousPageView::mousePressEvent(QMouseEvent *event)
{
    if (!m_document)
        return;

    for (int i = 0; i < m_pageRects.length(); ++i)
    {
        QPair<int,QRect> currentPage = m_pageRects.at(i);
        QRect displayRect = currentPage.second;
        FirstAidPage * cachedPage = getPage(currentPage.first);

        qreal xPos = (event->x() - displayRect.x()) / (qreal)displayRect.width();
        qreal yPos = (event->y() - displayRect.y()) / (qreal)displayRect.height();
        QPointF p = QPointF(xPos, yPos);

        foreach (Poppler::Annotation *a, cachedPage->m_annotations) {
            if (a->boundary().contains(p)) {
                Poppler::Link *link = static_cast<Poppler::LinkAnnotation *>(a)->linkDestination();
                switch (link->linkType()) {
                    case Poppler::Link::Goto:
                    {
                        Poppler::LinkDestination gotoLink = static_cast<Poppler::LinkGoto *>(link)->destination();
                        int offset = gotoLink.isChangeTop()?gotoLink.top()* displayRect.height():0;
                        emit gotoRequested(QString::number(gotoLink.pageNumber())+"#"+QString::number(offset));
                        delete link;
                    }
                    return;

                    case Poppler::Link::Browse:
                        QDesktopServices::openUrl(QUrl(static_cast<Poppler::LinkBrowse *>(link)->url()));
                        delete link;
                        return;

                    default:
                        qDebug("Not yet handled link type %d.", link->linkType());
                        delete link;
                        return;
                }

                break;
            }
        }

        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_rubberBandOrigin = QPair<int,QPoint>(currentPage.first,event->pos());
            m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, QSize()));
            m_rubberBand->show();
        }


        m_imageCache.insert(currentPage.first,cachedPage);
    }
}

void ContinousPageView::mouseReleaseEvent(QMouseEvent *)
{
    if (!m_document)
        return;

    if (m_rubberBandOrigin.first<0)
        return;

    FirstAidPage * cachedPage = getPage(m_rubberBandOrigin.first);

    QRect displayRect;

    for (int i = 0; i < m_pageRects.length(); ++i)
    {
        if (m_pageRects.at(i).first == m_rubberBandOrigin.first)
        {
            displayRect = m_pageRects.at(i).second;
            break;
        }
    }

    if (!displayRect.isValid())
        return;

    m_rubberBandOrigin= QPair<int,QPoint>(-1,QPoint(0,0));
    m_rubberBand->hide();
    emit copyRequested(m_rubberBand->geometry().intersected(displayRect).translated(-displayRect.topLeft()));

    m_imageCache.insert(m_rubberBandOrigin.first,cachedPage);
}

/*
 * protected slots
 */

void ContinousPageView::slotGotoRequested(const QString &destination)
{
    gotoDestination(destination);
}

void ContinousPageView::slotCopyRequested(const QRectF &rect)
{
    if (rect.isNull())
        return;

    QMenu m(this);
    QAction *copyAction = m.addAction(QIcon(":/icons/edit-copy.svg"), "Copy");

    if (copyAction != m.exec(QCursor::pos()))
        return;

    if (Poppler::Page *page = m_document->page(m_currentPage)) {
        QRectF r = toPoints(rect);
        QString text = page->text(r);
        delete page;

        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
}

void ContinousPageView::slotFindStarted()
{
    viewport()->update();
}

void ContinousPageView::slotHighlightMatch(int page, const QRectF &)
{
    gotoPage(page);
}

void ContinousPageView::slotMatchesFound(int page, const QList<QRectF> &)
{
    if (page == m_currentPage)
        viewport()->update();
}


/*
 * private methods
 */

int ContinousPageView::pageHeight()
{
    if (!m_document)
        return 0;

    int pageSize = 0;
    if (Poppler::Page *popplerPage = m_document->page(currentPage()))
    {
        pageSize = popplerPage->pageSize().height();
        delete popplerPage;
    }
    return (pageSize * resY()) / 72.0;
}

int ContinousPageView::pageWidth()
{
    if (!m_document)
        return 0;

    int pageSize = 0;
    if (Poppler::Page *popplerPage = m_document->page(currentPage()))
    {
        pageSize = popplerPage->pageSize().width();
        delete popplerPage;
    }
    return (pageSize * resX()) / 72.0;;
}

void ContinousPageView::updateScrollBars()
{
    QScrollBar *vbar = verticalScrollBar();
    int pageCount = m_document->numPages();
    int current = currentPage();
    if (m_doubleSideMode)
    {
        if (pageCount%2)
            pageCount = pageCount/2+1;
        else
            pageCount /= 2;
        if(current%2)
            current = current/2 +1;
        else
            current /= 2;
    }
    int pageSize = pageHeight();
    vbar->setRange(0, (pageCount*2) * PAGEFRAME + qMax(0,pageCount * pageSize - viewport()->height()));
    vbar->blockSignals(true);
    vbar->setValue(current * (pageSize+2*PAGEFRAME) + m_offset.y());
    vbar->blockSignals(false);

    QScrollBar *hbar = horizontalScrollBar();
    pageSize = pageWidth();
    if (m_doubleSideMode)
    {
        pageSize *=2;
        pageSize += PAGEFRAME;
    }
    hbar->setRange(0, qMax(0,(pageSize+ 2*PAGEFRAME )- viewport()->width()) );
    hbar->blockSignals(true);
    hbar->setValue(m_offset.x());
    hbar->blockSignals(false);
}

FirstAidPage *ContinousPageView::getPage(int pageNumber)
{
    FirstAidPage* cachedPage =  m_imageCache.take(pageNumber);

    if (!cachedPage)
    {
        if (Poppler::Page *page = m_document->page(pageNumber)) {
            QImage* image = new QImage();

            cachedPage = new FirstAidPage(image, page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink));

            *image = page->renderToImage(resX(), resY(), -1, -1, -1, -1, Poppler::Page::Rotate0);

            delete page;
        }
    }

    return cachedPage;
}
