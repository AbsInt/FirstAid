/*
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

#include <QWidget>

#include "documentobserver.h"

class QLineEdit;
class QToolButton;

class FindBar : public QWidget, public DocumentObserver
{
    Q_OBJECT

public:
    FindBar(QWidget *parent = nullptr);
    ~FindBar();

    void documentLoaded();
    void documentClosed();
    void pageChanged(int page);

signals:
    void markerRequested(const QRectF &rect);

private slots:
    void slotFind();
    void slotHide();

    void slotFindDone();
    void slotResetStyle();

private:
    QLineEdit *m_findEdit = nullptr;
    QToolButton *m_prevMatch = nullptr;
    QToolButton *m_nextMatch = nullptr;
};
