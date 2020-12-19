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
#include "pageview.h"
#include "viewer.h"

#include <poppler-qt6.h>

#include <QHeaderView>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#define DestinationRole (Qt::UserRole + 1)
#define FilterRole (Qt::UserRole + 2)
#define UnicodeOr QChar(0x22c1)

class MySortFilterProxyModel : public QSortFilterProxyModel
{
public:
    MySortFilterProxyModel(QObject *parent)
        : QSortFilterProxyModel(parent)
    {
    }

    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const
    {
        if (Qt::BackgroundRole == role && !filterRegExp().isEmpty()) {
            if (-1 != filterRegExp().indexIn(proxyIndex.data().toString()))
                return QVariant::fromValue(PageView::matchColor());
        }

        return QSortFilterProxyModel::data(proxyIndex, role);
    }
};

TocDock::TocDock(QWidget *parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("Table of contents"));
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // for state saving
    setObjectName(QStringLiteral("toc_info_dock"));

    m_model = new QStandardItemModel(this);

    m_proxyModel = new MySortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterRole(FilterRole);

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
    m_filter->setPlaceholderText(tr("Filter"));
    m_filter->setClearButtonEnabled(true);
    vbl->addWidget(m_filter);

    connect(this, SIGNAL(visibilityChanged(bool)), SLOT(visibilityChanged(bool)));
    connect(m_tree, SIGNAL(clicked(QModelIndex)), SLOT(indexClicked(QModelIndex)));

    connect(PdfViewer::document(), SIGNAL(documentChanged()), SLOT(documentChanged()));
    connect(PdfViewer::view(), SIGNAL(pageChanged(int)), SLOT(pageChanged(int)));
    connect(PdfViewer::view(), SIGNAL(pageRequested(int)), SLOT(pageChanged(int)));

    m_findStartTimer = new QTimer(this);
    m_findStartTimer->setSingleShot(true);
    m_findStartTimer->setInterval(1000);
    connect(m_findStartTimer, SIGNAL(timeout()), SLOT(setFilter()));
    connect(m_filter, SIGNAL(textChanged(QString)), m_findStartTimer, SLOT(start()));
    connect(m_proxyModel, SIGNAL(layoutChanged()), SLOT(expand()));
}

TocDock::~TocDock()
{
}

void TocDock::fillInfo()
{
    const auto toc = PdfViewer::document()->toc();
    if (!toc.isEmpty()) {
        QSet<QModelIndex> openIndices = fillToc(toc);

        // inform tree about new model
        m_tree->setModel(m_proxyModel);
        m_tree->header()->setStretchLastSection(false);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

        // expand open indices
        for (const QModelIndex &index : openIndices)
            m_tree->setExpanded(m_proxyModel->mapFromSource(index), true);
    } else {
        // tell we found no toc
        QStandardItem *item = new QStandardItem(tr("No table of contents available."));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        m_model->appendRow(item);
        m_tree->setModel(m_model);
    }
}

QSet<QModelIndex> TocDock::fillToc(const QVector<Poppler::OutlineItem> &items, QStandardItem *parentItem)
{
    QSet<QModelIndex> openIndices;

    for (const auto &item : items) {
        // tag name == link name, strange enough
        QStandardItem *labelItem = new QStandardItem(item.name());
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);

        /**
         * use raw string for destination or convert the named one
         */
        QString destination;
        if (item.destination()) {
            if (item.destination()->pageNumber() > 0)
                destination = item.destination()->toString();
        }

        // skip destination building if not there
        int pageNumber = 0;
        if (!destination.isEmpty()) {
            Poppler::LinkDestination link(destination);
            pageNumber = link.pageNumber();

            // remember link string representation
            labelItem->setData(link.toString(), DestinationRole);
        }

        QStandardItem *pageItem = new QStandardItem(QString::number(pageNumber));
        pageItem->setFlags(pageItem->flags() & ~Qt::ItemIsEditable);
        pageItem->setData(Qt::AlignRight, Qt::TextAlignmentRole);

        if (!parentItem)
            m_model->appendRow(QList<QStandardItem *>() << labelItem << pageItem);
        else
            parentItem->appendRow(QList<QStandardItem *>() << labelItem << pageItem);

        m_pageToIndexMap.insert(pageNumber, labelItem->index());

        if (item.isOpen())
            openIndices << labelItem->index();

        if (item.hasChildren())
            openIndices += fillToc(item.children(), labelItem);

        // adjust filter role
        QStringList filterRoles;
        for (int c = 0; c < labelItem->rowCount(); c++)
            filterRoles << labelItem->child(c, 0)->data(FilterRole).toString();

        filterRoles.prepend(labelItem->text());

        labelItem->setData(filterRoles.join(UnicodeOr), FilterRole);
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
    QList<QModelIndex> indices = m_pageToIndexMap.values(1 + page);
    m_markedIndex = (indices.count() > 0 ? indices.last() : QModelIndex());

    // special test for double page layout: if left page is not in toc check right page first
    if (!m_markedIndex.isValid() && PdfViewer::document()->doubleSided() && page > 0 && (page % 2) == 1) {
        indices = m_pageToIndexMap.values(2 + page);
        m_markedIndex = (indices.count() > 0 ? indices.last() : QModelIndex());
    }

    // still no item found? check previous pages
    while (!m_markedIndex.isValid() && page >= 0) {
        indices = m_pageToIndexMap.values(1 + page--);
        m_markedIndex = (indices.count() > 0 ? indices.first() : QModelIndex());
    }

    // if there is a selected index with the same page use this index instead
    if (m_tree->selectionModel()) {
        QModelIndexList selected = m_tree->selectionModel()->selectedIndexes();
        for (const QModelIndex &idx : selected) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(idx);
            if (indices.contains(sourceIndex)) {
                m_markedIndex = sourceIndex;
                break;
            }
        }
    }

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
    QString dest = firstColumnIndex.data(DestinationRole).toString();
    if (!dest.isEmpty())
        PdfViewer::view()->gotoDestination(dest);
}

void TocDock::setFilter()
{
    m_findStartTimer->stop();
    m_proxyModel->setFilterRegExp(QRegExp(m_filter->text(), Qt::CaseInsensitive));
    m_proxyModel->invalidate();
}

void TocDock::expand()
{
    if (!m_filter->text().isEmpty())
        m_tree->expandAll();
}
