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

class PageView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    PageView(QWidget *parent = nullptr);
    ~PageView();

public:
    enum ZoomMode { FitWidth, FitPage, Absolute };

    static QColor matchColor();
    static QColor highlightColor();

    qreal resX() const;
    qreal resY() const;

    
    QRectF fromPoints(const QRectF &rect) const
    {
        return QRectF(rect.left() / 72.0 * resX(), rect.top() / 72.0 * resY(), rect.width() / 72.0 * resX(), rect.height() / 72.0 * resY());
    }

    QRectF toPoints(const QRectF &rect) const
    {
        return QRectF(rect.left() * 72.0 / resX(), rect.top() * 72.0 / resY(), rect.width() * 72.0 / resX(), rect.height() * 72.0 / resY());
    }

    
    QSizeF fromPoints(const QSizeF &rect) const
    {
        return QSizeF(rect.width() / 72.0 * resX(), rect.height() / 72.0 * resY());
    }

    QSizeF toPoints(const QSizeF &rect) const
    {
        return QSizeF(rect.width() * 72.0 / resX(), rect.height() * 72.0 / resY());
    }

    QPointF fromPoints(const QPointF &rect) const
    {
        return QPointF(rect.x() / 72.0 * resX(), rect.y() / 72.0 * resY());
    }

    QPointF toPoints(const QPointF &rect) const
    {
        return QPointF(rect.x() * 72.0 / resX(), rect.y() * 72.0 / resY());
    }


    QPoint offset() const;
    int currentPage() const;
    bool doubleSided() const;

public slots:
    void setDoubleSided(bool on);
    void setZoomMode(PageView::ZoomMode mode);
    void setZoom(qreal zoom);
    void slotDocumentChanged();

public slots:
    /**
     * goto the given page and try to make the wanted rectange visible
     * @param page page to show
     * @param rectToBeVisibleInPoints rectangle to make visible
     */
    void gotoPage(int page, const QRectF &rectToBeVisibleInPoints = QRectF());

    void gotoPreviousPage();
    void gotoNextPage();
    void stepBack();
    void advance();
    void zoomIn();
    void zoomOut();
    void zoomOriginal();

    void historyPrev();
    void historyNext();

    void gotoDestination(const QString &destination, bool updateHistory = true);
    void gotoHistoryEntry(const HistoryEntry &entry);
    void slotCopyRequested(int page, const QRectF &rect);

    void slotFindStarted();
    void slotMatchesFound(int page, const QList<QRectF> &matches);
    void updateCurrentPage();

    void setOffset(const QPoint &point);

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
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    /**
     * Update viewport dimensions after:
     *  - document change
     *  - change of zoom variant/factor
     *  - change of single/double page mode
     */
    void updateViewSize();

private:
    QImage getPage(int page);

signals:
    void pageChanged(int page);
    void zoomChanged(qreal CurrentZoom);

private:
    int m_dpiX = 72;
    int m_dpiY = 72;
    ZoomMode m_zoomMode = Absolute;
    qreal m_zoom = 1.0;
    bool m_doubleSided = false;
    int m_currentPage = -1;

    QCache<int, QImage> m_imageCache;

    QPoint m_offset;

    QPair<int, QPoint> m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
    QRubberBand *m_rubberBand = nullptr;

    QPoint m_panOldOffset;  //! the current offset when panning starts
    QPoint m_panStartPoint; //! the global cursor position then panning starts

    int m_mousePressPage = -1;      //! page of link at mouse press
    int m_mousePressPageOffset = 0; //! offset for page of link
    QString m_mousePressUrl;        //! url at mouse press

    HistoryStack m_historyStack;

    /**
     * delayed updating of view size
     */
    QTimer m_updateViewSizeTimer;
};
