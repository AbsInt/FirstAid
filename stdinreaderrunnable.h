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

#ifndef STDINREADERRUNNABLE
#define STDINREADERRUNNABLE

#include <QCoreApplication>
#include <QEvent>
#include <QRunnable>
#include <stdio.h>

class StdinReadEvent: public QEvent
{
public:
    StdinReadEvent(const QString &text): QEvent(QEvent::User)
                                       , m_text(text)
    {
    }

    QString text() const { return m_text; }

private:
    QString m_text;
};



class StdinReaderRunnable: public QRunnable
{
public:
    StdinReaderRunnable(QObject *receiver): QRunnable()
                                          , m_receiver(receiver)
    {
    }

    void run()
    {
        char buffer[256];

        while (1) {
            if (buffer == fgets(buffer, 256, stdin))
                QCoreApplication::postEvent(m_receiver, new StdinReadEvent(QString::fromLocal8Bit(buffer)), Qt::NormalEventPriority);
            
        }
    }

private:
    QObject *m_receiver;
};



#endif // #ifndef STDINREADERRUNNABLE
