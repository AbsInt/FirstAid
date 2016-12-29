/*
 * Copyright (C) 2008-2009, Pino Toscano <pino@kde.org>
 * Copyright (C) 2013, Fabio D'Urso <fabiodurso@hotmail.it>
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

#include "navigationtoolbar.h"
#include "searchengine.h"

#include <poppler-qt5.h>

#include <QtGui/QIntValidator>
#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QShortcut>

NavigationToolBar::NavigationToolBar(QAction *tocAction, QWidget *parent)
                 : QToolBar("Navigation", parent)
{
    setFloatable(false);
    setMovable(false);

    // some shortcuts for first/last page
    QShortcut *firstShortcut=new QShortcut(Qt::Key_Home, this);
    connect(firstShortcut, SIGNAL(activated()), this, SLOT(slotGoFirst()));

    QShortcut *lastShortcut=new QShortcut(Qt::Key_End, this);
    connect(lastShortcut, SIGNAL(activated()), this, SLOT(slotGoLast()));

    // widget on toolbar
    tocAction->setIcon(QIcon(":/icons/bookmark-new.png"));
    addAction(tocAction);
    QShortcut *tocShortcut=new QShortcut(Qt::Key_F7, this);
    connect(tocShortcut, SIGNAL(activated()), tocAction, SLOT(trigger()));

    QAction *toggleContinous=addAction(QIcon(":/icons/zoom-select-y.png"), tr("Toggle continous mode"));
    toggleContinous->setCheckable(true);
    connect(toggleContinous, SIGNAL(toggled(bool)), SIGNAL(toggleContinous(bool)));

    QWidget *spacer=new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    addWidget(spacer);

    m_prevAct = addAction(QIcon(":/icons/go-previous.png"), tr("Previous"), this, SLOT(slotGoPrev()));
    QShortcut *previousShortcut=new QShortcut(Qt::Key_Backspace, this);
    connect(previousShortcut, SIGNAL(activated()), m_prevAct, SLOT(trigger()));

    m_pageEdit = new QLineEdit(this);
    m_pageEdit->setMaxLength(6);
    m_pageEdit->setFixedWidth(50);
    connect(m_pageEdit, SIGNAL(returnPressed()), this, SLOT(slotPageSet()));
    addWidget(m_pageEdit);

    QShortcut *gotoShortCut=new QShortcut(Qt::ControlModifier+Qt::Key_G, this);
    connect(gotoShortCut, SIGNAL(activated()), m_pageEdit, SLOT(setFocus()));
    connect(gotoShortCut, SIGNAL(activated()), m_pageEdit, SLOT(selectAll()));

    m_intValidator=new QIntValidator(this);
    m_pageEdit->setValidator(m_intValidator);

    m_pageLabel = new QLabel(this);
    addWidget(m_pageLabel);

    m_nextAct = addAction(QIcon(":/icons/go-next.png"), tr("Next"), this, SLOT(slotGoNext()));
    QShortcut *nextShortcut=new QShortcut(Qt::Key_Space, this);
    connect(nextShortcut, SIGNAL(activated()), m_nextAct, SLOT(trigger()));

    spacer=new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    addWidget(spacer);

    m_zoomCombo = new QComboBox(this);
    m_zoomCombo->setInsertPolicy(QComboBox::NoInsert);
    m_zoomCombo->setEditable(true);
    m_zoomCombo->addItem(tr("Fit width"));
    m_zoomCombo->addItem(tr("Fit page"));
    m_zoomCombo->addItem(tr("10%"));
    m_zoomCombo->addItem(tr("25%"));
    m_zoomCombo->addItem(tr("33%"));
    m_zoomCombo->addItem(tr("50%"));
    m_zoomCombo->addItem(tr("66%"));
    m_zoomCombo->addItem(tr("75%"));
    m_zoomCombo->addItem(tr("100%"));
    m_zoomCombo->addItem(tr("125%"));
    m_zoomCombo->addItem(tr("150%"));
    m_zoomCombo->addItem(tr("200%"));
    m_zoomCombo->addItem(tr("300%"));
    m_zoomCombo->addItem(tr("400%"));
    m_zoomCombo->setCurrentIndex(8); // "100%"
    connect(m_zoomCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotZoomComboChanged()));
    connect(m_zoomCombo->lineEdit(), SIGNAL(returnPressed()), this, SLOT(slotZoomComboChanged()));
    addWidget(m_zoomCombo);

    documentClosed();
}

NavigationToolBar::~NavigationToolBar()
{
}

void NavigationToolBar::documentLoaded()
{
    const int pageCount = document()->numPages();

    m_pageLabel->setText(QString(" / %1").arg(pageCount));

    m_pageEdit->setText("1");
    m_pageEdit->setEnabled(true);
    m_pageEdit->selectAll();

    m_intValidator->setRange(1, 1+pageCount);
}

void NavigationToolBar::documentClosed()
{
    m_prevAct->setEnabled(false);
    m_nextAct->setEnabled(false);
    m_pageEdit->clear();
    m_pageEdit->setEnabled(false);
}

void NavigationToolBar::pageChanged(int page)
{
    const int pageCount = document()->numPages();
    m_prevAct->setEnabled(page > 0);
    m_nextAct->setEnabled(page < (pageCount - 1));
    m_pageEdit->setText(QString::number(page+1));
    m_pageEdit->selectAll();
}

void NavigationToolBar::slotGoFirst()
{
    setPage(0);
}

void NavigationToolBar::slotGoPrev()
{
    setPage(page() - 1);
}

void NavigationToolBar::slotGoNext()
{
    setPage(page() + 1);
}

void NavigationToolBar::slotGoLast()
{
    setPage(document()->numPages() - 1);
}

void NavigationToolBar::slotPageSet()
{
    setPage(m_pageEdit->text().toInt()-1);
}

void NavigationToolBar::slotZoomComboChanged()
{
    QString text=m_zoomCombo->currentText();

    if ("Fit width" == text)
        emit zoomModeChanged(PageView::FitWidth);
    else if ("Fit page" == text)
        emit zoomModeChanged(PageView::FitPage);
    else {
        text.remove("%");
        bool ok;
        int value=text.toInt(&ok);

        if (ok && value>=10 && value <=400)
            emit zoomChanged(qreal(value)/100);
    }
}

#include "navigationtoolbar.moc"
