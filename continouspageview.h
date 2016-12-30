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
#ifndef SCROLLVIEW_H
#define SCROLLVIEW_H

#include <QAbstractScrollArea>
#include <QImage>
#include <QCache>

#include "documentobserver.h"
#include "pageview.h"
#include "singlepageview.h"

namespace Poppler
{
class Annotation;
}

class ContinousPageView : public QAbstractScrollArea, public PageView
{
    Q_OBJECT

public:
    ContinousPageView(QWidget *parent = nullptr);
    ~ContinousPageView();

    void gotoPage(int page) override;
    void setDocument(Poppler::Document *document) override;
    void setDoubleSideMode(DoubleSideMode mode) override;
    void setZoomMode(ZoomMode mode) override;
    void setZoom(qreal zoom) override;
signals:
    void currentPageChanged(int page);

protected:
    void paint() override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void slotGotoRequested(const QString &destination);
    void slotCopyRequested(const QRectF &rect);

    void slotFindStarted();
    void slotHighlightMatch(int page, const QRectF &match);
    void slotMatchesFound(int page, const QList<QRectF> &matches);
    void scrolled();

private:
    int pageHeight();
    int pageWidth();
    void updateScrollBars();

private:
    QImage m_image;

    QCache<int,QImage> m_imageCache;

    QPoint m_offset;
};

#endif // SCROLLVIEW_H
