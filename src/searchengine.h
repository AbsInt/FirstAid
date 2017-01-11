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

#pragma once

#include <QHash>
#include <QList>
#include <QObject>

class SearchEngine : public QObject
{
    Q_OBJECT

public:
    SearchEngine();
    ~SearchEngine();

    void currentMatch(int &page, QRectF &match) const;
    QHash<int, QList<QRectF>> matches() const;
    QList<QRectF> matchesFor(int page) const;

    int currentIndex() const;
    int matchesCount() const;

public slots:
    void reset();

    void find(const QString &text);
    void nextMatch();
    void previousMatch();

signals:
    void started();
    void progress(qreal progress);
    void finished();
    void highlightMatch(int page, const QRectF &match);
    void matchesFound(int page, const QList<QRectF> &matches);

protected slots:
    void find();

private:
    // members for finding text
    QString m_findText;
    int m_findCurrentPage = 0;
    int m_findStopAfterPage = 0;
    int m_findPagesScanned = 0;

    // members for navigating in find results
    QHash<int, QList<QRectF>> m_matchesForPage;
    int m_currentMatchPage = 0;
    int m_currentMatchPageIndex = 0;
    int m_currentMatchIndex = 0;
};
