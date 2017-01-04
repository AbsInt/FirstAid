/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
 * Copyright (C) 2016, Marc Langenbach <mlangen@absint.com>
 * Copyright (C) 2016, Christoph Cullmann <cullmann@absint.com>
 *
 * With portions of code from okular/ui/pageview.cpp by:
 *     Copyright (C) 2004-2005 by Enrico Ros <eros.kde@email.it>
 *     Copyright (C) 2004-2006 by Albert Astals Cid <aacid@kde.org>
 *
 * With portions of code from kpdf/kpdf_pagewidget.cc by:
 *     Copyright (C) 2002 by Wilco Greven <greven@kde.org>
 *     Copyright (C) 2003 by Christophe Devriese
 *                           <Christophe.Devriese@student.kuleuven.ac.be>
 *     Copyright (C) 2003 by Laurent Montel <montel@kde.org>
 *     Copyright (C) 2003 by Dirk Mueller <mueller@kde.org>
 *     Copyright (C) 2004 by James Ots <kde@jamesots.com>
 *     Copyright (C) 2011 by Jiri Baum - NICTA <jiri@baum.com.au>
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
#include "viewer.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QGestureEvent>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRubberBand>
#include <QScrollBar>

#include <QDebug>

/*
 * Helper class
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

PageView::PageView(QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_dpiX(QApplication::desktop()->physicalDpiX())
    , m_dpiY(QApplication::desktop()->physicalDpiY())
    , m_rubberBand(new QRubberBand(QRubberBand::Rectangle, this))
{
    // for now to have correct aspect ratio on windows
    m_dpiX = m_dpiY;

    /**
     * allow 64 cached pages
     */
    m_imageCache.setMaxCost(64);

    // ensure we recognize pinch and swipe guestures
    grabGesture(Qt::PinchGesture);
    grabGesture(Qt::SwipeGesture);
    grabGesture(Qt::PanGesture);

    connect(PdfViewer::searchEngine(), SIGNAL(started()), SLOT(slotFindStarted()));
    connect(PdfViewer::searchEngine(), SIGNAL(highlightMatch(int, QRectF)), SLOT(gotoPage(int, QRectF)));
    connect(PdfViewer::searchEngine(), SIGNAL(matchesFound(int, QList<QRectF>)), SLOT(slotMatchesFound(int, QList<QRectF>)));

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrolled()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrolled()));

    // we have static content that can be scrolled like an image
    setAttribute(Qt::WA_StaticContents);

    // no additional frame
    setFrameStyle(QFrame::NoFrame);

    /**
     * track the mouse + viewport should have focus
     */
    viewport()->setFocusProxy(this);
    viewport()->setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);

    /**
     * we paint everything on our own
     */
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_NoSystemBackground);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);

    // behave like QScrollArea => 20
    verticalScrollBar()->setSingleStep(20);
    horizontalScrollBar()->setSingleStep(20);

    // add some shortcuts
    new QShortcut(Qt::Key_Backspace, this, SLOT(stepBack()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::Key_Space, this, SLOT(advance()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::ControlModifier+Qt::Key_0, this, SLOT(zoomOriginal()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::ControlModifier+Qt::Key_Plus, this, SLOT(zoomIn()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::ControlModifier+Qt::Key_Minus, this, SLOT(zoomOut()), nullptr, Qt::ApplicationShortcut);
}

PageView::~PageView()
{
}

/*
 * public methods
 */

qreal PageView::resX() const
{
    return m_dpiX * m_zoom * devicePixelRatio();
}

qreal PageView::resY() const
{
    return m_dpiY * m_zoom * devicePixelRatio();
}

QRectF PageView::fromPoints(const QRectF &rect) const
{
    return QRectF(rect.left() / 72.0 * resX(), rect.top() / 72.0 * resY(), rect.width() / 72.0 * resX(), rect.height() / 72.0 * resY());
}

QRectF PageView::toPoints(const QRectF &rect) const
{
    return QRectF(rect.left() * 72.0 / resX(), rect.top() * 72.0 / resY(), rect.width() * 72.0 / resX(), rect.height() * 72.0 / resY());
}

int PageView::currentPage() const
{
    return m_currentPage;
}

/*
 * public methods
 */

void PageView::setDocument(Poppler::Document *document)
{
    m_document = document;
    m_currentPage = 0;

    // visual size of document might change now!
    updateViewSize();
}

void PageView::setZoomMode(ZoomMode mode)
{
    if (mode != m_zoomMode) {
        m_zoomMode = mode;

        // fake resize event to recompute sizes, e.g. for fit width/page
        QResizeEvent e(size(), size());
        resizeEvent(&e);

        // visual size of document might change now!
        updateViewSize();
    }
}

void PageView::setZoom(qreal zoom)
{
    setZoomMode(Absolute);

    if (zoom != m_zoom && zoom >= 0.1 && zoom <= 4.0) {
        m_zoom = zoom;

        // visual size of document might change now!
        updateViewSize();
    }
}

void PageView::wheelEvent(QWheelEvent *wheelEvent)
{
    if (wheelEvent->modifiers() & Qt::ControlModifier) {
        if (wheelEvent->delta() > 0)
            setZoom(m_zoom + 0.1);
        else
            setZoom(m_zoom - 0.1);
        emit zoomChanged(m_zoom);
        return;
    }

    QAbstractScrollArea::wheelEvent(wheelEvent);
}

void PageView::setDoubleSideMode(DoubleSideMode mode)
{
    if (mode != m_doubleSideMode) {
        m_doubleSideMode = mode;

        // fake resize event to recompute sizes, e.g. for fit width/page
        QResizeEvent e(size(), size());
        resizeEvent(&e);

        // visual size of document might change now!
        updateViewSize();
    }
}

void PageView::scrolled()
{
    int pageSize = pageHeight();

    // compute page
    int value = verticalScrollBar()->value();
    int page = value / (pageSize + 2 * PAGEFRAME);

    // set page
    value -= (page * (pageSize + 2 * PAGEFRAME));
    // scroll(page,value);
    m_offset = QPoint(horizontalScrollBar()->value(), value);

    if (m_doubleSideMode)
        page = page * 2 - 1;
    if (page < 0)
        page = 0;

    if (page < (m_document ? m_document->numPages() : 0)) {
        m_currentPage = page;
        emit currentPageChanged(m_currentPage);
    }
}

/*
 * protected methods
 */

bool PageView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        printf("Gesture detected...");
        QGestureEvent *ge = static_cast<QGestureEvent *>(event);

        if (QSwipeGesture *swipe = static_cast<QSwipeGesture *>(ge->gesture(Qt::SwipeGesture))) {
            printf("SWIPE");
            if (QSwipeGesture::Up == swipe->verticalDirection()) {
                printf("Swipe up detected");
                gotoNextPage();
                ge->accept(swipe);
                return true;
            }
            if (QSwipeGesture::Down == swipe->verticalDirection()) {
                printf("Swipe down detected");
                gotoPreviousPage();
                ge->accept(swipe);
                return true;
            }
        }

        if (QPinchGesture *pinch = static_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture))) {
            printf("PINCH");
            static qreal pinchStartZoom;

            if (Qt::GestureStarted == pinch->state())
                pinchStartZoom = m_zoom;

            setZoom(pinchStartZoom * pinch->totalScaleFactor());
            emit zoomChanged(m_zoom);

            ge->accept(pinch);
            return true;
        }

        if (QPanGesture *pan = static_cast<QPanGesture *>(ge->gesture(Qt::PanGesture))) {
            printf("PAN");
            static QPoint panStartOffset;

            if (Qt::GestureStarted == pan->state())
                panStartOffset = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

            horizontalScrollBar()->setValue(panStartOffset.x() + pan->offset().x());
            verticalScrollBar()->setValue(panStartOffset.y() + pan->offset().y());

            ge->accept(pan);
            return true;
        }
    }

    return QAbstractScrollArea::event(event);
}

void PageView::paintEvent(QPaintEvent *paintEvent)
{
    QSize vs = viewport()->size();
    QPainter p(viewport());
    p.fillRect(paintEvent->rect(), Qt::gray);

    if (!m_document)
        return;

    m_pageRects.clear();
    m_pageHeight = 0;
    int currentPage = m_currentPage;

    // show previous page in doubleside mode
    if (m_doubleSideMode && (currentPage > 1) && !(currentPage % 2))
        --currentPage;

    int matchPage;
    QRectF matchRect;
    PdfViewer::searchEngine()->currentMatch(matchPage, matchRect);

    QPoint pageStart = -m_offset + QPoint(0, PAGEFRAME);

    while (pageStart.y() < 0 || vs.height() > (pageStart.y() + 2 * PAGEFRAME)) {
        // draw another page

        FirstAidPage cachedPage = getPage(currentPage);

        if (cachedPage.m_image.isNull())
            break;

        int doubleSideOffset = 0;
        if (m_doubleSideMode) {
            // special handling for first page and documents with only two pages
            if (0!=currentPage || 2==m_document->numPages()) {
                doubleSideOffset = cachedPage.m_image.width() /  devicePixelRatio() / 2;
                if (currentPage % 2)
                    doubleSideOffset *= -1;
            }
        }

        pageStart.setX(qMax(0, vs.width() - cachedPage.m_image.width() /  devicePixelRatio()) / 2 - m_offset.x() + doubleSideOffset + ((m_zoomMode == Absolute) ? PAGEFRAME : 0));
        p.drawImage(pageStart, cachedPage.m_image);

        m_pageRects.append(qMakePair(currentPage, QRect(pageStart, cachedPage.m_image.size())));
        m_pageHeight = cachedPage.m_image.height() /  devicePixelRatio();

        // draw matches on page
        double sx = resX() / 72.0;
        double sy = resY() / 72.0;

        p.setPen(Qt::NoPen);

        foreach (QRectF rect, PdfViewer::searchEngine()->matchesFor(currentPage)) {
            QColor matchColor = QColor(255, 255, 0, 64);
            if (currentPage == matchPage && rect == matchRect)
                matchColor = QColor(255, 128, 0, 128);

            QRectF r = QRectF(rect.left() * sx, rect.top() * sy, rect.width() * sx, rect.height() * sy);
            r.adjust(-3, -5, 3, 2);
            p.fillRect(r.translated(pageStart), matchColor);
        }

        // draw border around page
        p.setPen(Qt::darkGray);
        p.drawRect(QRect(pageStart, cachedPage.m_image.size()).adjusted(-1, -1, 1, 1));

        // set next page
        ++currentPage;

        if (!m_doubleSideMode || (currentPage % 2))
            pageStart.setY(pageStart.y() + cachedPage.m_image.height() /  devicePixelRatio() + 2 * PAGEFRAME);
    }
    p.end();
}

void PageView::resizeEvent(QResizeEvent *resizeEvent)
{
    if (!m_document)
        return;

    if (FitWidth == m_zoomMode) {
        Poppler::Page *page = m_document->page(m_currentPage);
        if (!page)
            return;
        QSizeF pageSize = page->pageSize();
        pageSize.setWidth(2 * PAGEFRAME + pageSize.width());
        delete page;

        if (DoubleSided == m_doubleSideMode || (DoubleSidedNotFirst == m_doubleSideMode)) {
            if (Poppler::Page *page = m_document->page(m_currentPage + 1)) {
                pageSize.setWidth(pageSize.width() + page->pageSize().width() + PAGEFRAME);
                delete page;
            }
        }

        m_zoom = viewport()->size().width() / (m_dpiX * pageSize.width() / 72.0);
        updateViewSize();
    } else if (FitPage == m_zoomMode) {
        Poppler::Page *page = m_document->page(m_currentPage);
        if (!page)
            return;
        QSizeF pageSize = page->pageSize();
        pageSize.setWidth(2 * PAGEFRAME + pageSize.width());
        pageSize.setHeight(2 * PAGEFRAME + pageSize.height());
        delete page;

        if (DoubleSided == m_doubleSideMode || (DoubleSidedNotFirst == m_doubleSideMode)) {
            if (Poppler::Page *page = m_document->page(m_currentPage + 1)) {
                pageSize.setWidth(pageSize.width() + page->pageSize().width() + PAGEFRAME);
                delete page;
            }
        }

        qreal zx = viewport()->size().width() / (m_dpiX * pageSize.width() / 72.0);
        qreal zy = viewport()->size().height() / (m_dpiY * pageSize.height() / 72.0);

        m_zoom = qMin(zx, zy);
        updateViewSize();
    }

    QAbstractScrollArea::resizeEvent(resizeEvent);
}

void PageView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_document)
        return;

    for (int i = 0; i < m_pageRects.length(); ++i) {
        QPair<int, QRect> currentPage = m_pageRects.at(i);
        QRect displayRect = currentPage.second;

        qreal xPos = (event->x() - displayRect.x()) / (qreal)displayRect.width();
        qreal yPos = (event->y() - displayRect.y()) / (qreal)displayRect.height();
        QPointF p = QPointF(xPos, yPos);

        FirstAidPage cachedPage = getPage(currentPage.first);

        for (auto &a : *cachedPage.m_annotations) {
            if (a->boundary().contains(p)) {
                setCursor(Qt::PointingHandCursor);
                return;
            }
        }

        setCursor(Qt::ArrowCursor);

        if (m_rubberBandOrigin.first >= 0)
            m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, event->pos()).normalized());
    }
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    if (!m_document)
        return;

    for (int i = 0; i < m_pageRects.length(); ++i) {
        QPair<int, QRect> currentPage = m_pageRects.at(i);
        QRect displayRect = currentPage.second;
        FirstAidPage cachedPage = getPage(currentPage.first);

        qreal xPos = (event->x() - displayRect.x()) / (qreal)displayRect.width();
        qreal yPos = (event->y() - displayRect.y()) / (qreal)displayRect.height();
        QPointF p = QPointF(xPos, yPos);

        for (auto &a : *cachedPage.m_annotations) {
            if (a->boundary().contains(p)) {
                Poppler::Link *link = static_cast<Poppler::LinkAnnotation *>(a.get())->linkDestination();
                switch (link->linkType()) {
                    case Poppler::Link::Goto: {
                        Poppler::LinkDestination gotoLink = static_cast<Poppler::LinkGoto *>(link)->destination();
                        int offset = gotoLink.isChangeTop() ? gotoLink.top() * displayRect.height() : 0;
                        gotoPage(gotoLink.pageNumber()-1, offset);
                    }
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

        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_rubberBandOrigin = qMakePair(currentPage.first, event->pos());
            m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, QSize()));
            m_rubberBand->show();
        }
    }
}

void PageView::mouseReleaseEvent(QMouseEvent *)
{
    if (!m_document)
        return;

    if (m_rubberBandOrigin.first < 0)
        return;

    QRect displayRect;

    for (int i = 0; i < m_pageRects.length(); ++i) {
        if (m_pageRects.at(i).first == m_rubberBandOrigin.first) {
            displayRect = m_pageRects.at(i).second;
            break;
        }
    }

    if (!displayRect.isValid())
        return;

    m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
    m_rubberBand->hide();
    emit copyRequested(m_rubberBand->geometry().intersected(displayRect).translated(-displayRect.topLeft()));
}

/*
 * public slots
 */

void PageView::gotoPage(int page, int offset)
{
    if (!m_document || page < 0 || page >= m_document->numPages())
        return;

    if (page==m_currentPage && m_offset.y()==offset)
        return;

    m_offset = QPoint(m_offset.x(), offset);
    m_currentPage = page;

    // misuse to go to page
    updateViewSize(false /* goto mode, don't clear cache */);

    emit currentPageChanged(m_currentPage);
}

void PageView::gotoPage(int page, const QRectF &rect)
{
    // TODO instead of moving rect on top of the visible area we should ensure that the area is visible
    gotoPage(page, rect.top()/72.0*resX());
}

void PageView::gotoPreviousPage()
{
    int newPage = qMax(0, m_currentPage - (m_doubleSideMode ? 2 : 1));
    gotoPage(newPage, 0);
}

void PageView::gotoNextPage()
{
    int newPage = qMin(m_document->numPages() - 1, m_currentPage + (m_doubleSideMode ? 2 : 1));
    gotoPage(newPage, 0);
}

void PageView::stepBack()
{
    // TODO go to start of current page if not visible or to previous page
    gotoPreviousPage();
}

void PageView::advance()
{
    // TODO go to end of current page if not visible or to next page
    gotoNextPage();
}

void PageView::zoomIn()
{
    setZoom(m_zoom + 0.1);
    emit zoomChanged(m_zoom);
}

void PageView::zoomOut()
{
    setZoom(m_zoom - 0.1);
    emit zoomChanged(m_zoom);
}

void PageView::zoomOriginal()
{
    setZoom(1.0);
    emit zoomChanged(m_zoom);
}

/*
 * protected slots
 */

void PageView::gotoDestination(const QString &destination)
{
    bool ok = false;
    int pageNumber = destination.toInt(&ok);

    if (ok)
        gotoPage(pageNumber - 1);
    else if (m_document) {
        if (Poppler::LinkDestination *link = m_document->linkDestination(destination)) {
            gotoPage(link->pageNumber() - 1, (link->isChangeTop() ? link->top() : 0));
            delete link;
        }
    }
}

void PageView::slotCopyRequested(const QRectF &rect)
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

void PageView::slotFindStarted()
{
    viewport()->update();
}

void PageView::slotMatchesFound(int page, const QList<QRectF> &)
{
    if (page == m_currentPage)
        viewport()->update();
}

/*
 * private methods
 */

int PageView::pageHeight()
{
    if (!m_document)
        return 0;

    int pageSize = 0;
    if (Poppler::Page *popplerPage = m_document->page(currentPage())) {
        pageSize = popplerPage->pageSize().height();
        delete popplerPage;
    }
    return (pageSize * resY()) / 72.0;
}

int PageView::pageWidth()
{
    if (!m_document)
        return 0;

    int pageSize = 0;
    if (Poppler::Page *popplerPage = m_document->page(currentPage())) {
        pageSize = popplerPage->pageSize().width();
        delete popplerPage;
    }
    return (pageSize * resX()) / 72.0;
    ;
}

void PageView::updateViewSize(bool invalidateCache)
{
    // invalidate cache
    if (invalidateCache)
        m_imageCache.clear();

    QScrollBar *vbar = verticalScrollBar();
    int pageCount = m_document ? m_document->numPages() : 0;
    int current = currentPage();
    if (m_doubleSideMode) {
        if (pageCount % 2)
            pageCount = pageCount / 2 + 1;
        else
            pageCount /= 2;
        if (current % 2)
            current = current / 2 + 1;
        else
            current /= 2;
    }
    int pageSize = pageHeight();
    vbar->setRange(0, (pageCount * 2) * PAGEFRAME + qMax(0, pageCount * pageSize - viewport()->height()));
    vbar->blockSignals(true);
    vbar->setValue(current * (pageSize + 2 * PAGEFRAME) + m_offset.y());
    vbar->blockSignals(false);

    QScrollBar *hbar = horizontalScrollBar();
    pageSize = pageWidth();
    if (m_doubleSideMode) {
        pageSize *= 2;
        pageSize += PAGEFRAME;
    }
    hbar->setRange(0, qMax(0, (pageSize + 2 * PAGEFRAME) - viewport()->width()));
    hbar->blockSignals(true);
    hbar->setValue(m_offset.x());
    hbar->blockSignals(false);

    // update viewport
    viewport()->update();
}

FirstAidPage PageView::getPage(int pageNumber)
{
    FirstAidPage *cachedPage = m_imageCache.object(pageNumber);

    if (!cachedPage) {
        if (Poppler::Page *page = m_document->page(pageNumber)) {
            QList<Poppler::Annotation *> annots = page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink);

            /**
             * we render in too high resolution and then set the right ratio
             */
            cachedPage = new FirstAidPage(page->renderToImage(resX(), resY(), -1, -1, -1, -1, Poppler::Page::Rotate0), annots);
            cachedPage->m_image.setDevicePixelRatio(devicePixelRatio());

            m_imageCache.insert(pageNumber, cachedPage);
            delete page;
            return *cachedPage;
        }
    } else
        return *cachedPage;

    return FirstAidPage(QImage(), QList<Poppler::Annotation *>());
}
