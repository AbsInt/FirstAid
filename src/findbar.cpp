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
#include "viewer.h"

#include <poppler-qt5.h>

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
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
    connect(m_findEdit, SIGNAL(textChanged(QString)), SLOT(slotResetStyle()));
    hbl->addWidget(m_findEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setText("0 of 0");
    hbl->addWidget(m_statusLabel);

    m_prevMatch = new QToolButton(this);
    m_prevMatch->setIcon(QIcon(":/icons/go-up.svg"));
    m_prevMatch->setToolTip("Previous match");
    hbl->addWidget(m_prevMatch);

    m_nextMatch = new QToolButton(this);
    m_nextMatch->setIcon(QIcon(":/icons/go-down.svg"));
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

    SearchEngine *se = PdfViewer::searchEngine();
    connect(se, SIGNAL(started()), SLOT(slotUpdateStatus()));
    connect(se, SIGNAL(progress(qreal)), SLOT(slotFindProgress(qreal)));
    connect(se, SIGNAL(finished()), SLOT(slotFindDone()));
    connect(se, SIGNAL(highlightMatch(int, QRectF)), SLOT(slotUpdateStatus()));
    connect(se, SIGNAL(matchesFound(int, QList<QRectF>)), SLOT(slotUpdateStatus()));
    connect(m_prevMatch, SIGNAL(clicked()), PdfViewer::searchEngine(), SLOT(previousMatch()));
    connect(m_nextMatch, SIGNAL(clicked()), PdfViewer::searchEngine(), SLOT(nextMatch()));

    connect(PdfViewer::document(), SIGNAL(documentChanged()), SLOT(slotDocumentChanged()));

    slotDocumentChanged();
}

FindBar::~FindBar()
{
}

void FindBar::slotDocumentChanged()
{
    slotHide();
    m_findEdit->clear();

    bool on = PdfViewer::document()->isValid();
    m_findEdit->setEnabled(on);

    // delay this as the search engine might still react to the documentChanged() signal
    QTimer::singleShot(0, this, SLOT(slotUpdateStatus()));
}

void FindBar::slotFind()
{
    PdfViewer::searchEngine()->find(m_findEdit->text());
}

void FindBar::slotHide()
{
    hide();
    PdfViewer::searchEngine()->find(QString());
}

void FindBar::slotFindProgress(qreal progress)
{
    m_findEdit->setStyleSheet(QString("background-color: qlineargradient(x1: %1, y1: 0, x2: %2, y2: 0, stop: 0 %3, stop: 1 %4);").arg(qMax(0.0, progress-0.1)).arg(progress).arg(palette().color(QPalette::Highlight).name()).arg(palette().color(QPalette::Base).name()));
}

void FindBar::slotFindDone()
{
    slotUpdateStatus();

    if (isVisible()) {
        if (PdfViewer::searchEngine()->matches().isEmpty())
            m_findEdit->setStyleSheet("background-color: #f0a0a0");
        else
            m_findEdit->setStyleSheet("background-color: #a0f0a0");

        QTimer::singleShot(5000, this, SLOT(slotResetStyle()));
    }
}

void FindBar::slotResetStyle()
{
    m_findEdit->setStyleSheet(QString());
}

void FindBar::slotUpdateStatus()
{
    int index = PdfViewer::searchEngine()->currentIndex();
    int count = PdfViewer::searchEngine()->matchesCount();

    m_statusLabel->setText(QString("%1 of %2").arg(index).arg(count));
    m_nextMatch->setEnabled(count > 1);
    m_prevMatch->setEnabled(count > 1);
}
