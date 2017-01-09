/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
 * Copyright (C) 2016, Marc Langenbach <mlangen@absint.com>
 * Copyright (C) 2016, Christoph Cullmann <cullmann@absint.com>
 * Copyright (C) 2016, Jan Pohland <pohland@absint.com>
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
#include <QScroller>
#include <QShortcut>

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

    // add scroller
    QScroller::grabGesture(viewport(), QScroller::TouchGesture);

    connect(PdfViewer::searchEngine(), SIGNAL(started()), SLOT(slotFindStarted()));
    connect(PdfViewer::searchEngine(), SIGNAL(highlightMatch(int, QRectF)), SLOT(gotoPage(int, QRectF)));
    connect(PdfViewer::searchEngine(), SIGNAL(matchesFound(int, QList<QRectF>)), SLOT(slotMatchesFound(int, QList<QRectF>)));

    connect(PdfViewer::document(), SIGNAL(documentChanged()), SLOT(slotDocumentChanged()));
    connect(PdfViewer::document(), SIGNAL(layoutChanged()), SLOT(slotLayoutChanged()));

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(updateCurrentPage()));

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

    // always on to make resizing less magic
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // behave like QScrollArea => 20
    verticalScrollBar()->setSingleStep(20);
    horizontalScrollBar()->setSingleStep(20);

    // add some shortcuts
    new QShortcut(Qt::Key_Backspace, this, SLOT(stepBack()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::Key_Space, this, SLOT(advance()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::ControlModifier + Qt::Key_0, this, SLOT(zoomOriginal()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::ZoomIn, this, SLOT(zoomIn()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(Qt::ControlModifier + Qt::Key_Equal, this, SLOT(zoomIn()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::ZoomOut, this, SLOT(zoomOut()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::Back, this, SLOT(historyPrev()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::Forward, this, SLOT(historyNext()), nullptr, Qt::ApplicationShortcut);

    /**
     * delays updateViewSize
     * delay it by 100 mseconds e.g. during resizing
     */
    m_updateViewSizeTimer.setSingleShot(true);
    m_updateViewSizeTimer.setInterval(100);
    connect(&m_updateViewSizeTimer, &QTimer::timeout, this, &PageView::updateViewSize);

    /**
     * initial inits
     */
    slotDocumentChanged();
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

/*
 * public methods
 */

void PageView::slotDocumentChanged()
{
    m_currentPage = -1;

    m_historyStack.clear();

    // visual size of document might change now!
    updateViewSize();

    // update page
    updateCurrentPage();
}

void PageView::slotLayoutChanged()
{
    // visual size of document might change now!
    updateViewSize();

    // update page
    updateCurrentPage();
}

int PageView::currentPage() const
{
    return m_currentPage;
}

void PageView::setZoomMode(ZoomMode mode)
{
    if (mode != m_zoomMode) {
        m_zoomMode = mode;

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

void PageView::contextMenuEvent(QContextMenuEvent *event)
{
    m_panStartPoint = QPoint();
    setCursor(Qt::ArrowCursor);

    QMenu m(this);
    m.addAction(QIcon(":/icons/edit-copy.svg"), "Copy");

    if (!m.exec(event->globalPos()))
        return;

    int pageNumber = PdfViewer::document()->pageForPoint(event->pos() + offset());
    if (-1 != pageNumber) {
        m_rubberBandOrigin = qMakePair(pageNumber, event->pos());
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, QSize()));

        // TODO CHECK
        QRect r = QRect(m_rubberBandOrigin.second, offset() + viewport()->mapFromGlobal(QCursor::pos())).intersected(fromPoints(PdfViewer::document()->pageRect(m_rubberBandOrigin.first)).toRect());
        m_rubberBand->setGeometry(r.normalized());

        m_rubberBand->show();
    }
}

void PageView::updateCurrentPage()
{
    const int page = qMax(0, PdfViewer::document()->pageForPoint(toPoints(QPointF(0, offset().y()))));
    if (page != m_currentPage) {
        m_currentPage = page;
        emit pageChanged(m_currentPage);
    }
}

QPoint PageView::offset() const
{
    /**
     * we need to adjust for:
     *   - scrollbar position
     *   - centered position of pages if smaller than viewport
     */
    return QPoint(horizontalScrollBar()->value() + qMin(0, (int(fromPoints(PdfViewer::document()->layoutSize()).width()) - viewport()->width()) / 2), verticalScrollBar()->value());
}

void PageView::setOffset(const QPoint &offset)
{
    horizontalScrollBar()->setValue(offset.x());
    verticalScrollBar()->setValue(offset.y());
}

/*
 * protected methods
 */

bool PageView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        QGestureEvent *ge = static_cast<QGestureEvent *>(event);

        if (QSwipeGesture *swipe = static_cast<QSwipeGesture *>(ge->gesture(Qt::SwipeGesture))) {
            printf("SWIPE\n");
            if (QSwipeGesture::Up == swipe->verticalDirection()) {
                printf("Swipe up detected\n");
                gotoNextPage();
                ge->accept(swipe);
                return true;
            }
            if (QSwipeGesture::Down == swipe->verticalDirection()) {
                printf("Swipe down detected\n");
                gotoPreviousPage();
                ge->accept(swipe);
                return true;
            }
        }

        if (QPinchGesture *pinch = static_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture))) {
            static qreal pinchStartZoom;

            if (Qt::GestureStarted == pinch->state())
                pinchStartZoom = m_zoom;

            setZoom(pinchStartZoom * pinch->totalScaleFactor());
            emit zoomChanged(m_zoom);

            ge->accept(pinch);
            return true;
        }
    }

    if (event->type() == QEvent::ScrollPrepare) {
        QScrollPrepareEvent *spe = static_cast<QScrollPrepareEvent *>(event);
        spe->setViewportSize(viewport()->size());
        spe->setContentPosRange(QRect(0, 0, horizontalScrollBar()->maximum(), verticalScrollBar()->maximum()));
        spe->setContentPos(offset());
        spe->accept();
        return true;
    }

    if (event->type() == QEvent::Scroll) {
        QScrollEvent *se = static_cast<QScrollEvent *>(event);
        setOffset(se->contentPos().toPoint());
        se->accept();
        return true;
    }

    return QAbstractScrollArea::event(event);
}

void PageView::paintEvent(QPaintEvent *paintEvent)
{
    QPainter p(viewport());
    p.fillRect(paintEvent->rect(), Qt::gray);
    p.translate(-offset());

    int matchPage;
    QRectF matchRect;
    PdfViewer::searchEngine()->currentMatch(matchPage, matchRect);

    foreach (int page, PdfViewer::document()->visiblePages(toPoints(paintEvent->rect().translated(offset())))) {
        QRectF pageRect = PdfViewer::document()->pageRect(page);
        QRectF displayRect = fromPoints(pageRect);

        QImage cachedPage = getPage(page);
        p.drawImage(displayRect, cachedPage);

        p.setPen(Qt::NoPen);

        foreach (QRectF rect, PdfViewer::searchEngine()->matchesFor(page)) {
            QColor matchColor = QColor(255, 255, 0, 64);
            if (page == matchPage && rect == matchRect)
                matchColor = QColor(255, 128, 0, 128);

            QRectF r = fromPoints(rect);
            r.adjust(-3, -5, 3, 2);
            p.fillRect(r.translated(displayRect.topLeft()), matchColor);
        }

        // draw border around page
        p.setPen(Qt::darkGray);
        p.drawRect(displayRect.adjusted(-1, -1, 1, 1));
    }
}

void PageView::resizeEvent(QResizeEvent *resizeEvent)
{
    QAbstractScrollArea::resizeEvent(resizeEvent);

    // trigger delayed update, will restart if already running to collapse events
    m_updateViewSizeTimer.start();
}

void PageView::mouseMoveEvent(QMouseEvent *event)
{
    // handle panning first
    if (!m_panStartPoint.isNull()) {
        setCursor(Qt::ClosedHandCursor);
        setOffset(m_panOldOffset + m_panStartPoint - event->globalPos());
        return;
    }

    // update rubber band?
    if (m_rubberBandOrigin.first >= 0) {
        QRect r = QRect(m_rubberBandOrigin.second, event->pos()).intersected(fromPoints(PdfViewer::document()->pageRect(m_rubberBandOrigin.first)).toRect());
        m_rubberBand->setGeometry(r.normalized());
    }

    // now check if we want to highlight a link location
    int page = PdfViewer::document()->pageForPoint(toPoints(offset() + event->pos()));
    if (-1 != page) {
        QRectF pageRect = fromPoints(PdfViewer::document()->pageRect(page));
        qreal xPos = (offset().x() + event->x() - pageRect.x()) / (qreal)pageRect.width();
        qreal yPos = (offset().y() + event->y() - pageRect.y()) / (qreal)pageRect.height();
        QPointF p = QPointF(xPos, yPos);

        for (auto &l : PdfViewer::document()->links(page)) {
            if (l->boundary().contains(p)) {
                setCursor(Qt::PointingHandCursor);
                return;
            }
        }
    }

    setCursor(Qt::ArrowCursor);
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    // special case: end rubber band that has been created by context menu copy
    if (m_rubberBandOrigin.first >= 0) {
        QRect displayRect = fromPoints(PdfViewer::document()->pageRect(m_rubberBandOrigin.first)).toRect();
        slotCopyRequested(m_rubberBandOrigin.first, m_rubberBand->geometry().translated(-displayRect.topLeft()));
        m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
        m_rubberBand->hide();
        return;
    }

    // now check if we want to highlight a link location
    int page = PdfViewer::document()->pageForPoint(toPoints(offset() + event->pos()));
    if (-1 != page) {
        QRectF pageRect = fromPoints(PdfViewer::document()->pageRect(page));
        qreal xPos = (offset().x() + event->x() - pageRect.x()) / (qreal)pageRect.width();
        qreal yPos = (offset().y() + event->y() - pageRect.y()) / (qreal)pageRect.height();
        QPointF p = QPointF(xPos, yPos);

        for (auto &l : PdfViewer::document()->links(page)) {
            if (l->boundary().contains(p)) {
                Poppler::Link *link = static_cast<Poppler::LinkAnnotation *>(l)->linkDestination();
                switch (link->linkType()) {
                    case Poppler::Link::Goto: {
                        Poppler::LinkDestination gotoLink = static_cast<Poppler::LinkGoto *>(link)->destination();
                        m_mousePressPage = gotoLink.pageNumber() - 1;
                        QRect displayRect = fromPoints(PdfViewer::document()->pageRect(m_mousePressPage)).toRect();
                        m_mousePressPageOffset = gotoLink.isChangeTop() ? gotoLink.top() * displayRect.height() : 0;
                    } break;

                    case Poppler::Link::Browse:
                        m_mousePressUrl = static_cast<Poppler::LinkBrowse *>(link)->url();
                        break;

                    default:
                        qDebug("Not yet handled link type %d.", link->linkType());
                }

                break;
            }
        }
    }

    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        int pageNumber = PdfViewer::document()->pageForPoint(event->pos() + offset());
        if (-1 != pageNumber) {
            m_rubberBandOrigin = qMakePair(pageNumber, event->pos());
            m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, QSize()));
            m_rubberBand->show();
        }
    }

    // if there is no Shift pressed we start panning
    if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
        m_panOldOffset = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
        m_panStartPoint = event->globalPos();

        if (-1 == m_mousePressPage && m_mousePressUrl.isEmpty())
            setCursor(Qt::ClosedHandCursor);
    }
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    // restore mouse cursor
    setCursor(Qt::ArrowCursor);

    // reset pan information
    if (!m_panStartPoint.isNull() && m_panStartPoint != event->globalPos()) {
        m_panStartPoint = QPoint(0, 0);
        return;
    }

    m_panStartPoint = QPoint(0, 0);

    // was there a page to goto?
    if (m_mousePressPage != -1) {
        m_historyStack.add(m_mousePressPage, m_mousePressPageOffset);
        gotoPage(m_mousePressPage, toPoints(QRectF(0, m_mousePressPageOffset, 0, 0)));
        m_mousePressPage = -1;
        return;
    }

    // was there an url to open?
    if (!m_mousePressUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_mousePressUrl));
        m_mousePressUrl.clear();
        return;
    }

    // reset rubber band if needed
    if (m_rubberBandOrigin.first >= 0) {
        QRect displayRect = fromPoints(PdfViewer::document()->pageRect(m_rubberBandOrigin.first)).toRect();
        if (!displayRect.isValid())
            return;

        slotCopyRequested(m_rubberBandOrigin.first, m_rubberBand->geometry().intersected(displayRect).translated(-displayRect.topLeft()));

        m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
        m_rubberBand->hide();
    }
}

/*
 * public slots
 */

void PageView::gotoPage(int page, const QRectF &rectToBeVisibleInPoints)
{
    /**
     * filter out invalid pages
     */
    if (page < 0 || page >= PdfViewer::document()->numPages())
        return;

    /**
     * get page rect for the requested page, converted to pixels
     */
    const QRectF pageRectInPixel = fromPoints(PdfViewer::document()->pageRect(page));

    /**
     * translate the rectange we want to show from points and move it to the page
     * we transform the rectToBeVisibleInPoints first to pixel, move it to the page and then add some margin
     * finally it is clipped with page rectangle to not end in the void
     */
    const int marginToBeSeen = 100;
    const QRectF toBeVisibleInPixel = fromPoints(rectToBeVisibleInPoints).translated(pageRectInPixel.topLeft()).marginsAdded(QMarginsF(marginToBeSeen, marginToBeSeen, marginToBeSeen, marginToBeSeen)).intersected(pageRectInPixel);

    /**
     * if the page difference is large, just jump there, else smooth scroll
     */
    if (qAbs(m_currentPage - page) > 4) {
        setOffset(toBeVisibleInPixel.topLeft().toPoint());
    } else {
        QScroller::scroller(viewport())->ensureVisible(toBeVisibleInPixel, 0, 0);
    }

    /**
     * trigger repaint in any case
     */
    viewport()->update();
}

void PageView::gotoPreviousPage()
{
    int newPage = qMax(0, m_currentPage - (PdfViewer::document()->doubleSided() ? 2 : 1));
    gotoPage(newPage);
}

void PageView::gotoNextPage()
{
    int newPage = qMin(PdfViewer::document()->numPages() - 1, m_currentPage + (PdfViewer::document()->doubleSided() ? 2 : 1));
    gotoPage(newPage);
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

void PageView::historyPrev()
{
    HistoryEntry entry;
    if (m_historyStack.previous(entry))
        gotoHistoryEntry(entry);
}

void PageView::historyNext()
{
    HistoryEntry entry;
    if (m_historyStack.next(entry))
        gotoHistoryEntry(entry);
}

/*
 * protected slots
 */

void PageView::gotoDestination(const QString &destination, bool updateHistory)
{
    bool ok = false;
    int pageNumber = destination.toInt(&ok);

    if (ok)
        gotoPage(pageNumber - 1);
    else {
        if (Poppler::LinkDestination *link = PdfViewer::document()->linkDestination(destination)) {
            int pageNumber = link->pageNumber() - 1;
            gotoPage(pageNumber, QRectF(0, (link->isChangeTop() ? link->top() * PdfViewer::document()->pageRect(pageNumber).height() : 0), 0, 0));
            delete link;

            if (updateHistory)
                m_historyStack.add(destination);
        }
    }
}

void PageView::gotoHistoryEntry(const HistoryEntry &entry)
{
    if (HistoryEntry::PageWithOffset == entry.m_type)
        gotoPage(entry.m_page, QRectF(0, entry.m_offset, 0, 0));

    else if (HistoryEntry::Destination == entry.m_type)
        gotoDestination(entry.m_destination, false);
}

void PageView::slotCopyRequested(int page, const QRectF &rect)
{
    if (rect.isNull())
        return;

    if (Poppler::Page *p = PdfViewer::document()->page(page)) {
        QRectF r = toPoints(rect);
        QString text = p->text(r);

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

void PageView::updateViewSize()
{
    /**
     * invalidate cache
     * we might alter sizes
     */
    m_imageCache.clear();

    /**
     * adjust zoom level
     * HACK: we just use first page for some things
     */
    if (PdfViewer::document()->numPages() > 0) {
        if (FitWidth == m_zoomMode) {
            m_zoom = qreal(viewport()->width()) / (PdfViewer::document()->layoutSize().width() / 72.0 * m_dpiX);
        } else if (FitPage == m_zoomMode) {
            const qreal zx = qreal(viewport()->width()) / (PdfViewer::document()->layoutSize().width() / 72.0 * m_dpiX);
            const qreal zy = qreal(viewport()->height()) / (PdfViewer::document()->pageRect(0).height() / 72.0 * m_dpiY);
            m_zoom = qMin(zx, zy);
        }
    }

    /**
     * update ranges of scrollbars, after zoom is adjusted if needed
     */
    const QSizeF size = fromPoints(PdfViewer::document()->layoutSize());
    horizontalScrollBar()->setRange(0, qMax(0, int(size.width() - viewport()->width())));
    verticalScrollBar()->setRange(0, qMax(0, int(size.height() - viewport()->height())));

    /**
     * update page, perhaps
     */
    updateCurrentPage();

    /**
     * repaint
     */
    viewport()->update();
}

QImage PageView::getPage(int pageNumber)
{
    QImage *cachedPage = m_imageCache.object(pageNumber);

    if (!cachedPage) {
        if (Poppler::Page *page = PdfViewer::document()->page(pageNumber)) {
            /**
             * we render in too high resolution and then set the right ratio
             */
            cachedPage = new QImage(page->renderToImage(resX() * devicePixelRatio(), resY() * devicePixelRatio(), -1, -1, -1, -1, Poppler::Page::Rotate0));
            cachedPage->setDevicePixelRatio(devicePixelRatio());

            m_imageCache.insert(pageNumber, cachedPage);
            return *cachedPage;
        }
    } else
        return *cachedPage;

    return QImage();
}
