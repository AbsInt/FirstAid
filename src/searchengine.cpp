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
#include "viewer.h"

#include <QGuiApplication>
#include <QTimer>

/*
 * defines
 */

#define PagePileSize 20

/*
 * constructors / destructor
 */

SearchEngine::SearchEngine()
    : QObject()
{
    reset();
}

SearchEngine::~SearchEngine()
{
}

/*
 * public methods
 */

void SearchEngine::currentMatch(int &page, QRectF &match) const
{
    if (m_matchesForPage.isEmpty()) {
        page = 0;
        match = QRectF();
    } else {
        page = m_currentMatchPage;
        match = m_matchesForPage.value(m_currentMatchPage).at(m_currentMatchPageIndex);
    }
}

QHash<int, QList<QRectF>> SearchEngine::matches() const
{
    return m_matchesForPage;
}

QList<QRectF> SearchEngine::matchesFor(int page) const
{
    return m_matchesForPage.value(page);
}

int SearchEngine::currentIndex() const
{
    return m_currentMatchIndex;
}

int SearchEngine::matchesCount() const
{
    // count matches
    int count = 0;
    QList<QRectF> matches;
    foreach (matches, m_matchesForPage)
        count += matches.count();

    return count;
}

/*
 * public slots
 */

void SearchEngine::reset()
{
    m_matchesForPage.clear();
    m_currentMatchPage = 0;
    m_currentMatchPageIndex = 0;
    m_currentMatchIndex = 0;

    m_findText.clear();
}

void SearchEngine::find(const QString &text, bool caseSensitive, bool wholeWords)
{
    // compose flags first
    int flags = 0;
    if (!caseSensitive)
        flags |= Poppler::Page::IgnoreCase;
    if (wholeWords)
        flags |= Poppler::Page::WholeWords;

    m_matchesForPage.clear();
    m_currentMatchPage = 0;
    m_currentMatchPageIndex = 0;
    m_currentMatchIndex = 0;

    emit started();

    m_findText = text;
    m_findFlags = flags;

    if (text.isEmpty()) {
        emit finished();
        return;
    }

    m_findCurrentPage = PdfViewer::view()->currentPage();
    m_findPagesScanned = 0;

    m_findStopAfterPage = m_findCurrentPage - 1;
    if (m_findStopAfterPage < 0)
        m_findStopAfterPage = PdfViewer::document()->numPages() - 1;

    find();
}

void SearchEngine::nextMatch()
{
    if (-1 == m_currentMatchPage || m_matchesForPage.isEmpty())
        return;

    m_currentMatchIndex++;
    if (m_currentMatchIndex > matchesCount())
        m_currentMatchIndex = 1;

    // more matches on current match page?
    QList<QRectF> matches = m_matchesForPage.value(m_currentMatchPage);
    if (m_currentMatchPageIndex < matches.count() - 1) {
        m_currentMatchPageIndex++;
        emit highlightMatch(m_currentMatchPage, matches.at(m_currentMatchPageIndex));
        return;
    }

    // find next match
    for (int page = m_currentMatchPage + 1; page < PdfViewer::document()->numPages(); page++) {
        QList<QRectF> matches = m_matchesForPage.value(page);
        if (!matches.isEmpty()) {
            m_currentMatchPage = page;
            m_currentMatchPageIndex = 0;
            emit highlightMatch(m_currentMatchPage, matches.first());
            return;
        }
    }

    for (int page = 0; page <= m_currentMatchPage; page++) {
        QList<QRectF> matches = m_matchesForPage.value(page);
        if (!matches.isEmpty()) {
            m_currentMatchPage = page;
            m_currentMatchPageIndex = 0;
            emit highlightMatch(m_currentMatchPage, matches.first());
            return;
        }
    }
}

void SearchEngine::previousMatch()
{
    if (-1 == m_currentMatchPage || m_matchesForPage.isEmpty())
        return;

    m_currentMatchIndex--;
    if (m_currentMatchIndex < 1)
        m_currentMatchIndex = matchesCount();

    // more matches on current match page?
    QList<QRectF> matches = m_matchesForPage.value(m_currentMatchPage);
    if (m_currentMatchPageIndex > 0) {
        m_currentMatchPageIndex--;
        emit highlightMatch(m_currentMatchPage, matches.at(m_currentMatchPageIndex));
        return;
    }

    // find previous match
    for (int page = m_currentMatchPage - 1; page >= 0; page--) {
        QList<QRectF> matches = m_matchesForPage.value(page);
        if (!matches.isEmpty()) {
            m_currentMatchPage = page;
            m_currentMatchPageIndex = matches.count() - 1;
            emit highlightMatch(m_currentMatchPage, matches.last());
            return;
        }
    }

    for (int page = PdfViewer::document()->numPages() - 1; page >= m_currentMatchPage; page--) {
        QList<QRectF> matches = m_matchesForPage.value(page);
        if (!matches.isEmpty()) {
            m_currentMatchPage = page;
            m_currentMatchPageIndex = matches.count() - 1;
            emit highlightMatch(m_currentMatchPage, matches.last());
            return;
        }
    }
}

/*
 * private slots
 */

void SearchEngine::find()
{
    if (m_findText.isEmpty())
        return;

    bool delayAfterFirstMatch = false;

    for (int count = 0; count < PagePileSize; count++) {
        // find our text on the current search page
        Poppler::Page *p = PdfViewer::document()->page(m_findCurrentPage);
        QList<QRectF> matches = p->search(m_findText, static_cast<Poppler::Page::SearchFlag>(m_findFlags));

        // signal matches and remember them
        if (!matches.isEmpty()) {
            if (m_matchesForPage.isEmpty()) {
                m_currentMatchPage = m_findCurrentPage;
                m_currentMatchPageIndex = 0;
                m_currentMatchIndex = 1;
                emit highlightMatch(m_currentMatchPage, matches.first());
                delayAfterFirstMatch = true;
            }

            m_matchesForPage.insert(m_findCurrentPage, matches);

            emit matchesFound(m_findCurrentPage, matches);
        }

        // are we done with our search
        if (m_findCurrentPage == m_findStopAfterPage) {
            emit finished();
            return;
        }

        // no? proceed with next page or wrap around
        m_findCurrentPage++;
        if (m_findCurrentPage >= PdfViewer::document()->numPages())
            m_findCurrentPage = 0;

        if (delayAfterFirstMatch)
            break;

        // emit progress
        m_findPagesScanned++;
        emit progress(m_findPagesScanned / (qreal)PdfViewer::document()->numPages());
    }

    QTimer::singleShot(delayAfterFirstMatch ? 10 : 0, this, SLOT(find()));
}

/*
 * eof
 */
