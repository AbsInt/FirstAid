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

#ifndef HISTORYSTACK_H
#define HISTORYSTACK_H

#include <QList>
#include <QString>

class HistoryEntry
{
public:
    enum Type { Unknown, PageWithOffset, Destination };

    HistoryEntry();
    HistoryEntry(int page, int offset = 0);
    HistoryEntry(const QString &destination);

    Type m_type = Unknown;
    int m_page = 0;
    int m_offset = 0;
    QString m_destination;
};

class HistoryStack
{
public:
    HistoryStack();
    ~HistoryStack();

    //! clean history stack
    void clear();

    //! add page with offset to history
    void add(int page, int offset = 0);

    //! add names destination to history
    void add(const QString &destination);

    //! get previous history entry if any
    bool previous(HistoryEntry &entry);

    //! get next history entry if any
    bool next(HistoryEntry &entry);

private:
    //! the stack holding history entries
    QList<HistoryEntry> m_stack;

    //! current index in history stack
    int m_index = -1;
};

#endif // #ifndef HISTORYSTACK_H
