/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
 * Copyright (C) 2016, Marc Langenbach <mlangen@absint.com>
 * Copyright (C) 2016, Christoph Cullmann <cullmann@absint.com>
 * Copyright (C) 2016, Jan Pohland <pohland@absint.com>
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

#include <memory>

#include <QAbstractScrollArea>
#include <QCache>
#include <QImage>
#include <QTimer>

#include <poppler-qt5.h>

#include "document.h"
#include "historystack.h"

class QRubberBand;
class QVariantAnimation;

class PageView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    PageView(QWidget *parent = nullptr);
    ~PageView();

public:
    enum ZoomMode { FitWidth, FitPage, Absolute };

    QPoint offset() const;
    int currentPage() const;

public slots:
    void setZoomMode(PageView::ZoomMode mode);
    void setZoom(qreal zoom);
    void slotDocumentChanged();
    void slotLayoutChanged();
    void slotAnimationValueChanged(const QVariant &value);

public slots:
    /**
     * goto the given page and try to make the wanted rectange visible
     * @param page page to show
     * @param rectToBeVisibleInPoints rectangle to make visible
     */
    void gotoPage(int page, const QRectF &rectToBeVisibleInPoints = QRectF(), bool showHighlight = true);

    void gotoPreviousPage();
    void gotoNextPage();
    void stepBack();
    void advance();
    void zoomIn();
    void zoomOut();

    void historyPrev();
    void historyNext();

    /**
     * Go to given destination name (named destination)
     * @param destination named destination to go to
     * @param updateHistory update jump history?
     */
    void gotoDestinationName(const QString &destination, bool updateHistory = true);

    /**
     * Go to given destination (raw string representation parsable by LinkDestination constructor)
     * @param destination string representation of destination to go to
     * @param updateHistory update jump history?
     */
    void gotoDestination(const QString &destination, bool updateHistory = true);

    void gotoHistoryEntry(const HistoryEntry &entry);
    void slotCopyRequested(int page, const QRect &viewportRect);

    void slotFindStarted();
    void slotHighlightMatch(int page, const QRectF &rect);
    void slotMatchesFound(int page, const QList<QRectF> &matches);
    void updateCurrentPage();

    void setOffset(const QPoint &point);

    QSize sizeHint() const override;

signals:
    void copyRequested(const QRectF &area);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *wheelEvent) override;

private slots:

    /**
     * slot to trigger viewsize update
     */
    void slotUpdateViewSize();

private:
    /**
     * Color for search matches.
     * @return search match color
     */
    static QColor matchColor()
    {
        return QColor(255, 255, 0, 64);
    }

    /**
     * Color for currently highlighted search match.
     * @return highlighed search match color
     */
    static QColor highlightColor()
    {
        return QColor(255, 128, 0, 128);
    }

    /**
     * Update viewport dimensions after:
     *  - document change
     *  - change of zoom variant/factor
     *  - change of single/double page mode
     * @param zoom new fixed zoom value if any
     */
    void updateViewSize(qreal zoom = -1.0);

    /**
     * Current resolution for X axis, using right DPI + zoom.
     * @return X axis resolution
     */
    qreal resX() const
    {
        return logicalDpiX() * m_zoom;
    }

    /**
     * Current resolution for Y axis, using right DPI + zoom.
     * @return Y axis resolution
     */
    qreal resY() const
    {
        return logicalDpiY() * m_zoom;
    }

    /**
     * Convert from points to pixels using the current DPI and zoom.
     * @param rect input in points to convert
     * @return result in pixel
     */
    QRectF fromPoints(const QRectF &rect) const
    {
        return QRectF(rect.left() / 72.0 * resX(), rect.top() / 72.0 * resY(), rect.width() / 72.0 * resX(), rect.height() / 72.0 * resY());
    }

    /**
     * Convert from pixels to points using the current DPI and zoom.
     * @param rect input in pixel to convert
     * @return result in point
     */
    QRectF toPoints(const QRectF &rect) const
    {
        return QRectF(rect.left() * 72.0 / resX(), rect.top() * 72.0 / resY(), rect.width() * 72.0 / resX(), rect.height() * 72.0 / resY());
    }

    /**
     * Convert from points to pixels using the current DPI and zoom.
     * @param rect input in points to convert
     * @return result in pixel
     */
    QSizeF fromPoints(const QSizeF &rect) const
    {
        return QSizeF(rect.width() / 72.0 * resX(), rect.height() / 72.0 * resY());
    }

    /**
     * Convert from pixels to points using the current DPI and zoom.
     * @param rect input in pixel to convert
     * @return result in point
     */
    QSizeF toPoints(const QSizeF &rect) const
    {
        return QSizeF(rect.width() * 72.0 / resX(), rect.height() * 72.0 / resY());
    }

    /**
     * Convert from points to pixels using the current DPI and zoom.
     * @param rect input in points to convert
     * @return result in pixel
     */
    QPointF fromPoints(const QPointF &rect) const
    {
        return QPointF(rect.x() / 72.0 * resX(), rect.y() / 72.0 * resY());
    }

    /**
     * Convert from pixels to points using the current DPI and zoom.
     * @param rect input in pixel to convert
     * @return result in point
     */
    QPointF toPoints(const QPointF &rect) const
    {
        return QPointF(rect.x() * 72.0 / resX(), rect.y() * 72.0 / resY());
    }

    /**
     * Get prerendered image for given page.
     * Uses some cache to not render the last X pages again and again.
     * @param page requested page
     * @return rendered page as image, HiDPI aware
     */
    QImage getPage(int page);

signals:
    void pageChanged(int page);
    void pageRequested(int page);
    void zoomChanged(qreal CurrentZoom);

private:
    /**
     * members to handle zooming
     */
    ZoomMode m_zoomMode = Absolute;
    qreal m_zoom = 1.0;

    /**
     * the current page in the range from 0 to numpages-1
     */
    int m_currentPage = -1;

    /*
     * cache for already created images, key is the page number
     */
    QCache<int, QImage> m_imageCache;

    /**
     * current offset of viewport
     */
    QPoint m_offset;

    /**
     * members for handling the rubber band
     * m_rubberBandOrigin is a pair of page number and offset
     */
    QPair<int, QPoint> m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
    QRubberBand *m_rubberBand = nullptr;

    /**
     * members needed for handling panning
     */
    QPoint m_panOldOffset;  //! the current offset when panning starts
    QPoint m_panStartPoint; //! the global cursor position when panning starts

    /**
     * members for handling clicking on  links
     */
    int m_mousePressLinkPage = -1;   //! page of link at mouse press
    QRectF m_mousePressLinkPageRect; //! rect for page of link
    QString m_mousePressLinkUrl;     //! url at mouse press

    /**
     * the history stack for navigation
     */
    HistoryStack m_historyStack;

    /**
     * delayed updating of view size
     */
    QTimer m_updateViewSizeTimer;

    /**
     * area in the viewport to highlight via animation
     */
    QRect m_highlightRect;
    int m_highlightValue = 0;
};
