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

/*
 * includes
 */

#include "searchengine.h"

#include <poppler-qt5.h>

#include <QTimer>



/*
 * defines
 */

#define SearchInterval  10
#define PagePileSize    10



/*
 * class members
 */

SearchEngine *SearchEngine::s_globalInstance=nullptr;



/*
 * constructors / destructor
 */

SearchEngine::SearchEngine()
            : DocumentObserver()
{
    m_timer=new QTimer(this);
    m_timer->setInterval(SearchInterval);
    m_timer->setSingleShot(true);

    connect(m_timer, SIGNAL(timeout()), SLOT(find()));

    reset();
}



SearchEngine::~SearchEngine()
{
}



/*
 * public methods
 */

SearchEngine *
SearchEngine::globalInstance()
{
    if (!s_globalInstance)
        s_globalInstance=new SearchEngine();

    return s_globalInstance;
}



void
SearchEngine::destroy()
{
    if (s_globalInstance) {
        delete s_globalInstance;
        s_globalInstance=nullptr;
    }
}



void
SearchEngine::documentLoaded()
{
    reset();
}



void
SearchEngine::documentClosed()
{
    reset();
}



void
SearchEngine::pageChanged(int page)
{
    Q_UNUSED(page)
}



/*
 * public slots
 */

void
SearchEngine::reset()
{
    m_timer->stop();

    m_matchesForPage.clear();
    m_currentMatchPage=0;
    m_currentMatchIndex=0;

    m_findText.clear();
}



void
SearchEngine::find(const QString &text)
{
    m_timer->stop();

    if (text.isEmpty())
        return;

    if (text == m_findText) {
        nextMatch();
        return;
    }

    m_findText=text;

    m_findCurrentPage=page();

    m_findStopAfterPage=m_findCurrentPage-1;
    if (m_findStopAfterPage < 0)
        m_findStopAfterPage=document()->numPages()-1;

    m_matchesForPage.clear();
    
    m_timer->start();
}



void
SearchEngine::nextMatch()
{
    if (-1==m_currentMatchPage || m_matchesForPage.isEmpty())
        return;

    // more matches on current match page?
    QList<QRectF> matches=m_matchesForPage.value(m_currentMatchPage);
    if (m_currentMatchIndex < matches.count()-1) {
        m_currentMatchIndex++;
        emit highlightMatch(m_currentMatchPage, matches.at(m_currentMatchIndex));
        return;
    }

    // find next match
    for (int page=m_currentMatchPage+1; page<document()->numPages(); page++) {
        QList<QRectF> matches=m_matchesForPage.value(page);
        if (!matches.isEmpty()) {
            m_currentMatchPage=page;
            m_currentMatchIndex=0;
            emit highlightMatch(m_currentMatchPage, matches.first());
            return;
        }
    }

    for (int page=0; page<m_currentMatchPage; page++) {
        QList<QRectF> matches=m_matchesForPage.value(page);
        if (!matches.isEmpty()) {
            m_currentMatchPage=page;
            m_currentMatchIndex=0;
            emit highlightMatch(m_currentMatchPage, matches.first());
            return;
        }
    }
}



void
SearchEngine::previousMatch()
{
}



/*
 * private slots
 */

void
SearchEngine::find()
{
    // find our text on the current search page
    Poppler::Page *p=document()->page(m_findCurrentPage);
    QList<QRectF> matches=p->search(m_findText, Poppler::Page::IgnoreCase);
    delete p;

    // signal matches and remember them
    if (!matches.isEmpty()) {
        if (m_matchesForPage.isEmpty()) {
            m_currentMatchPage=m_findCurrentPage;
            m_currentMatchIndex=0;
            emit highlightMatch(m_currentMatchPage, matches.first());
        }

        m_matchesForPage.insert(m_findCurrentPage, matches);

        emit matchesFound(m_findCurrentPage, matches);
    }

    // are we done with our search
    if (m_findCurrentPage == m_findStopAfterPage)
        return;

    // no? proceed with next page or wrap around
    m_findCurrentPage++;
    if (m_findCurrentPage >= document()->numPages())
        m_findCurrentPage=0;

    m_timer->start();

}



/*
 * eof
 */
