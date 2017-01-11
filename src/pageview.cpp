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
#include <QDebug>
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
#include <QVariantAnimation>

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

    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(updateCurrentPage()));
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
    connect(&m_updateViewSizeTimer, &QTimer::timeout, this, &PageView::slotUpdateViewSize);

    /**
     * initial inits
     */
    slotDocumentChanged();
}

PageView::~PageView()
{
}

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
        // visual size of document might change now!
        updateViewSize(zoom);
    }
}

void PageView::wheelEvent(QWheelEvent *wheelEvent)
{
    if (wheelEvent->modifiers() & Qt::ControlModifier) {
        if (wheelEvent->delta() > 0)
            setZoom(m_zoom + 0.1);
        else
            setZoom(m_zoom - 0.1);
        return;
    }

    QAbstractScrollArea::wheelEvent(wheelEvent);
}

void PageView::contextMenuEvent(QContextMenuEvent *)
{
}

void PageView::slotUpdateViewSize()
{
    updateViewSize();
}

void PageView::updateCurrentPage()
{
    const int page = qMax(0, PdfViewer::document()->pageForRect(toPoints(QRectF(offset(), viewport()->size()))));
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
            if (QSwipeGesture::Up == swipe->verticalDirection()) {
                gotoNextPage();
                ge->accept(swipe);
                return true;
            }
            if (QSwipeGesture::Down == swipe->verticalDirection()) {
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

    // draw the highlight rect
    if (m_highlightValue > 0) {
        p.setPen(Qt::NoPen);
        p.fillRect(m_highlightRect, QColor(0xa5, 0x1e, 0x22, m_highlightValue));
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

        //keep mouse in viewport while panning, wrap at the borders
        if (!viewport()->rect().contains(event->pos()))
        {
            QPoint newStart = event->globalPos();
            if (event->x() < 0)
            {
                newStart += QPoint(viewport()->width(),0);
            } else if (event->x() > viewport()->width()) {
                newStart -= QPoint(viewport()->width(),0);
            }
            if (event->y() < 0)
            {
                newStart += QPoint(0,viewport()->height());
            } else if (event->y() > viewport()->height()){
                newStart -= QPoint(0,viewport()->height());
            }
            m_panStartPoint = newStart;
            m_panOldOffset= QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
            QApplication::desktop()->cursor().setPos(newStart);
        }

        setCursor(Qt::ClosedHandCursor);
        setOffset(m_panOldOffset + m_panStartPoint - event->globalPos());
        return;
    }

    // update rubber band?
    if (m_rubberBandOrigin.first >= 0) {
        QRect r = QRect(m_rubberBandOrigin.second, event->pos()).intersected(fromPoints(PdfViewer::document()->pageRect(m_rubberBandOrigin.first)).toRect().translated(-offset()));
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
        slotCopyRequested(m_rubberBandOrigin.first, m_rubberBand->geometry());
        m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
        m_rubberBand->hide();
        return;
    }

    int page = PdfViewer::document()->pageForPoint(toPoints(offset() + event->pos()));

    // start rubber band with right click or shift+left click
    if ((event->button() == Qt::RightButton || event->modifiers().testFlag(Qt::ShiftModifier)) && -1 != page) {
        m_rubberBandOrigin = qMakePair(page, event->pos());
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, QSize()));
        m_rubberBand->show();
        return;
    }

    // now check if we want to highlight a link location
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
                        QRectF pageRect = PdfViewer::document()->pageRect(m_mousePressPage);

                        m_mousePressPageRect = QRectF();
                        if (gotoLink.left() > 0) {
                            m_mousePressPageRect.setLeft(gotoLink.left() * pageRect.width());
                            m_mousePressPageRect.setRight(1+ gotoLink.left() * pageRect.width());
                        }
                        if (gotoLink.top() > 0) {
                            m_mousePressPageRect.setTop(gotoLink.top() * pageRect.height());
                            m_mousePressPageRect.setBottom(1 + gotoLink.top() * pageRect.height());
                        }
                        if (gotoLink.right() > 0)
                            m_mousePressPageRect.setRight(gotoLink.right() * pageRect.width());
                        if (gotoLink.bottom() > 0 && gotoLink.bottom() < 1.0)
                            m_mousePressPageRect.setBottom(gotoLink.bottom() * pageRect.height());

                        m_mousePressPageRect = m_mousePressPageRect.intersected(pageRect.translated(-pageRect.topLeft()));
                    } break;

                    case Poppler::Link::Browse:
                        m_mousePressUrl = static_cast<Poppler::LinkBrowse *>(link)->url();
                        break;

                    default:
                        break;
                }
                break;
            }
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
        m_historyStack.add(m_mousePressPage, m_mousePressPageRect);
        gotoPage(m_mousePressPage, m_mousePressPageRect);
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
        slotCopyRequested(m_rubberBandOrigin.first, m_rubberBand->geometry());

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
     * transform the rectToBeVisibleInPoints first to pixel
     */
    QRectF toBeVisibleInPixel = fromPoints(rectToBeVisibleInPoints);

    /**
     * do not change x value (scroll to 0) if not given (use old value)
     */
    bool scrollXValue = PdfViewer::document()->doubleSided() || (toBeVisibleInPixel.x() != 0);

    /**
     * move it to the page
     */
    toBeVisibleInPixel.translate(pageRectInPixel.topLeft());

    /**
     * if needed visualize link destination
     */
    if (!rectToBeVisibleInPoints.isNull()) {
        m_highlightRect = toBeVisibleInPixel.toRect();
        m_highlightRect.setLeft(pageRectInPixel.left());
        m_highlightRect.setRight(pageRectInPixel.right());
        if (m_highlightRect.height() < 30)
            m_highlightRect.setHeight(30);
        QVariantAnimation *va = new QVariantAnimation(this);
        connect(va, SIGNAL(valueChanged(QVariant)), SLOT(slotAnimationValueChanged(QVariant)));
        va->setDuration(1000);
        va->setStartValue(255);
        va->setEndValue(0);
        va->start(QAbstractAnimation::DeleteWhenStopped);
    }

    /**
     * add some margin (don't do that if x value should not be changed)
     */
    const int marginToBeSeen = 100;
    QMarginsF m = QMarginsF(scrollXValue ? marginToBeSeen : 0, marginToBeSeen, scrollXValue ? marginToBeSeen : 1, marginToBeSeen);
    toBeVisibleInPixel = toBeVisibleInPixel.marginsAdded(m);

    if (!scrollXValue && offset().x() > toBeVisibleInPixel.x())
        toBeVisibleInPixel.moveLeft(offset().x());

    /**
     * finally it is clipped with page rectangle to not end in the void
     */
    toBeVisibleInPixel = toBeVisibleInPixel.intersected(pageRectInPixel);

    if(toBeVisibleInPixel.isEmpty())
        toBeVisibleInPixel = pageRectInPixel;

    /**
     * if the page difference is large, just jump there, else smooth scroll
     */
    if (qAbs(m_currentPage - page) > 4) {
        setOffset(toBeVisibleInPixel.topLeft().toPoint());
    } else {
        setOffset(toBeVisibleInPixel.topLeft().toPoint());
        // QScroller::scroller(viewport())->ensureVisible(toBeVisibleInPixel, 0, 0);
    }

    /**
     * trigger repaint in any case
     */
    viewport()->update();

    /**
     * inform objects about the actual requested page
     */

    emit pageRequested(page);
}

void PageView::gotoPreviousPage()
{
    if (!PdfViewer::document()->doubleSided())
        gotoPage(qMax(0, m_currentPage - 1));
    else
        gotoPage(qMax(0, m_currentPage - (0 == horizontalScrollBar()->maximum() ? 2 : 1)));
}

void PageView::gotoNextPage()
{
    if (!PdfViewer::document()->doubleSided())
        gotoPage(qMin(PdfViewer::document()->numPages() - 1, m_currentPage + 1));
    else {
        if (0 == m_currentPage)
            gotoPage(1);
        else
            gotoPage(qMin(PdfViewer::document()->numPages() - 1, m_currentPage + (0 == horizontalScrollBar()->maximum() ? 2 : 1)));
    }
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
}

void PageView::zoomOut()
{
    setZoom(m_zoom - 0.1);
}

void PageView::zoomOriginal()
{
    setZoom(1.0);
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

void PageView::gotoDestinationName(const QString &destination, bool updateHistory)
{
    // try to lookup
    if (Poppler::LinkDestination *link = PdfViewer::document()->linkDestination(destination)) {
        // call function that takes description, skip that if page is already bogus
        if (link->pageNumber() > 0)
            gotoDestination(link->toString(), updateHistory);
        delete link;
    }
}
void PageView::gotoDestination(const QString &destination, bool updateHistory)
{
    if (destination.isEmpty())
        return;

    // directly construct from description
    Poppler::LinkDestination link(destination);
    if (link.pageNumber() > 0) {
        const int pageNumber = link.pageNumber() - 1;
        gotoPage(pageNumber, QRectF(0, (link.isChangeTop() ? link.top() * PdfViewer::document()->pageRect(pageNumber).height() : 0), 0, 0));

        if (updateHistory)
            m_historyStack.add(destination);
    }
}

void PageView::gotoHistoryEntry(const HistoryEntry &entry)
{
    if (HistoryEntry::PageWithRect == entry.m_type)
        gotoPage(entry.m_page, entry.m_rect);

    else if (HistoryEntry::Destination == entry.m_type)
        gotoDestination(entry.m_destination, false);
}

void PageView::slotCopyRequested(int page, const QRect &viewportRect)
{
    if (-1 == page || viewportRect.isNull())
        return;

    if (Poppler::Page *p = PdfViewer::document()->page(page)) {
        QRectF pageRect = PdfViewer::document()->pageRect(page);
        QRectF r = toPoints(viewportRect.translated(offset())).translated(-pageRect.topLeft());
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

void PageView::slotAnimationValueChanged(const QVariant &value)
{
    m_highlightValue = value.toInt();
    viewport()->update();
}

/*
 * private methods
 */

void PageView::updateViewSize(qreal zoom)
{
    /**
     * invalidate cache
     * we might alter sizes
     */
    m_imageCache.clear();

    QPointF currentScrollPosition = toPoints(offset());

    /**
     * adjust zoom level
     * HACK: we just use first page for some things
     */
    if (zoom >= 0.0) {
        m_zoom = zoom;
        emit zoomChanged(m_zoom);
    } else if (PdfViewer::document()->numPages() > 0) {
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
     * set single & page step depending on page size (with margin)
     * fallback to dummy values if no pages
     */
    const QSize pageSizeInPixel = (PdfViewer::document()->numPages() > 0) ? fromPoints(PdfViewer::document()->pageRect(0, true)).size().toSize() : QSize(100, 100);
    verticalScrollBar()->setSingleStep(pageSizeInPixel.height() / 30);
    horizontalScrollBar()->setSingleStep(pageSizeInPixel.width() / 30);
    verticalScrollBar()->setPageStep(pageSizeInPixel.height());
    horizontalScrollBar()->setPageStep(pageSizeInPixel.width());

    verticalScrollBar()->setValue(fromPoints(currentScrollPosition).y());

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
    /**
     * prefer cached image
     */
    if (QImage *cachedPage = m_imageCache.object(pageNumber))
        return *cachedPage;

    /**
     * try to get poppler page => render
     */
    if (Poppler::Page *page = PdfViewer::document()->page(pageNumber)) {
        /**
         * we render in too high resolution and then set the right ratio
         */
        QImage *cachedPage = new QImage(page->renderToImage(resX() * devicePixelRatio(), resY() * devicePixelRatio(), -1, -1, -1, -1, Poppler::Page::Rotate0));
        cachedPage->setDevicePixelRatio(devicePixelRatio());
        m_imageCache.insert(pageNumber, cachedPage);
        return *cachedPage;
    }

    /**
     * else: empty image, if no page there
     */
    return QImage();
}

QSize PageView::sizeHint() const
{
    /**
     * adjust size hint to document
     * HACK: we just use first page for some things
     */
    if (PdfViewer::document()->numPages() > 0)
        return (1.1 * fromPoints(PdfViewer::document()->pageRect(0, true)).size()).toSize();

    /**
     * else: normal hint
     */
    return QWidget::sizeHint();
}
