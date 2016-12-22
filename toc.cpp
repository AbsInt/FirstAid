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

#include "toc.h"

#include <poppler-qt5.h>

#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTreeWidget>

static void fillToc(const QDomNode &parent, QTreeWidget *tree, QTreeWidgetItem *parentItem)
{
    QTreeWidgetItem *newitem=nullptr;
    for (QDomNode node=parent.firstChild(); !node.isNull(); node=node.nextSibling()) {
        QDomElement e=node.toElement();

        if (!parentItem)
            newitem=new QTreeWidgetItem(tree, newitem);
        else
            newitem=new QTreeWidgetItem(parentItem, newitem);

        newitem->setText(0, e.tagName());
        newitem->setData(0, Qt::UserRole, e.attribute("DestinationName"));

        bool isOpen = false;
        if (e.hasAttribute(QString::fromLatin1("Open")))
            isOpen=QVariant(e.attribute(QString::fromLatin1("Open"))).toBool();

        if (isOpen)
            tree->expandItem(newitem);

        if (e.hasChildNodes())
            fillToc(node, tree, newitem);
    }
}


TocDock::TocDock(QWidget *parent)
    : AbstractInfoDock(parent)
{
    setWindowTitle(tr("Table of contents"));

    m_tree=new QTreeWidget(this);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->hide();
    m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    setWidget(m_tree);

    connect(m_tree, SIGNAL(itemClicked(QTreeWidgetItem*,int)), SLOT(itemClicked(QTreeWidgetItem*,int)));
}

TocDock::~TocDock()
{
}

void TocDock::fillInfo()
{
    const QDomDocument *toc = document()->toc();
    if (toc) {
        fillToc(*toc, m_tree, 0);
    } else {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, tr("No table of contents available."));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        m_tree->addTopLevelItem(item);
    }
}

void TocDock::documentClosed()
{
    m_tree->clear();
    AbstractInfoDock::documentClosed();
}

void TocDock::itemClicked(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    QString dest=item->data(column, Qt::UserRole).toString();
    emit gotoRequested(dest);
}

#include "toc.moc"
