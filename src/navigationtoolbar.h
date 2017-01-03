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

#pragma once

#include <QToolBar>

#include "documentobserver.h"
#include "pageview.h"

class QAction;
class QComboBox;
class QIntValidator;
class QLabel;
class QLineEdit;
class QToolButton;

class NavigationToolBar : public QToolBar, public DocumentObserver
{
    Q_OBJECT

public:
    NavigationToolBar(QAction *tocAction, QMenu *menu, QWidget *parent = nullptr);
    ~NavigationToolBar();

    void documentLoaded() override;
    void documentClosed() override;
    void pageChanged(int page) override;

    bool eventFilter(QObject *object, QEvent *e) override;

signals:
    void showToc(bool on);
    void zoomChanged(qreal value);
    void zoomModeChanged(PageView::ZoomMode mode);
    void toggleFacingPages(bool on);

private slots:
    void slotGoFirst();
    void slotGoLast();
    void slotGoPrev();
    void slotGoNext();
    void slotPageSet();
    void slotZoomChanged();
    void slotGoto();
    void slotHideGoto();
    void slotToggleFacingPages();

    void slotChangeZoom(qreal currentZoom);

private:
    QAction *m_toggleFacingPagesAct = nullptr;

    QAction *m_prevAct = nullptr;
    QLineEdit *m_pageEdit = nullptr;
    QIntValidator *m_intValidator = nullptr;
    QAction *m_pageEditAct = nullptr;
    QLabel *m_pageLabel = nullptr;
    QAction *m_pageLabelAct = nullptr;
    QAction *m_nextAct = nullptr;

    QLabel *m_zoomLabel = nullptr;
    QAction *m_zoomLabelAct = nullptr;
    QToolButton *m_zoomButton = nullptr;
};
