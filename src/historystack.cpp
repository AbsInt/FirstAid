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

#include "historystack.h"

/*
 * helper class
 */

HistoryEntry::HistoryEntry()
    : m_type(Unknown)
{
}

HistoryEntry::HistoryEntry(int page, const QRectF &rect)
    : m_type(PageWithRect)
    , m_page(page)
    , m_rect(rect)
{
}

HistoryEntry::HistoryEntry(const QString &destination)
    : m_type(Destination)
    , m_destination(destination)
{
}

/*
 * constructors / destructor
 */

HistoryStack::HistoryStack()
{
}

HistoryStack::~HistoryStack()
{
}

/*
 * public methods
 */

void HistoryStack::clear()
{
    m_stack.clear();
    m_index = -1;
}

void HistoryStack::add(int page, const QRectF &rect)
{
    // pop entries after current index
    while (m_index >= 0 && m_index + 1 < m_stack.count())
        m_stack.takeLast();

    // add new location
    m_stack << HistoryEntry(page, rect);
    m_index = m_stack.count() - 1;
}

void HistoryStack::add(const QString &destination)
{
    // pop entries after current index
    while (m_index >= 0 && m_index + 1 < m_stack.count())
        m_stack.takeLast();

    // add new location
    m_stack << HistoryEntry(destination);
    m_index = m_stack.count() - 1;
}

bool HistoryStack::HistoryStack::previous(HistoryEntry &entry)
{
    if (m_index > 0) {
        m_index--;
        entry = m_stack.at(m_index);
        return true;
    } else
        return false;
}

bool HistoryStack::HistoryStack::next(HistoryEntry &entry)
{
    if (m_index < m_stack.count() - 1) {
        m_index++;
        entry = m_stack.at(m_index);
        return true;
    } else
        return false;
}

/*
 * eof
 */
