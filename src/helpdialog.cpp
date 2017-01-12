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

#include "helpdialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    QVBoxLayout *vbl = new QVBoxLayout(this);

    QTextBrowser *tb = new QTextBrowser(this);
    vbl->addWidget(tb);

    QDialogButtonBox *dbb = new QDialogButtonBox(this);
    vbl->addWidget(dbb);

    QPushButton *button = dbb->addButton(QDialogButtonBox::Close);
    connect(button, SIGNAL(clicked()), SLOT(close()));

    tb->setHtml("<table><tr>"
                "<td><table>"
                "<tr><td colspan=2 align=center padding=2px>Navigating</td></tr>"
                "<tr><td>PageUp</td><td>Go to next page</td></tr>"
                "<tr><td>PageDown</td><td>Go to previous page</td></tr>"
                "<tr><td>Space</td><td>Scroll to next part of document</td></tr>"
                "<tr><td>Backspace</td><td>Scroll to previous part of document</td></tr>"
                "<tr><td>Alt + Left</td><td>Go back in history</td></tr>"
                "<tr><td>Alt + Right</td><td>Go forward in history</td></tr>"
                "<tr><td>Ctrl + G</td><td>Go to page</td></tr>"
                "<tr></tr>"
                "</table></td>"
                "<td><table>"
                "<tr><td colspan=2 align=center padding=2px>View</td></tr>"
                "<tr><td>W</td><td>Fit page width</td></tr>"
                "<tr><td>F</td><td>Fit full page</td></tr>"
                "<tr><td>Crtl + 0</td><td>Zoom 100%</td></tr>"
                "<tr><td>Crtl + +</td><td>Zoom in</td></tr>"
                "<tr><td>Crtl + -</td><td>Zoom out</td></tr>"
                "<tr><td>F7</td><td>Toggle table of contents</td></tr>"
                "<tr><td>D </td><td>Toggle double sided mode</td></tr>"
                "</table></td>"
                "</tr><tr>"
                "<td><table>"
                "<tr><td colspan=2 align=center padding=2px>Document Handling</td></tr>"
                "<tr><td>Ctrl + O</td><td>Open file</td></tr>"
                "<tr><td>F5</td><td>Reload document</td></tr>"
                "<tr><td>Crtl + F</td><td>Find text in document</td></tr>"
                "<tr><td>Crtl + E</td><td>Open document in external application</td></tr>"
                "<tr><td>Crtl + P</td><td>Print document</td></tr>"
                "<tr><td>Crtl + Q</td><td>Quit application</td></tr>"
                "</table></td>"
                "</tr></table>");
}
