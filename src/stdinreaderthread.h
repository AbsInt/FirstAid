/*
 * Copyright (C) 2016, Marc Langenbach <mlangen@absint.com>
 * Copyright (C) 2016, Christoph Cullmann <cullmann@absint.com>
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

#include <QMetaObject>
#include <QThread>
#include <iostream>

/**
 * Thread to read commands from stdin and pass them on via QueuedConnection
 * to given object (e.g. Viewer in main thread).
 */
class StdinReaderThread : public QThread
{
public:
    /**
     * Construct reader, pass object that should receive the commands
     * @param receiver receiver of commands read from stdin
     */
    StdinReaderThread(QObject *receiver)
        : QThread()
        , m_receiver(receiver)
    {
    }

    /**
     * Thread main function, endless stdin reading, until that is closed.
     * TODO: interruption handling
     */
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
            std::getline(std::cin, line);
            if (!line.empty())
                QMetaObject::invokeMethod(m_receiver, "processCommand", Qt::QueuedConnection, Q_ARG(QString, QString::fromLocal8Bit(line.c_str()).trimmed()));
        }

        /**
         * if we leave this, force close in any case
         * if we already are closing down, nothing will happen
         */
        QMetaObject::invokeMethod(m_receiver, "processCommand", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("close")));
    }

private:
    /**
     * receiver for commands
     */
    QObject *const m_receiver;
};
