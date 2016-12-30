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

#ifndef NAVIGATIONTOOLBAR_H
#define NAVIGATIONTOOLBAR_H

#include <QToolBar>

#include "documentobserver.h"
#include "pageview.h"

class QAction;
class QComboBox;
class QIntValidator;
class QLabel;
class QLineEdit;

class NavigationToolBar : public QToolBar, public DocumentObserver
{
    Q_OBJECT

public:
    NavigationToolBar(QAction *tocAction, QMenu *menu, QWidget *parent = nullptr);
    ~NavigationToolBar();

    void documentLoaded();
    void documentClosed();
    void pageChanged(int page);

signals:
    void showToc(bool on);
    void zoomChanged(qreal value);
    void zoomModeChanged(PageView::ZoomMode mode);
    void toggleContinous(bool on);
    void lostFocus();

private slots:
    void slotGoFirst();
    void slotGoLast();
    void slotGoPrev();
    void slotGoNext();
    void slotPageSet();
    void slotZoomComboChanged();
    void slotGoto();
    void slotHideGoto();

private:
    QAction *m_prevAct;
    QLabel *m_pageFullLabel;
    QAction *m_pageFullLabelAct;
    QLineEdit *m_pageEdit;
    QAction *m_pageEditAct;
    QLabel *m_pageLabel;
    QAction *m_pageLabelAct;
    QAction *m_nextAct;
    QComboBox *m_zoomCombo;
    QIntValidator *m_intValidator;
};

#endif
