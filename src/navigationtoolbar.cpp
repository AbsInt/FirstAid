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
#include "document.h"
#include "searchengine.h"
#include "viewer.h"

#include <poppler-qt5.h>

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>

NavigationToolBar::NavigationToolBar(QAction *tocAction, QMenu *menu, QWidget *parent)
    : QToolBar(QStringLiteral("Navigation"), parent)
{
    // for state saving
    setObjectName(QStringLiteral("navigation_toolbar"));

    // we stay in place
    setFloatable(false);
    setMovable(false);

    // prepare access to settings
    QSettings s;

    // some shortcuts for first/last page
    QShortcut *firstShortcut = new QShortcut(QKeySequence::MoveToStartOfLine, this);
    connect(firstShortcut, SIGNAL(activated()), this, SLOT(slotGoFirst()));

    QShortcut *lastShortcut = new QShortcut(QKeySequence::MoveToEndOfLine, this);
    connect(lastShortcut, SIGNAL(activated()), this, SLOT(slotGoLast()));

    // left side is table of content action
    tocAction->setIcon(QIcon(QStringLiteral(":/icons/bookmark-new.svg")));
    addAction(tocAction);
    QShortcut *tocShortcut = new QShortcut(Qt::Key_F7, this);
    connect(tocShortcut, SIGNAL(activated()), tocAction, SLOT(trigger()));

    // left side also holds the toggle button for facing pages mode
    m_toggleFacingPagesAct = addAction(QIcon(QStringLiteral(":/icons/facing-pages.svg")), tr("Facing pages"));
    m_toggleFacingPagesAct->setCheckable(true);
    m_toggleFacingPagesAct->setChecked(s.value(QStringLiteral("MainWindow/facingPages"), false).toBool());
    m_toggleFacingPagesAct->setShortcut(Qt::Key_D);
    m_toggleFacingPagesAct->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_toggleFacingPagesAct, SIGNAL(toggled(bool)), SLOT(slotToggleFacingPages()));

    // add some space so next widget group is centered
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    // previous page action
    m_prevAct = addAction(QIcon(QStringLiteral(":/icons/go-previous.svg")), tr("Previous page"), this, SLOT(slotGoPrev()));
    QShortcut *previousShortcut = new QShortcut(QKeySequence::MoveToPreviousPage, this);
    connect(previousShortcut, SIGNAL(activated()), m_prevAct, SLOT(trigger()));

    // combined line edit for goto action and status label
    m_pageEdit = new QLineEdit(this);
    m_pageEdit->setMaxLength(6);
    m_pageEdit->setFixedWidth(50);
    connect(m_pageEdit, SIGNAL(returnPressed()), this, SLOT(slotPageSet()));
    connect(m_pageEdit, SIGNAL(editingFinished()), this, SLOT(slotHideGoto()));
    m_pageEditAct = addWidget(m_pageEdit);

    m_intValidator = new QIntValidator(this);
    m_pageEdit->setValidator(m_intValidator);

    m_pageLabel = new QLabel(this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->installEventFilter(this);
    m_pageLabelAct = addWidget(m_pageLabel);

    // got is accessible by ctrl+g
    QShortcut *gotoShortCut = new QShortcut(Qt::ControlModifier + Qt::Key_G, this);
    connect(gotoShortCut, SIGNAL(activated()), SLOT(slotGoto()));

    // next page action
    m_nextAct = addAction(QIcon(QStringLiteral(":/icons/go-next.svg")), tr("Next page"), this, SLOT(slotGoNext()));
    QShortcut *nextShortcut = new QShortcut(QKeySequence::MoveToNextPage, this);
    connect(nextShortcut, SIGNAL(activated()), m_nextAct, SLOT(trigger()));

    // more space for centering
    spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    // add label and combo box for zooming
    m_zoomLabel = new QLabel(this);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabelAct = addWidget(m_zoomLabel);
    m_zoomLabel->installEventFilter(this);

    m_zoomButton = new QToolButton(this);
    m_zoomButton->setFocusPolicy(Qt::NoFocus);
    m_zoomButton->setToolTip(tr("Zoom"));
    QMenu *zoomMenu = new QMenu(this);
    QAction *a = zoomMenu->addAction(QIcon(QStringLiteral(":/icons/zoom-fit-width.svg")), tr("Fit width"));
    a->setShortcut(Qt::Key_W);
    a->setShortcutContext(Qt::ApplicationShortcut);

    a = zoomMenu->addAction(QIcon(QStringLiteral(":/icons/zoom-fit-best.svg")), tr("Fit page"));
    a->setShortcut(Qt::Key_F);
    a->setShortcutContext(Qt::ApplicationShortcut);

    zoomMenu->addAction(tr("10%"));
    zoomMenu->addAction(tr("25%"));
    zoomMenu->addAction(tr("33%"));
    zoomMenu->addAction(tr("50%"));
    zoomMenu->addAction(tr("66%"));
    zoomMenu->addAction(tr("75%"));

    a = zoomMenu->addAction(tr("100%"));
    a->setShortcut(Qt::ControlModifier + Qt::Key_0);
    a->setShortcutContext(Qt::ApplicationShortcut);

    zoomMenu->addAction(tr("125%"));
    zoomMenu->addAction(tr("150%"));
    zoomMenu->addAction(tr("200%"));
    zoomMenu->addAction(tr("300%"));
    zoomMenu->addAction(tr("400%"));
    m_zoomButton->setMenu(zoomMenu);
    m_zoomButton->setPopupMode(QToolButton::InstantPopup);

    foreach (QAction *a, zoomMenu->actions())
        connect(a, SIGNAL(triggered()), SLOT(slotZoomChanged()));
    addWidget(m_zoomButton);

    // add menu replacement action to right side
    QToolButton *menuButton = new QToolButton();
    menuButton->setToolTip(tr("Application menu"));
    menuButton->setFocusPolicy(Qt::NoFocus);
    menuButton->setIcon(QIcon(QStringLiteral(":/icons/application-menu.svg")));
    menuButton->setMenu(menu);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    addWidget(menuButton);

    // esc to hide goto widgets
    QShortcut *closeGoto = new QShortcut(Qt::Key_Escape, this);
    closeGoto->setContext(Qt::WidgetWithChildrenShortcut);
    connect(closeGoto, SIGNAL(activated()), SLOT(slotHideGoto()));

    // init widgets' state
    m_pageEditAct->setVisible(false);
    m_prevAct->setEnabled(false);
    m_nextAct->setEnabled(false);

    // triggere these slots so view gets redrawn with stored settings
    QTimer::singleShot(0, this, SLOT(slotToggleFacingPages()));

    int index = s.value(QStringLiteral("MainWindow/zoom"), 8).toInt();
    QList<QAction *> zoomActions = zoomMenu->actions();
    if (index >= 0 && index < zoomActions.count())
        QTimer::singleShot(0, zoomActions.at(index), SLOT(trigger()));

    connect(PdfViewer::document(), SIGNAL(documentChanged()), SLOT(slotDocumentChanged()));
    connect(PdfViewer::view(), SIGNAL(pageChanged(int)), SLOT(slotPageChanged(int)));
    connect(PdfViewer::view(), SIGNAL(zoomChanged(qreal)), SLOT(slotChangeZoom(qreal)));

    // init page label
    slotHideGoto();
}

NavigationToolBar::~NavigationToolBar()
{
    // store settings
    QSettings s;
    s.setValue(QStringLiteral("MainWindow/facingPages"), m_toggleFacingPagesAct->isChecked());
}

bool NavigationToolBar::eventFilter(QObject *object, QEvent *event)
{
    // clicking on the status label displaying the current page number will trigger goto action
    if (object == m_pageLabel && QEvent::MouseButtonPress == event->type() && PdfViewer::document()->isValid())
        QTimer::singleShot(0, this, SLOT(slotGoto()));

    // clicking on the zoom label displaying the current scale factor will trigger zoom menu
    if (object == m_zoomLabel && QEvent::MouseButtonPress == event->type() && PdfViewer::document()->isValid())
        QTimer::singleShot(0, m_zoomButton, SLOT(showMenu()));

    return QToolBar::eventFilter(object, event);
}

void NavigationToolBar::slotDocumentChanged()
{
    slotHideGoto();

    bool valid = PdfViewer::document()->isValid();

    if (valid)
        m_intValidator->setRange(1, PdfViewer::document()->numPages());

    m_prevAct->setEnabled(valid);
    m_nextAct->setEnabled(valid);
    m_pageEdit->clear();
    m_pageEdit->setEnabled(valid);
}

void NavigationToolBar::slotPageChanged(int page)
{
    slotHideGoto();

    const int pageCount = PdfViewer::document()->numPages();
    m_prevAct->setEnabled(page > 0);
    m_nextAct->setEnabled(page < (pageCount - 1));
}

void NavigationToolBar::slotGoFirst()
{
    PdfViewer::view()->gotoPage(0);
}

void NavigationToolBar::slotGoPrev()
{
    PdfViewer::view()->gotoPreviousPage();
}

void NavigationToolBar::slotGoNext()
{
    PdfViewer::view()->gotoNextPage();
}

void NavigationToolBar::slotGoLast()
{
    PdfViewer::view()->gotoPage(PdfViewer::document()->numPages() - 1);
}

void NavigationToolBar::slotPageSet()
{
    PdfViewer::view()->gotoPage(m_pageEdit->text().toInt() - 1);
}

void NavigationToolBar::slotGoto()
{
    if (!PdfViewer::document())
        return;

    m_pageLabel->setText(QStringLiteral(" / %2").arg(PdfViewer::document()->numPages()));

    m_pageEditAct->setVisible(true);

    m_pageEdit->setText(QString::number(1 + PdfViewer::view()->currentPage()));
    m_pageEdit->selectAll();
    m_pageEdit->setFocus();
}

void NavigationToolBar::slotHideGoto()
{
    if (PdfViewer::document()->isValid())
        m_pageLabel->setText(QStringLiteral("%1 / %2").arg(1 + PdfViewer::view()->currentPage()).arg(PdfViewer::document()->numPages()));
    else
        m_pageLabel->setText(QStringLiteral("n/a"));

    m_pageEditAct->setVisible(false);
}

void NavigationToolBar::slotZoomChanged()
{
    QAction *a = qobject_cast<QAction *>(sender());
    if (!a)
        return;

    QSettings s;
    s.setValue(QStringLiteral("MainWindow/zoom"), m_zoomButton->menu()->actions().indexOf(a));

    QString text = a->text();

    if (QLatin1String("Fit width") == text) {
        m_zoomLabelAct->setVisible(false);
        m_zoomButton->setIcon(QIcon(QStringLiteral(":/icons/zoom-fit-width.svg")));
        PdfViewer::view()->setZoomMode(PageView::FitWidth);
    } else if (QLatin1String("Fit page") == text) {
        m_zoomLabelAct->setVisible(false);
        m_zoomButton->setIcon(QIcon(QStringLiteral(":/icons/zoom-fit-best.svg")));
        PdfViewer::view()->setZoomMode(PageView::FitPage);
    } else {
        m_zoomLabelAct->setVisible(true);
        m_zoomLabel->setText(text);
        m_zoomButton->setIcon(QIcon(QStringLiteral(":/icons/zoom.svg")));

        text.remove(QLatin1Char('%'));
        bool ok;
        int value = text.toInt(&ok);

        if (ok && value >= 10 && value <= 400)
            PdfViewer::view()->setZoom(qreal(value) / 100);
    }
}

void NavigationToolBar::slotChangeZoom(qreal currentZoom)
{
    m_zoomLabelAct->setVisible(true);
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(currentZoom * 100)));
    m_zoomButton->setIcon(QIcon(QStringLiteral(":/icons/zoom.svg")));

    // save nearest zoom
    QList<QAction *> actions = m_zoomButton->menu()->actions();
    for (int c = 2; c < actions.count(); c++) {
        QString text = actions.at(c)->text();
        text.remove(QLatin1Char('%'));
        bool ok;
        int value = text.toInt(&ok);
        if (ok && currentZoom <= value / 100.0) {
            QSettings s;
            s.setValue(QStringLiteral("MainWindow/zoom"), c);
            break;
        }
    }
}

void NavigationToolBar::slotToggleFacingPages()
{
    PdfViewer::document()->setDoubleSided(m_toggleFacingPagesAct->isChecked());
}
