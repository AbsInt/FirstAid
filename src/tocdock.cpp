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

#include <QHeaderView>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

TocDock::TocDock(QWidget *parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("Table of contents"));
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // for state saving
    setObjectName("toc_info_dock");

    m_model = new QStandardItemModel(this);

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    QWidget *container = new QWidget(this);
    QVBoxLayout *vbl = new QVBoxLayout(container);
    vbl->setContentsMargins(0, 0, 0, 0);
    setWidget(container);

    m_tree = new QTreeView(this);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->hide();
    m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    vbl->addWidget(m_tree);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText("Filter");
    m_filter->setClearButtonEnabled(true);
    vbl->addWidget(m_filter);

    connect(this, SIGNAL(visibilityChanged(bool)), SLOT(visibilityChanged(bool)));
    connect(m_tree, SIGNAL(clicked(QModelIndex)), SLOT(indexClicked(QModelIndex)));
    connect(m_filter, SIGNAL(textChanged(QString)), SLOT(filterChanged(QString)));

    connect(PdfViewer::document(), SIGNAL(documentChanged()), SLOT(documentChanged()));
    connect(PdfViewer::view(), SIGNAL(pageChanged(int)), SLOT(pageChanged(int)));
    connect(PdfViewer::view(), SIGNAL(pageRequested(int)), SLOT(pageChanged(int)));
}

TocDock::~TocDock()
{
}

void TocDock::fillInfo()
{
    if (const QDomDocument *toc = PdfViewer::document()->toc()) {
        QSet<QModelIndex> openIndices = fillToc(*toc);

        // inform tree about new model
        m_tree->setModel(m_proxyModel);
        m_tree->header()->setStretchLastSection(false);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

        // expand open indices
        foreach (QModelIndex index, openIndices)
            m_tree->setExpanded(index, true);
    } else {
        // tell we found no toc
        QStandardItem *item = new QStandardItem(tr("No table of contents available."));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        m_model->appendRow(item);
        m_tree->setModel(m_model);
    }
}

QSet<QModelIndex> TocDock::fillToc(const QDomNode &parent, QStandardItem *parentItem)
{
    QSet<QModelIndex> openIndices;

    for (QDomNode node = parent.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement e = node.toElement();

        // tag name == link name, strange enough
        QStandardItem *labelItem = new QStandardItem(e.tagName());

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

        // skip destination building if not there
        int pageNumber = 0;
        if (!destination.isEmpty()) {
            Poppler::LinkDestination link(destination);
            pageNumber = link.pageNumber();

            // remember link string representation
            labelItem->setData(link.toString(), Qt::UserRole);
        }

        QStandardItem *pageItem = new QStandardItem(QString::number(pageNumber));
        pageItem->setData(Qt::AlignRight, Qt::TextAlignmentRole);

        if (!parentItem)
            m_model->appendRow(QList<QStandardItem *>() << labelItem << pageItem);
        else
            parentItem->appendRow(QList<QStandardItem *>() << labelItem << pageItem);

        if (!m_pageToIndexMap.contains(pageNumber))
            m_pageToIndexMap.insert(pageNumber, labelItem->index());

        if (e.hasAttribute(QString::fromLatin1("Open"))) {
            if (QVariant(e.attribute(QString::fromLatin1("Open"))).toBool())
                openIndices << labelItem->index();
        }

        if (e.hasChildNodes())
            openIndices += fillToc(node, labelItem);
    }

    return openIndices;
}

/*
 * protected slots
 */

void TocDock::documentChanged()
{
    // reset old data
    m_model->clear();
    m_proxyModel->setFilterRegExp(QRegExp());
    m_tree->setModel(nullptr);
    m_filter->clear();
    m_pageToIndexMap.clear();
    m_markedIndex = QModelIndex();
    m_filled = false;

    // try to fill toc if visible
    if (!isHidden()) {
        fillInfo();
        m_filled = true;
    }
}

void TocDock::pageChanged(int page)
{
    while (m_markedIndex.isValid()) {
        m_model->setData(m_markedIndex, QVariant(), Qt::FontRole);
        m_model->setData(m_markedIndex, QVariant(), Qt::FontRole);
        m_markedIndex = m_markedIndex.parent();
    }

    // init new marked item
    m_markedIndex = m_pageToIndexMap.value(1 + page);

    // special test for double page layout: if left page is not in toc check right page first
    if (!m_markedIndex.isValid() && PdfViewer::document()->doubleSided() && page > 0 && (page % 2) == 1)
        m_markedIndex = m_pageToIndexMap.value(2 + page);

    // still no item found? check previous pages
    while (!m_markedIndex.isValid() && page >= 0)
        m_markedIndex = m_pageToIndexMap.value(1 + page--);

    if (m_markedIndex.isValid()) {
        QFont font = m_tree->font();
        font.setBold(true);

        m_model->setData(m_markedIndex, font, Qt::FontRole);
        m_model->setData(m_markedIndex, font, Qt::FontRole);

        QModelIndex index = m_markedIndex.parent();
        while (index.isValid()) {
            m_model->setData(index, font, Qt::FontRole);
            m_model->setData(index, font, Qt::FontRole);
            index = index.parent();
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

void TocDock::indexClicked(const QModelIndex &index)
{
    QModelIndex firstColumnIndex = index.sibling(index.row(), 0);
    QString dest = firstColumnIndex.data(Qt::UserRole).toString();
    if (!dest.isEmpty())
        PdfViewer::view()->gotoDestination(dest);
}

void TocDock::filterChanged(const QString &text)
{
    m_proxyModel->setFilterRegExp(text);
}
