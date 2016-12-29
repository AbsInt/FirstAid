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

#ifndef SINGLEPAGEVIEW_H
#define SINGLEPAGEVIEW_H

#include <QtWidgets/QMenu>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

#include "documentobserver.h"
#include "pageview.h"

class QRubberBand;

namespace Poppler {
class Annotation;
}

class ImageLabel: public QLabel
{
    Q_OBJECT

public:
    ImageLabel(QWidget *parent=nullptr);
    ~ImageLabel();

    void setDisplayRect(const QRect &rect);
    void setAnnotations(const QList<Poppler::Annotation *> &annotations);

signals:
    void gotoRequested(const QString &dest);
    void copyRequested(const QRectF &area);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect m_displayRect;
    QList<Poppler::Annotation *> m_annotations;
    QPoint m_rubberBandOrigin;
    QRubberBand *m_rubberBand;
};

class SinglePageView: public QScrollArea, public PageView
{
    Q_OBJECT

public:
    SinglePageView(QWidget *parent=nullptr);
    ~SinglePageView();

    void gotoPage(int page) override;

signals:
    void currentPageChanged(int page);

protected:
    void paint() override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void slotGotoRequested(const QString &destination);
    void slotCopyRequested(const QRectF &rect);

    void slotFindStarted();
    void slotHighlightMatch(int page, const QRectF &match);
    void slotMatchesFound(int page, const QList<QRectF> &matches);

private:
    ImageLabel *m_imageLabel;
};

#endif // #ifndef SINGLEPAGEVIEW_H
