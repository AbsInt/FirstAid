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
#include "main.h"
#include "viewer.h"

#include <QApplication>
#include <QClipboard>
#include <QtConcurrent>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QGestureEvent>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QScrollBar>
#include <QScroller>
#include <QShortcut>
#include <QVariantAnimation>
#include <QWhatsThis>

/*
 * constructors / destructor
 */

PageView::PageView(QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_rubberBand(new QRubberBand(QRubberBand::Rectangle, this))
    , m_mutex(new QRecursiveMutex())
{
    /**
     * allow 64 cached pages
     */
    m_imageCache.setMaxCost(64);

    // ensure we recognize pinch and swipe guestures
    grabGesture(Qt::PinchGesture);
    grabGesture(Qt::SwipeGesture);

    // add scroller
    QScroller::grabGesture(viewport(), QScroller::TouchGesture);

    connect(PdfViewer::searchEngine(), &SearchEngine::started, this, &PageView::slotFindStarted);
    connect(PdfViewer::searchEngine(), &SearchEngine::highlightMatch, this, &PageView::slotHighlightMatch);
    connect(PdfViewer::searchEngine(), &SearchEngine::matchesFound, this, &PageView::slotMatchesFound);

    connect(PdfViewer::document(), &Document::documentChanged, this, &PageView::slotDocumentChanged);
    connect(PdfViewer::document(), &Document::layoutChanged, this, &PageView::slotLayoutChanged);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &PageView::updateCurrentPage);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &PageView::updateCurrentPage);

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
    new QShortcut(Qt::Key_Backspace, this, this, &PageView::stepBack, Qt::ApplicationShortcut);
    new QShortcut(Qt::Key_Space, this, this, &PageView::advance, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::ZoomIn, this, this, &PageView::zoomIn, Qt::ApplicationShortcut);
    new QShortcut(Qt::ControlModifier | Qt::Key_Equal, this, this, &PageView::zoomIn, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::ZoomOut, this, this, &PageView::zoomOut, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::Back, this, this, &PageView::historyPrev, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence::Forward, this, this, &PageView::historyNext, Qt::ApplicationShortcut);

    // prepare hint label
    m_hintLabel = new QLabel(viewport());
    m_hintLabel->move(5, 5);
    m_hintLabel->hide();

    m_hintLabel->setStyleSheet(
        QStringLiteral("color: white;"
                       "border: 2px solid white;"
                       "border-radius: 4px;"
                       "padding: 6px;"
                       "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #000000, stop: 0.3 #505050, stop: 1 #000000);"));

    m_hintLabelTimer = new QTimer(this);
    m_hintLabelTimer->setSingleShot(true);
    connect(m_hintLabelTimer, &QTimer::timeout, m_hintLabel, &QLabel::hide);

    /**
     * delays updateViewSize
     * delay it by 100 mseconds e.g. during resizing
     */
    m_updateViewSizeTimer.setSingleShot(true);
    m_updateViewSizeTimer.setInterval(100);
    connect(&m_updateViewSizeTimer, &QTimer::timeout, this, &PageView::slotUpdateViewSize);

    /**
     * delays clearing of the image cache when zooming
     */
    m_clearImageCacheTimer.setSingleShot(true);
    m_clearImageCacheTimer.setInterval(100);
    connect(&m_clearImageCacheTimer, &QTimer::timeout, this, &PageView::slotClearImageCache);

    /**
     * initial inits
     */
    slotDocumentChanged();
}

PageView::~PageView()
{
    delete m_mutex;
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
    // remember current page
    int currentPage = m_currentPage;

    // visual size of document might change now!
    updateViewSize();

    // goto remembered page
    gotoPage(currentPage);
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
        if (wheelEvent->angleDelta().y() > 0)
            setZoom(m_zoom + 0.1);
        else if (wheelEvent->angleDelta().y() < 0)
            setZoom(m_zoom - 0.1);
        return;
    }

    QAbstractScrollArea::wheelEvent(wheelEvent);
}

void PageView::slotUpdateViewSize()
{
    updateViewSize();
}

void PageView::slotClearImageCache()
{
    QMutexLocker locker(m_mutex);
    m_imageCache.clear();
    m_usePageRect = false;
    viewport()->update();
}

void PageView::prerender(int firstPage, int lastPage, int numberOfPages)
{
    for (int i = 1; i <= 3; ++i)
        if (numberOfPages > lastPage + i)
            getPage(lastPage + i);

    for (int i = 1; i <= 3; ++i)
        if (firstPage - i >= 0)
            getPage(firstPage - i);
}

void PageView::updateCurrentPage()

{
    const int page = qMax(0, PdfViewer::document()->pageForRect(toPoints(QRect(offset(), viewport()->size()))));
    if (page != m_currentPage) {
        m_currentPage = page;
        QList<int> pages = PdfViewer::document()->visiblePages(toPoints(QRect(offset(), viewport()->size())));
        if (!pages.isEmpty()) {
            const auto future = QtConcurrent::run(&PageView::prerender, this, pages.first(), pages.last(), PdfViewer::document()->numPages());
        }
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
    const int xOffset = horizontalScrollBar()->value() + qMin(0, (int(fromPoints(PdfViewer::document()->layoutSize()).width()) - viewport()->width()) / 2);
    const int yOffset = verticalScrollBar()->value() + qMin(0, (int(fromPoints(PdfViewer::document()->layoutSize()).height()) - viewport()->height()) / 2);

    return QPoint(xOffset, yOffset);
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
    // create painter on the viewport
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // fill background
    p.fillRect(paintEvent->rect(), appIsDarkThemed ? palette().color(QPalette::Window) : Qt::gray);

    // move the desired offset
    p.translate(-offset());

    // get the current match that should be highlighted differently
    int matchPage;
    QRectF matchRect;
    PdfViewer::searchEngine()->currentMatch(matchPage, matchRect);

    // paint all visible pages
    for (int page : PdfViewer::document()->visiblePages(toPoints(paintEvent->rect().translated(offset())))) {
        QRectF pageRect = PdfViewer::document()->pageRect(page);
        QRect displayRect = fromPoints(pageRect);

        QImage cachedPage = getPage(page);
        if (m_usePageRect)
            p.drawImage(displayRect, cachedPage);
        else
            p.drawImage(displayRect.topLeft(), cachedPage);

        p.setPen(Qt::NoPen);

        // paint any matches on the current page
        for (const QRectF &rect : PdfViewer::searchEngine()->matchesFor(page)) {
            QColor matchColor = QColor(255, 255, 0, 64);
            if (page == matchPage && rect == matchRect)
                matchColor = QColor(255, 128, 0, 128);

            QRect r = fromPoints(rect);
            r.adjust(-3, -5, 3, 2);
            p.fillRect(r.translated(displayRect.topLeft()), matchColor);
        }

        // draw border around page
        p.setPen(Qt::darkGray);
        p.drawRect(displayRect);
    }

    // draw the highlight rect
    if (m_highlightValue < 100) {
        p.setPen(Qt::NoPen);

        QColor c = QColor(255, 255, 0, m_highlightValue > 50 ? 128 * (100 - m_highlightValue) / 50 : 128);

        QLinearGradient lg(QPointF(0.0, 0.5), QPointF(0.0, 1.0));
        lg.setSpread(QGradient::ReflectSpread);
        lg.setCoordinateMode(QGradient::ObjectBoundingMode);
        lg.setColorAt(0.5, c);
        lg.setColorAt(1.0, c.lighter());

        QRect r = m_highlightRect;
        if (m_highlightValue < 10)
            r.setWidth(m_highlightValue * r.width() / 10);

        p.fillRect(r, lg);
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
        // keep mouse in viewport while panning, wrap at the borders
        if (!viewport()->rect().contains(event->pos())) {
            QPoint newStart = event->globalPosition().toPoint();
            if (event->position().x() < 0) {
                newStart += QPoint(viewport()->width(), 0);
            } else if (event->position().x() > viewport()->width()) {
                newStart -= QPoint(viewport()->width(), 0);
            }
            if (event->position().y() < 0) {
                newStart += QPoint(0, viewport()->height());
            } else if (event->position().y() > viewport()->height()) {
                newStart -= QPoint(0, viewport()->height());
            }
            m_panStartPoint = newStart;
            m_panOldOffset = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
            QCursor::setPos(newStart);
        }

        setCursor(Qt::ClosedHandCursor);
        setOffset(m_panOldOffset + m_panStartPoint - event->globalPosition().toPoint());
        return;
    }

    // update rubber band?
    if (m_rubberBandOrigin.first >= 0) {
        QRect r = QRect(m_rubberBandOrigin.second, event->pos()).intersected(fromPoints(PdfViewer::document()->pageRect(m_rubberBandOrigin.first)).translated(-offset()));
        m_rubberBand->setGeometry(r.normalized());
    }

    // now check if we want to highlight a link location
    int page = PdfViewer::document()->pageForPoint(toPoints(offset() + event->pos()));
    if (-1 != page) {
        QRect pageRect = fromPoints(PdfViewer::document()->pageRect(page));
        qreal xPos = (offset().x() + event->position().x() - pageRect.x()) / (qreal)pageRect.width();
        qreal yPos = (offset().y() + event->position().y() - pageRect.y()) / (qreal)pageRect.height();
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

    // handle history buttons early
    if (event->button() == Qt::BackButton) {
        historyPrev();
        return;
    } else if (event->button() == Qt::ForwardButton) {
        historyNext();
        return;
    }

    int page = PdfViewer::document()->pageForPoint(toPoints(offset() + event->pos()));

    // start rubber band with right click or shift+left click
    if ((event->button() == Qt::RightButton || event->modifiers().testFlag(Qt::ShiftModifier)) && -1 != page) {
        m_rubberBandOrigin = qMakePair(page, event->pos());
        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin.second, QSize()));
        m_rubberBand->show();

        showHint(QStringLiteral("<b>Drag to copy text in selection...<b>"));

        return;
    }

    // now check if we want to highlight a link location
    if (-1 != page) {
        QRectF pageRect = PdfViewer::document()->pageRect(page);
        QRect pixelPageRect = fromPoints(pageRect);
        qreal xPos = (offset().x() + event->position().x() - pixelPageRect.x()) / (qreal)pixelPageRect.width();
        qreal yPos = (offset().y() + event->position().y() - pixelPageRect.y()) / (qreal)pixelPageRect.height();
        QPointF p = QPointF(xPos, yPos);

        for (auto &l : PdfViewer::document()->links(page)) {
            if (l->boundary().contains(p)) {
                m_mousePressPage = page;
                m_mousePressPageRect = QRectF(pageRect.width() * l->boundary().left(), pageRect.height() * l->boundary().top(), pageRect.width() * l->boundary().width(), pageRect.height() * l->boundary().height());

                if (Poppler::Annotation::ALink == l->subType()) {
                    if (Poppler::Link *link = static_cast<Poppler::LinkAnnotation *>(l.get())->linkDestination()) {
                        switch (link->linkType()) {
                            case Poppler::Link::Goto: {
                                Poppler::LinkDestination gotoLink = static_cast<Poppler::LinkGoto *>(link)->destination();
                                m_mousePressLinkPage = gotoLink.pageNumber() - 1;

                                m_mousePressLinkPageRect = QRectF();
                                if (gotoLink.left() > 0) {
                                    m_mousePressLinkPageRect.setLeft(gotoLink.left() * pageRect.width());
                                    m_mousePressLinkPageRect.setRight(1 + gotoLink.left() * pageRect.width());
                                }
                                if (gotoLink.top() > 0) {
                                    m_mousePressLinkPageRect.setTop(gotoLink.top() * pageRect.height());
                                    m_mousePressLinkPageRect.setBottom(1 + gotoLink.top() * pageRect.height());
                                }
                                if (gotoLink.right() > 0)
                                    m_mousePressLinkPageRect.setRight(gotoLink.right() * pageRect.width());
                                if (gotoLink.bottom() > 0 && gotoLink.bottom() < 1.0)
                                    m_mousePressLinkPageRect.setBottom(gotoLink.bottom() * pageRect.height());

                                m_mousePressLinkPageRect = m_mousePressLinkPageRect.intersected(pageRect.translated(-pageRect.topLeft()));
                            } break;

                            case Poppler::Link::Browse:
                                m_mousePressLinkUrl = static_cast<Poppler::LinkBrowse *>(link)->url();
                                break;

                            default:
                                break;
                        }
                    }
                    break;
                }

                else if (Poppler::Annotation::AText == l->subType() || Poppler::Annotation::ACaret == l->subType()) {
                    QWhatsThis::showText(event->globalPosition().toPoint(), l->contents());
                    return;
                }
            }
        }
    }

    // if there is no Shift pressed we start panning
    if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
        m_panOldOffset = QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
        m_panStartPoint = event->globalPosition().toPoint();

        if (-1 == m_mousePressLinkPage && m_mousePressLinkUrl.isEmpty())
            setCursor(Qt::ClosedHandCursor);
    }
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    // restore mouse cursor
    setCursor(Qt::ArrowCursor);

    // reset pan information
    if (!m_panStartPoint.isNull() && m_panStartPoint != event->globalPosition().toPoint()) {
        m_panStartPoint = QPoint(0, 0);
        return;
    }

    m_panStartPoint = QPoint(0, 0);

    // was there a page to goto?
    if (m_mousePressLinkPage != -1) {
        m_historyStack.add(m_mousePressPage, m_mousePressPageRect);
        m_historyStack.add(m_mousePressLinkPage, m_mousePressLinkPageRect);
        gotoPage(m_mousePressLinkPage, m_mousePressLinkPageRect);
        m_mousePressPage = -1;
        m_mousePressLinkPage = -1;
        return;
    }

    // was there an url to open?
    if (!m_mousePressLinkUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_mousePressLinkUrl));
        m_mousePressLinkUrl.clear();
        return;
    }

    // reset rubber band if needed
    if (m_rubberBandOrigin.first >= 0) {
        slotCopyRequested(m_rubberBandOrigin.first, m_rubberBand->geometry());

        m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
        m_rubberBand->hide();
    }
}

void PageView::showHint(const QString &text, int timeout)
{
    if (text.isEmpty())
        m_hintLabel->hide();

    m_hintLabel->setText(text);
    m_hintLabel->adjustSize();
    m_hintLabel->show();

    m_hintLabelTimer->stop();

    if (timeout)
        m_hintLabelTimer->start(timeout);
}

/*
 * public slots
 */

void PageView::gotoPage(int page, const QRectF &rectToBeVisibleInPoints, bool highlightMatch, bool downwards)
{
    /**
     * filter out invalid pages
     */
    if (page < 0 || page >= PdfViewer::document()->numPages())
        return;

    /**
     * get page rect for the requested page, in points and converted to pixels
     */
    const QRectF pageRectInPoints = PdfViewer::document()->pageRect(page);
    const QRect pageRectInPixel = fromPoints(pageRectInPoints);

    /**
     * create a working copy of the rectToBeVisibleInPoints
     */
    QRectF adjustedRectToBeVisibleInPoints;
    if (!rectToBeVisibleInPoints.isNull()) {
        adjustedRectToBeVisibleInPoints = rectToBeVisibleInPoints;
        adjustedRectToBeVisibleInPoints.setLeft(0);
        adjustedRectToBeVisibleInPoints.setRight(pageRectInPoints.width());
    }

    /**
     * if needed visualize link destination
     */
    if (highlightMatch && !rectToBeVisibleInPoints.isNull()) {
        if (PdfViewer::document()->page(page)->text(adjustedRectToBeVisibleInPoints.adjusted(-5, -5, 5, 5)).isEmpty()) {
            // ensure the rectangle covers any text
            for (int c = 1; c < 100; c += 5) {
                QRectF r = adjustedRectToBeVisibleInPoints.translated(0, downwards ? c : -c);
                QString text = PdfViewer::document()->page(page)->text(r.adjusted(-5, -5, 5, 5));
                if (!text.isEmpty()) {
                    adjustedRectToBeVisibleInPoints = r;
                    break;
                }
            }
        }

        m_highlightRect = fromPoints(adjustedRectToBeVisibleInPoints.translated(pageRectInPoints.topLeft()));
        if (m_highlightRect.height() < 50 * m_zoom) {
            m_highlightRect.setHeight(50 * m_zoom);
            m_highlightRect.translate(0, -25 * m_zoom);
            m_highlightRect.adjust(1, 0, -1, 0);
        }

        // start animation
        QVariantAnimation *va = new QVariantAnimation(this);
        connect(va, &QVariantAnimation::valueChanged, this, &PageView::slotAnimationValueChanged);
        va->setDuration(1000);
        va->setStartValue(0);
        va->setEndValue(100);
        va->start(QAbstractAnimation::DeleteWhenStopped);
    }

    /**
     * transform the adjustedRectToBeVisibleInPoints first to pixel
     */
    QRect toBeVisibleInPixel;
    if (!adjustedRectToBeVisibleInPoints.isNull()) {
        toBeVisibleInPixel = fromPoints(adjustedRectToBeVisibleInPoints);

        /**
         * do not change x value (scroll to 0) if not given (use old value)
         */
        bool scrollXValue = PdfViewer::document()->doubleSided() || (toBeVisibleInPixel.x() != 0);

        /**
         * move it to the page
         */
        toBeVisibleInPixel.translate(pageRectInPixel.topLeft());

        /**
         * add some margin (don't do that if x value should not be changed)
         */
        const int marginToBeSeen = 100;
        QMargins m = QMargins(scrollXValue ? marginToBeSeen : 0, marginToBeSeen, scrollXValue ? marginToBeSeen : 1, marginToBeSeen);
        toBeVisibleInPixel = toBeVisibleInPixel.marginsAdded(m);

        if (!scrollXValue && offset().x() > toBeVisibleInPixel.x())
            toBeVisibleInPixel.moveLeft(offset().x());

        /**
         * finally it is clipped with page rectangle to not end in the void
         */
        toBeVisibleInPixel = toBeVisibleInPixel.intersected(pageRectInPixel);
    }

    if (toBeVisibleInPixel.isEmpty())
        toBeVisibleInPixel = pageRectInPixel;

    toBeVisibleInPixel.adjust(0, -1, 0, 0);

    /**
     * set the new offset - only if needed
     */
    QRect visibleRect = QRect(offset(), QSize(viewport()->width(), pageRectInPixel.height()));
    if (!visibleRect.contains(toBeVisibleInPixel))
        setOffset(toBeVisibleInPixel.topLeft());

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
    if (-1 == m_currentPage)
        return;

    // go to start of current page if not visible or to previous page
    QRect pageRect = fromPoints(PdfViewer::document()->pageRect(m_currentPage));
    QRect visibleRect = QRect(offset(), viewport()->size());

    if (visibleRect.top() <= pageRect.top()) {
        if (pageRect.height() <= visibleRect.height())
            gotoPreviousPage();
        else {
            // special handling here: we have to go the end of the previous page...
            if (m_currentPage > 0) {
                int xOffset = offset().x() - pageRect.x();

                pageRect = fromPoints(PdfViewer::document()->pageRect(m_currentPage - 1));
                int yOffset = qMax(0, pageRect.bottom() - viewport()->height() + 1);
                xOffset += pageRect.x();
                setOffset(QPoint(xOffset, yOffset));
            }
        }
    } else
        setOffset(QPoint(offset().x(), offset().y() - qMin(viewport()->height(), visibleRect.top() - pageRect.top())));
}

void PageView::advance()
{
    if (-1 == m_currentPage)
        return;

    // go to end of current page if not visible or to next page
    QRect pageRect = fromPoints(PdfViewer::document()->pageRect(m_currentPage));
    QRect visibleRect = QRect(offset(), viewport()->size());

    if (visibleRect.bottom() >= pageRect.bottom() - 1) {
        if (m_currentPage < PdfViewer::document()->numPages() - 1)
            gotoNextPage();
    } else
        setOffset(QPoint(offset().x(), offset().y() + qMin(viewport()->height(), pageRect.bottom() - visibleRect.bottom())));
}

void PageView::zoomIn()
{
    setZoom(m_zoom + 0.1);
}

void PageView::zoomOut()
{
    setZoom(m_zoom - 0.1);
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

void PageView::gotoDestinationName(const QString &destination, bool updateHistory, bool downwards)
{
    // try to lookup
    if (auto link = PdfViewer::document()->linkDestination(destination)) {
        // call function that takes description, skip that if page is already bogus
        if (link->pageNumber() > 0)
            gotoDestination(link->toString(), updateHistory, downwards);
    }
}
void PageView::gotoDestination(const QString &destination, bool updateHistory, bool downwards)
{
    if (destination.isEmpty())
        return;

    // directly construct from description
    Poppler::LinkDestination link(destination);
    if (link.pageNumber() > 0) {
        const int pageNumber = link.pageNumber() - 1;
        gotoPage(pageNumber, QRectF(0, (link.isChangeTop() ? link.top() * PdfViewer::document()->pageRect(pageNumber).height() : 0), 1, 1), true, downwards);

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

        if (!text.isEmpty()) {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text, QClipboard::Clipboard);
            clipboard->setText(text, QClipboard::Selection);

            text = text.toHtmlEscaped();
            text.replace(QLatin1String("\n"), QLatin1String("<br>"));

            showHint(QStringLiteral("<b>Text copied:</b><br>%1").arg(text));
        }
    } else
        showHint(QString());
}

void PageView::slotFindStarted()
{
    viewport()->update();
}

void PageView::slotHighlightMatch(int page, const QRectF &rect, bool searchWrapped)
{
    gotoPage(page, rect, false);

    if (searchWrapped)
        showHint(QStringLiteral("<b>Search wrapped</b>"));
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
     * invalidate cache delayed
     * we might alter sizes
     */
    m_clearImageCacheTimer.start();
    m_usePageRect = true;

    /**
     * remember current center of viewport
     */
    QPointF center = toPoints(offset() + QPoint(viewport()->width() / 2, viewport()->height() / 2));

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
    const QSize pageSizeInPixel = (PdfViewer::document()->numPages() > 0) ? fromPoints(PdfViewer::document()->pageRect(0, true)).size() : QSize(100, 100);
    verticalScrollBar()->setSingleStep(pageSizeInPixel.height() / 30);
    horizontalScrollBar()->setSingleStep(pageSizeInPixel.width() / 30);
    verticalScrollBar()->setPageStep(pageSizeInPixel.height());
    horizontalScrollBar()->setPageStep(pageSizeInPixel.width());

    horizontalScrollBar()->setValue(fromPoints(center).x() - viewport()->width() / 2);
    verticalScrollBar()->setValue(fromPoints(center).y() - viewport()->height() / 2);

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
    QMutexLocker locker(m_mutex);
    /**
     * prefer cached image
     */
    if (QImage *cachedPage = m_imageCache.object(pageNumber))
        return *cachedPage;

    /**
     * try to get poppler page => render
     */
    if (Poppler::Page *page = PdfViewer::document()->page(pageNumber)) {
        // unlock to block less
        locker.unlock();

        /**
         * we render in too high resolution and then set the right ratio
         */
        QImage *cachedPage = new QImage(page->renderToImage(resX() * devicePixelRatioF(), resY() * devicePixelRatioF(), -1, -1, -1, -1, Poppler::Page::Rotate0));
        cachedPage->setDevicePixelRatio(devicePixelRatioF());

        // relock before we modify the cache
        locker.relock();
        if (m_imageCache.insert(pageNumber, cachedPage))
            return *cachedPage;
        else
            return QImage();
    }

    /**
     * else return empty image
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
        return fromPoints(1.1 * PdfViewer::document()->pageRect(0, true).size());

    /**
     * else: normal hint
     */
    return QAbstractScrollArea::sizeHint();
}
