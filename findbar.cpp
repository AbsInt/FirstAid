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

#include <QTimer>
#include <QAction>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *hbl = new QHBoxLayout(this);
    hbl->setContentsMargins(0, 0, 0, 0);

    QToolButton *tb = new QToolButton(this);
    tb->setIcon(QIcon(":/icons/window-close.svg"));
    connect(tb, SIGNAL(clicked()), SLOT(slotHide()));
    hbl->addWidget(tb);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(tr("Find"));
    m_findEdit->setClearButtonEnabled(true);
    connect(m_findEdit, SIGNAL(returnPressed()), SLOT(slotFind()));
    hbl->addWidget(m_findEdit);

    m_prevMatch=new QToolButton(this);
    m_prevMatch->setIcon(QIcon(":/icons/go-previous.svg"));
    m_prevMatch->setToolTip("Previous match");
    hbl->addWidget(m_prevMatch);

    m_nextMatch=new QToolButton(this);
    m_nextMatch->setIcon(QIcon(":/icons/go-next.svg"));
    m_nextMatch->setToolTip("Next match");
    hbl->addWidget(m_nextMatch);

    QAction *findAction = new QAction(parent);
    findAction->setShortcutContext(Qt::ApplicationShortcut);
    findAction->setShortcut(QKeySequence::Find);
    parent->addAction(findAction);
    connect(findAction, SIGNAL(triggered()), SLOT(show()));
    connect(findAction, SIGNAL(triggered()), SLOT(slotFind()));
    connect(findAction, SIGNAL(triggered()), m_findEdit, SLOT(setFocus()));
    connect(findAction, SIGNAL(triggered()), m_findEdit, SLOT(selectAll()));

    QAction *closeAction = new QAction(this);
    closeAction->setShortcut(Qt::Key_Escape);
    closeAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(closeAction);
    connect(closeAction, SIGNAL(triggered()), SLOT(slotHide()));

    SearchEngine *se=SearchEngine::globalInstance();
    connect(se, SIGNAL(finished()), SLOT(slotFindDone()));
    connect(m_prevMatch, SIGNAL(clicked()), se, SLOT(previousMatch()));
    connect(m_nextMatch, SIGNAL(clicked()), se, SLOT(nextMatch()));

    documentClosed();
}

FindBar::~FindBar()
{
}

void FindBar::documentLoaded()
{
    m_findEdit->setEnabled(true);
    m_nextMatch->setEnabled(true);
    m_prevMatch->setEnabled(true);
}

void FindBar::documentClosed()
{
    slotHide();
    m_findEdit->clear();
    m_findEdit->setEnabled(false);
    m_nextMatch->setEnabled(false);
    m_prevMatch->setEnabled(false);
}

void FindBar::pageChanged(int)
{
}

void FindBar::slotFind()
{
    SearchEngine::globalInstance()->find(m_findEdit->text());
}

void FindBar::slotHide()
{
    hide();
    SearchEngine::globalInstance()->find(QString());
}

void FindBar::slotFindDone()
{
    if (SearchEngine::globalInstance()->matches().isEmpty()) {
        m_findEdit->setStyleSheet("background-color: #f08080");
        QTimer::singleShot(5000, this, SLOT(slotResetStyle()));
    }
}

void FindBar::slotResetStyle()
{
    m_findEdit->setStyleSheet(QString());
}
