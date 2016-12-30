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
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QDesktopWidget>
#include <QLabel>
#include <QRubberBand>
#include <QScrollBar>

#include <QDebug>

#define PAGEDISTANCE 20

/*
 * constructors / destructor
 */

ContinousPageView::ContinousPageView(QWidget *parent)
    : QAbstractScrollArea(parent)
    , PageView()
{
    setMouseTracking(true);

    m_imageCache.setMaxCost(7);

    m_imageLabel = new ImageLabel(this);
    m_imageLabel->resize(0, 0);

    connect(m_imageLabel, SIGNAL(gotoRequested(QString)), SLOT(slotGotoRequested(QString)));
    connect(m_imageLabel, SIGNAL(copyRequested(QRectF)), SLOT(slotCopyRequested(QRectF)));

    SearchEngine *se = SearchEngine::globalInstance();
    connect(se, SIGNAL(started()), SLOT(slotFindStarted()));
    connect(se, SIGNAL(highlightMatch(int, QRectF)), SLOT(slotHighlightMatch(int, QRectF)));
    connect(se, SIGNAL(matchesFound(int, QList<QRectF>)), SLOT(slotMatchesFound(int, QList<QRectF>)));

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrolled()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scrolled()));

    horizontalScrollBar()->setSingleStep(1);
    verticalScrollBar()->setSingleStep(1);

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
    int page = value / pageSize;

    // set page
    value -= (page * pageSize);
    // scroll(page,value);
    m_offset = value;
    if (page < m_document->numPages())
        m_currentPage = page;
    paint();
}

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
    return (pageSize * resY()) / 72.0;;
}

/*
 * public methods
 */

void ContinousPageView::gotoPage(int page)
{
    if (!m_document || page < 0 || page >= m_document->numPages())
        return;

    m_currentPage = page;
    QScrollBar *vbar = verticalScrollBar();

    vbar->setValue(page * pageHeight());
    paint();

    emit currentPageChanged(m_currentPage);
}

/*
 * protected methods
 */

void ContinousPageView::paint()
{
    QImage* image = nullptr;
    QList<Poppler::Annotation *> annotations;

    if (!m_document)
        return;

    int currentPage = m_currentPage;

    QScrollBar *vbar = verticalScrollBar();
    const int pageCount = m_document->numPages();
    int pageSize = pageHeight();
    vbar->setRange(0, pageCount * pageSize - viewport()->height());
    vbar->blockSignals(true);
    vbar->setValue(currentPage * pageSize + m_offset);
    vbar->blockSignals(false);

    QSize vs = viewport()->size();

    QImage centeredImage(viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    centeredImage.fill(Qt::gray);
    QPainter p(&centeredImage);

    SearchEngine *se = SearchEngine::globalInstance();
    int matchPage;
    QRectF matchRect;
    se->currentMatch(matchPage, matchRect);

    QPoint pageStart = QPoint(0, -m_offset);

    while (pageStart.y() < 0 || vs.height() > (pageStart.y() + PAGEDISTANCE)) {
        // draw another page
        image = nullptr;

        if (Poppler::Page *page = m_document->page(currentPage)) {
            image = m_imageCache.take(currentPage);
            if (!image)
            {
                image = new QImage();
                *image = page->renderToImage(resX(), resY(), -1, -1, -1, -1, Poppler::Page::Rotate0);
            }

            annotations += page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink);
            delete page;
        }

        if (!image || image->isNull())
            break;

        pageStart.setX(qMax(0, vs.width() - image->width()) / 2);

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

        p.setPen(Qt::darkGray);
        p.drawRect(QRect(pageStart, image->size()).adjusted(-1, -1, 1, 1));

        //insert image into cache
        m_imageCache.insert(currentPage,image);

        // set next page
        ++currentPage;

        pageStart.setY(pageStart.y() + image->height() + PAGEDISTANCE);
    }

    p.end();

    m_imageLabel->resize(centeredImage.size());
    m_imageLabel->setPixmap(QPixmap::fromImage(centeredImage));

    m_imageLabel->setAnnotations(annotations);
}

void ContinousPageView::resizeEvent(QResizeEvent * /*resizeEvent*/)
{
    m_imageCache.clear();
    setSize(viewport()->size() - QSize(1, 1));
    if (m_document) {
        QScrollBar *vbar = verticalScrollBar();
        const int pageCount = m_document->numPages();

        vbar->setRange(0, pageCount * pageHeight() - viewport()->height());
    }
    paint();
}

void ContinousPageView::keyPressEvent(QKeyEvent *event)
{
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
    paint();
}

void ContinousPageView::slotHighlightMatch(int page, const QRectF &)
{
    gotoPage(page);
}

void ContinousPageView::slotMatchesFound(int page, const QList<QRectF> &)
{
    if (page == m_currentPage)
        paint();
}