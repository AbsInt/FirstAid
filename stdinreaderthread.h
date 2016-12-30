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

#ifndef STDINREADERTHREAD_H
#define STDINREADERTHREAD_H

#include <QMetaObject>
#include <QThread>
#include <iostream>

class StdinReaderThread : public QThread
{
public:
    StdinReaderThread(QObject *parent)
        : QThread()
        , m_receiver(parent)
    {
    }

    void run()
    {
        /**
         * read until first error or closed stdin
         */
        while (std::cin) {
            /**
             * read line => queue it to main thread as command
             */
            std::string line;
            std::getline (std::cin, line);
            if (!line.empty())
                QMetaObject::invokeMethod(m_receiver, "processCommand", Qt::QueuedConnection, Q_ARG(QString, QString::fromLocal8Bit(line.c_str()).trimmed()));
        }

        /**
         * if we leave this, force close in any case
         * if we already are closing down, nothing will happen
         */
        QMetaObject::invokeMethod(m_receiver, "processCommand", Qt::QueuedConnection, Q_ARG(QString, QString("close")));
    }

private:
    QObject * const m_receiver;
};

#endif // #ifndef STDINREADERTHREAD_H
