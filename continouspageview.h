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

#include <QImage>
#include <QCache>
#include <QRubberBand>
#include <QAbstractScrollArea>

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

    ~FirstAidPage();

    QImage m_image;
    std::shared_ptr<std::vector<std::unique_ptr<Poppler::Annotation>>> m_annotations;
};

class ContinousPageView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    ContinousPageView(QWidget *parent = nullptr);
    ~ContinousPageView();

public:
    enum ZoomMode { FitWidth, FitPage, Absolute };
    enum DoubleSideMode { None, DoubleSided, DoubleSidedNotFirst };

    static QColor matchColor();
    static QColor highlightColor();

    qreal resX() const;
    qreal resY() const;

    QRectF fromPoints(const QRectF &rect) const;
    QRectF toPoints(const QRectF &rect) const;

    void gotoPage(int page, int offset = 0);
    void setDocument(Poppler::Document *document);
    void setDoubleSideMode(DoubleSideMode mode);
    void setZoomMode(ZoomMode mode);
    void setZoom(qreal zoom);

signals:
    void currentPageChanged(int page);
    void gotoRequested(const QString &dest);
    void copyRequested(const QRectF &area);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *wheelEvent) override;

public slots:
    void gotoPreviousPage();
    void gotoNextPage();

public slots:
    void gotoDestination(const QString &destination);
    void slotCopyRequested(const QRectF &rect);

    void slotFindStarted();
    void slotHighlightMatch(int page, const QRectF &match);
    void slotMatchesFound(int page, const QList<QRectF> &matches);
    void scrolled();

private:
    int pageHeight();
    int pageWidth();
    void updateScrollBars();

    FirstAidPage getPage(int page);

public:
    void setSize(const QSize &size);

    void reset();
    int currentPage() const;

signals:
    void zoomChanged(qreal CurrentZoom);

private:
    Poppler::Document *m_document;
    int m_dpiX;
    int m_dpiY;
    int m_currentPage;
    ZoomMode m_zoomMode;
    qreal m_zoom;
    DoubleSideMode m_doubleSideMode;
    QSize m_size;
    int m_pageHeight = 0;
    
    QCache<int, FirstAidPage> m_imageCache;

    QPoint m_offset;

    QList<QPair<int, QRect>> m_pageRects;
    QList<Poppler::Annotation *> m_annotations;
    QPair<int, QPoint> m_rubberBandOrigin = qMakePair(-1, QPoint(0, 0));
    QRubberBand *m_rubberBand;
};
