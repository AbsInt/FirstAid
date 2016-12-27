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

#include <QTimer>



/*
 * defines
 */

#define SearchInterval  100
#define PagePileSize    10



/*
 * class members
 */

SearchEngine *SearchEngine::s_globalInstance=nullptr;



/*
 * constructors / destructor
 */

SearchEngine::SearchEngine()
            : QObject()
{
    m_timer=new QTimer(this);
    m_timer->setInterval(SearchInterval);

    connect(m_timer, SIGNAL(timeout()), SLOT(find()));
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
SearchEngine::reset()
{
    m_timer->stop();
}



void
SearchEngine::setDocument()
{
    reset();
}



/*
 * protected slots
 */
void
SearchEngine::find()
{
}



/*
 * eof
 */
