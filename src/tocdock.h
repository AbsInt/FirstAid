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

#include <QDockWidget>
#include <QHash>
#include <QModelIndex>

class QDomNode;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

class TocDock : public QDockWidget
{
    Q_OBJECT

public:
    TocDock(QWidget *parent = 0);
    ~TocDock();

signals:
    void gotoRequested(const QString &dest);

protected:
    void fillInfo();
    QSet<QModelIndex> fillToc(const QDomNode &parent, QStandardItem *parentItem = nullptr);

protected slots:
    void documentChanged();
    void pageChanged(int page);
    void visibilityChanged(bool visible);
    void indexClicked(const QModelIndex &index);
    void filterChanged(const QString &text);

private:
    bool m_filled = false;
    QStandardItemModel *m_model;
    QSortFilterProxyModel *m_proxyModel;
    QTreeView *m_tree = nullptr;
    QLineEdit *m_filter = nullptr;
    QMultiMap<int, QModelIndex> m_pageToIndexMap;
    QModelIndex m_markedIndex;
};
