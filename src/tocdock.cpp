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

#include "tocdock.h"
#include "viewer.h"

#include <poppler-qt5.h>

#include <QDebug>
#include <QHeaderView>
#include <QTreeWidget>

TocDock::TocDock(QWidget *parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("Table of contents"));

    // for state saving
    setObjectName("toc_info_dock");

    m_tree = new QTreeWidget(this);
    m_tree->setAlternatingRowColors(true);
    m_tree->setColumnCount(2);
    m_tree->header()->hide();
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    setWidget(m_tree);

    connect(this, SIGNAL(visibilityChanged(bool)), SLOT(visibilityChanged(bool)));
    connect(m_tree, SIGNAL(itemClicked(QTreeWidgetItem *, int)), SLOT(itemClicked(QTreeWidgetItem *, int)));

    connect(PdfViewer::document(), SIGNAL(documentChanged()), SLOT(documentChanged()));
    connect(PdfViewer::view(), SIGNAL(pageChanged(int)), SLOT(pageChanged(int)));
}

TocDock::~TocDock()
{
}

void TocDock::fillInfo()
{
    if (const QDomDocument *toc = PdfViewer::document()->toc()) {
        fillToc(*toc, m_tree, 0);
        return;
    }
    
    // tell we found no toc
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, tr("No table of contents available."));
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    m_tree->addTopLevelItem(item);
}

void TocDock::fillToc(const QDomNode &parent, QTreeWidget *tree, QTreeWidgetItem *parentItem)
{
    
    QTreeWidgetItem *newitem = nullptr;
    for (QDomNode node = parent.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement e = node.toElement();

        if (!parentItem)
            newitem = new QTreeWidgetItem(tree, newitem);
        else
            newitem = new QTreeWidgetItem(parentItem, newitem);

        // tag name == link name, strange enough
        newitem->setText(0, e.tagName());
        
        /**
         * use raw string for destination or convert the named one
         */
        QString destination = e.attribute("Destination");
        if (!e.attribute("DestinationName").isNull()) {
            Poppler::LinkDestination *link = PdfViewer::document()->linkDestination(e.attribute("DestinationName"));
            if (link && link->pageNumber() > 0)
                destination = link->toString();
            delete link;
        }

        Poppler::LinkDestination link(destination);
        int pageNumber = link.pageNumber();
        
        // remember link string representation
        newitem->setData(0, Qt::UserRole, link.toString());

        newitem->setText(1, QString::number(pageNumber));
        newitem->setData(1, Qt::TextAlignmentRole, Qt::AlignRight);

        if (!m_pageToItemMap.contains(pageNumber))
            m_pageToItemMap.insert(pageNumber, newitem);

        bool isOpen = false;
        if (e.hasAttribute(QString::fromLatin1("Open")))
            isOpen = QVariant(e.attribute(QString::fromLatin1("Open"))).toBool();

        if (isOpen)
            tree->expandItem(newitem);

        if (e.hasChildNodes())
            fillToc(node, tree, newitem);
    }
}

/*
 * protected slots
 */

void TocDock::documentChanged()
{
    // reset old data
    m_tree->clear();
    m_pageToItemMap.clear();
    m_markedItem = nullptr;
    m_filled = false;

    // try to fill toc if visible
    if (!isHidden()) {
        fillInfo();
        m_filled = true;
    }
}

void TocDock::pageChanged(int page)
{
    if (m_markedItem) {
        m_markedItem->setData(0, Qt::FontRole, QVariant());
        m_markedItem->setData(1, Qt::FontRole, QVariant());

        while (m_markedItem) {
            m_markedItem->setData(0, Qt::FontRole, QVariant());
            m_markedItem->setData(1, Qt::FontRole, QVariant());
            m_markedItem = m_markedItem->parent();
        }
    }

    while (!m_markedItem && page >= 0)
        m_markedItem = m_pageToItemMap.value(1 + page--);

    if (m_markedItem) {
        QFont font = m_tree->font();
        font.setBold(true);
        m_markedItem->setData(0, Qt::FontRole, font);
        m_markedItem->setData(1, Qt::FontRole, font);

        QTreeWidgetItem *item = m_markedItem;
        while (item) {
            item->setData(0, Qt::FontRole, font);
            item->setData(1, Qt::FontRole, font);
            item = item->parent();
        }
    }
}

void TocDock::visibilityChanged(bool visible)
{
    if (visible && !m_filled) {
        fillInfo();
        m_filled = true;
    }
}

void TocDock::itemClicked(QTreeWidgetItem *item, int)
{
    if (!item)
        return;

    QString dest = item->data(0, Qt::UserRole).toString();
    PdfViewer::view()->gotoDestination(dest);
}
