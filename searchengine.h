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

#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include "documentobserver.h"

#include <QHash>
#include <QList>
#include <QObject>

class SearchEngine : public QObject, public DocumentObserver
{
    Q_OBJECT

    SearchEngine();
    ~SearchEngine();

public:
    static SearchEngine *globalInstance();
    static void destroy();

    void documentLoaded();
    void documentClosed();
    void pageChanged(int page);

    void reset();

    void currentMatch(int &page, QRectF &match) const;
    QHash<int, QList<QRectF>> matches() const;
    QList<QRectF> matchesFor(int page) const;

public slots:
    void find(const QString &text);
    void nextMatch();
    void previousMatch();

signals:
    void started();
    void finished();
    void highlightMatch(int page, const QRectF &match);
    void matchesFound(int page, const QList<QRectF> &matches);

protected slots:
    void find();

private:
    static SearchEngine *s_globalInstance;

    // members for finding text
    QString m_findText;
    int m_findCurrentPage;
    int m_findStopAfterPage;

    // members for navigating in find results
    QHash<int, QList<QRectF>> m_matchesForPage;
    int m_currentMatchPage;
    int m_currentMatchIndex;
};

#endif // #ifndef SEARCHENGINE_H
