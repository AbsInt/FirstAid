/*
 * Copyright (C) 2008, Pino Toscano <pino@kde.org>
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

#include "abstractinfodock.h"

class QTreeWidget;
class QTreeWidgetItem;

class TocDock : public AbstractInfoDock
{
    Q_OBJECT

public:
    TocDock(QWidget *parent = 0);
    ~TocDock();

    /*virtual*/ void documentClosed();

signals:
    void gotoRequested(const QString &dest);

protected:
    /*virtual*/ void fillInfo();

protected slots:
    void itemClicked(QTreeWidgetItem *item, int column);

private:
    QTreeWidget *m_tree = nullptr;
};
