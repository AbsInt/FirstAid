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

#include "findbar.h"
#include "searchengine.h"

#include <poppler-qt5.h>

#include <QtWidgets/QAction>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>

FindBar::FindBar(QWidget *parent)
       : QWidget(parent)
{
    QHBoxLayout *hbl=new QHBoxLayout(this);
    hbl->setContentsMargins(0, 0, 0, 0);

    QToolButton *tb=new QToolButton(this);
    tb->setIcon(QIcon(":/icons/window-close.png"));
    connect(tb, SIGNAL(clicked()), this, SLOT(hide()));
    hbl->addWidget(tb);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(tr("Find"));
    m_findEdit->setClearButtonEnabled(true);
    connect(m_findEdit, SIGNAL(returnPressed()), SLOT(slotFind()));
    hbl->addWidget(m_findEdit);

    hbl->addStretch();

    QAction *findAction=new QAction(parent);
    findAction->setShortcutContext(Qt::ApplicationShortcut);
    findAction->setShortcut(QKeySequence::Find);
    parent->addAction(findAction);
    connect(findAction, SIGNAL(triggered()), SLOT(show()));
    connect(findAction, SIGNAL(triggered()), SLOT(slotFind()));
    connect(findAction, SIGNAL(triggered()), m_findEdit, SLOT(setFocus()));
    connect(findAction, SIGNAL(triggered()), m_findEdit, SLOT(selectAll()));

    QAction *closeAction=new QAction(this);
    closeAction->setShortcut(Qt::Key_Escape);
    closeAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(closeAction);
    connect(closeAction, SIGNAL(triggered()), SLOT(hide()));

    documentClosed();
}

FindBar::~FindBar()
{
}

void FindBar::documentLoaded()
{
    m_findEdit->setEnabled(true);
}

void FindBar::documentClosed()
{
    hide();
    m_findEdit->clear();
    m_findEdit->setEnabled(false);
}

void FindBar::pageChanged(int)
{
}

void FindBar::slotFind()
{
    SearchEngine::globalInstance()->find(m_findEdit->text());
}

#include "findbar.moc"
