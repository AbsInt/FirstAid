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

class QLabel;
class QLineEdit;
class QToolButton;
class QTimer;

class FindBar : public QWidget
{
    Q_OBJECT

public:
    FindBar(QWidget *parent = nullptr);
    ~FindBar();

signals:
    void markerRequested(const QRectF &rect);

private slots:
    void slotDocumentChanged();

    void slotFindActionTriggered();
    void slotFind();
    void slotHide();

    void slotFindProgress(qreal progress);
    void slotFindDone();
    void slotResetStyle();
    void slotUpdateStatus();
    void slotReturnPressed();

private:
    QLineEdit *m_findEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QAction *m_acCaseSensitive = nullptr;
    QAction *m_acWholeWords = nullptr;
    QToolButton *m_prevMatch = nullptr;
    QToolButton *m_nextMatch = nullptr;
    QTimer *m_findStartTimer = nullptr;
    QColor m_foundColor;
    QColor m_notFoundColor;
};
