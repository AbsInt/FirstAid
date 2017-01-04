/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
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
#include <QRubberBand>
#include <QShortcut>

#include <poppler-qt5.h>

#include "documentobserver.h"

#define PAGEFRAME 5

class QRubberBand;

class FirstAidPage
{
public:
    FirstAidPage(QImage image, const QList<Poppler::Annotation *> &annotations)
        : m_image(image)
        , m_annotations(new std::vector<std::unique_ptr<Poppler::Annotation>>())
    {
        foreach (auto *a, annotations)
            m_annotations->push_back(std::unique_ptr<Poppler::Annotation>(a));
    }

    QImage m_image;
    std::shared_ptr<std::vector<std::unique_ptr<Poppler::Annotation>>> m_annotations;
};

class PageView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    PageView(QWidget *parent = nullptr);
    ~PageView();

public:
    enum ZoomMode { FitWidth, FitPage, Absolute };
    enum DoubleSideMode { None, DoubleSided, DoubleSidedNotFirst };

    static QColor matchColor();
    static QColor highlightColor();

    qreal resX() const;
    qreal resY() const;

    QRectF fromPoints(const QRectF &rect) const;
    QRectF toPoints(const QRectF &rect) const;

    void setDocument(Poppler::Document *document);
    void setDoubleSideMode(DoubleSideMode mode);
    void setZoomMode(ZoomMode mode);
    void setZoom(qreal zoom);

signals:
    void currentPageChanged(int page);
    void copyRequested(const QRectF &area);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *wheelEvent) override;

public slots:
    void gotoPage(int page, int offset = -1);
    void gotoPage(int page, const QRectF &rect);
    void gotoPreviousPage();
    void gotoNextPage();
    void stepBack();
    void advance();
    void zoomIn();
    void zoomOut();
    void zoomOriginal();

    void gotoDestination(const QString &destination);
    void slotCopyRequested(const QRectF &rect);

    void slotFindStarted();
    void slotMatchesFound(int page, const QList<QRectF> &matches);
    void scrolled();

private:
    int pageHeight();
    int pageWidth();

    /**
     * Update viewport dimensions after:
     *  - document change
     *  - change of zoom variant/factor
     *  - change of single/double page mode
     *
     * @param invalidateCache invalidate page cache? default true, only use that for position setting with false
     */
    void updateViewSize(bool invalidateCache = true);

    FirstAidPage getPage(int page);

public:
    int currentPage() const;

signals:
    void zoomChanged(qreal CurrentZoom);

private:
    Poppler::Document *m_document = nullptr;
    int m_dpiX = 72;
    int m_dpiY = 72;
    int m_currentPage = 0;
    ZoomMode m_zoomMode = Absolute;
    qreal m_zoom = 1.0;
    DoubleSideMode m_doubleSideMode = None;
    int m_pageHeight = 0;

    QCache<int, FirstAidPage> m_imageCache;

    QPoint m_offset;

    QList<QPair<int, QRect>> m_pageRects;
    QList<Poppler::Annotation *> m_annotations;
    QPair<int, QPoint> m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
    QRubberBand *m_rubberBand = nullptr;
};
