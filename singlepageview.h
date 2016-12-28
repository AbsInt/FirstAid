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

#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

#include "documentobserver.h"
#include "pageview.h"

namespace Poppler {
class Annotation;
}

class ImageLabel: public QLabel
{
    Q_OBJECT

public:
    ImageLabel(QWidget *parent=nullptr);
    ~ImageLabel();

    void setAnnotations(const QList<Poppler::Annotation *> &annotations);

signals:
    void gotoRequested(const QString &dest);

protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    QList<Poppler::Annotation *> m_annotations;
};

class SinglePageView: public QScrollArea, public PageView
{
    Q_OBJECT

public:
    SinglePageView(QWidget *parent=nullptr);
    ~SinglePageView();

    void gotoPage(int page) override;

protected:
    void paint() override;

private slots:
    void slotGotoRequested(const QString &destination);
    void slotFindStarted();
    void slotHighlightMatch(int page, const QRectF &match);
    void slotMatchesFound(int page, const QList<QRectF> &matches);

private:
    ImageLabel *m_imageLabel;
};

#endif // #ifndef SINGLEPAGEVIEW_H
