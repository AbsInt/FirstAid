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

#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent, Qt::Popup)
{
    QVBoxLayout *vbl = new QVBoxLayout(this);
    vbl->setContentsMargins(0, 0, 0, 0);

    QLabel *label = new QLabel(this);
    vbl->addWidget(label);

    QStringList html;
    html << "<style>"
         << "table { color: white; }"
         << "cap { font-weight: bold; }"
         << "key { color: yellow; font-weight: bold; }"
         << "</style>"
         << "<h3 align=center>FirstAid Shortcuts</h3>"
         << "<table bgcolor=#000000 cellspacing=5 cellpadding=5><tr>";

    html += addTable("Navigation");

    html += addShortcut(QStringList() << "PageUp", "Go to next page");
    html += addShortcut(QStringList() << "PageDown", "Go to previous page");
    html += addShortcut(QStringList() << "Space", "Scroll to next part of document");
    html += addShortcut(QStringList() << "Backspace", "Scroll to previous part of document");
    html += addShortcut(QStringList() << "Alt"
                                      << "Left",
                        "Go back in history");
    html += addShortcut(QStringList() << "Alt"
                                      << "Right",
                        "Go forward in history");
    html += addShortcut(QStringList() << "Ctrl"
                                      << "G",
                        "Go to page");
    html += endTable();

    html += addTable("View");
    html += addShortcut(QStringList() << "W", "Fit page width");
    html += addShortcut(QStringList() << "F", "Fit full page");
    html += addShortcut(QStringList() << "Crtl"
                                      << "0",
                        "Zoom 100%");
    html += addShortcut(QStringList() << "Crtl"
                                      << "+",
                        "Zoom in");
    html += addShortcut(QStringList() << "Crtl"
                                      << "-",
                        "Zoom out");
    html += addShortcut(QStringList() << "F7", "Toggle table of contents");
    html += addShortcut(QStringList() << "D", "Toggle double sided mode");
    html += endTable();

    html += addTable("Document Handling");
    html += addShortcut(QStringList() << "Ctrl"
                                      << "O",
                        "Open file");
    html += addShortcut(QStringList() << "F5", "Reload document");
    html += addShortcut(QStringList() << "Crtl"
                                      << "F",
                        "Find text in document");
    html += addShortcut(QStringList() << "Crtl"
                                      << "E",
                        "Open document in external application");
    html += addShortcut(QStringList() << "Crtl"
                                      << "P",
                        "Print document");
    html += addShortcut(QStringList() << "Crtl"
                                      << "Q",
                        "Quit application");
    html += endTable();

    html << "</tr></table>";

    label->setText(html.join(""));
}

QStringList HelpDialog::addTable(const QString &title)
{
    QStringList lines;
    lines << "<td><table>";
    lines << "<tr><td colspan=2 align=center border-style=solid border-width=1 border-color=white><cap>" + title + "</cap></td></tr>";
    return lines;
}

QStringList HelpDialog::endTable()
{
    QStringList lines;
    lines << "<tr></tr>";
    lines << "</table></td>";
    return lines;
}

QStringList HelpDialog::addShortcut(const QStringList &keys, const QString &description)
{
    QStringList quotedKeys = keys;
    quotedKeys.replaceInStrings(QRegExp("(.*)"), "<key>\\1</key>");

    return QStringList() << "<tr><td>" + quotedKeys.join("&nbsp;+&nbsp;") + "</td><td>" + description + "</td></tr>";
}
