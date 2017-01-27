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
    label->setStyleSheet("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #000000, stop: 0.3 #505050, stop: 1 #000000);");
    vbl->addWidget(label);

    QStringList html;
    html << "<style>"
         << "table { color: white; }"
         << "cap { font-weight: bold; }"
         << "key { color: yellow; font-weight: bold; }"
         << "</style>"
         << "<table bgcolor=#f0f0f0 width=100%><tr><td align=center><font color=#000000 size=+1><b>FirstAid Shortcuts</b></font></td></tr></table>"
         << "<table cellspacing=5 cellpadding=5><tr>";

    html += addTable("Document Handling");
    html += addShortcut(fromStandardKey(QKeySequence::Open), "Open file");
    html += addShortcut(fromStandardKey(QKeySequence::Refresh), "Reload document");
    html += addShortcut(fromStandardKey(QKeySequence::Find), "Find text in document");
    html += addShortcut(QStringList() << "Crtl"
                                      << "E",
                        "Open in external application");
    html += addShortcut(fromStandardKey(QKeySequence::Print), "Print document");
    html += addShortcut(fromStandardKey(QKeySequence::Quit), "Quit application");
    html += addShortcut(QStringList() << "RightClick" << "Drag", "Copy text area");
    html += addShortcut(QStringList() << "Shift" << "LeftClick" << "Drag", "Copy text area");
    html += endTable();

    html += addTable("Navigation");

    html += addShortcut(fromStandardKey(QKeySequence::MoveToStartOfLine), "Go to first page");
    html += addShortcut(fromStandardKey(QKeySequence::MoveToEndOfLine), "Go to last page");
    html += addShortcut(fromStandardKey(QKeySequence::MoveToNextPage), "Go to next page");
    html += addShortcut(fromStandardKey(QKeySequence::MoveToPreviousPage), "Go to previous page");
    html += addShortcut(QStringList() << "Space", "Scroll to next part of document");
    html += addShortcut(QStringList() << "Backspace", "Scroll to previous part of document");
    html += addShortcut(fromStandardKey(QKeySequence::Back), "Go back in history");
    html += addShortcut(fromStandardKey(QKeySequence::Forward), "Go forward in history");
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
    html += addShortcut(fromStandardKey(QKeySequence::ZoomIn), "Zoom in");
    html += addShortcut(fromStandardKey(QKeySequence::ZoomOut), "Zoom out");
    html += addShortcut(QStringList() << "F7", "Toggle table of contents");
    html += addShortcut(QStringList() << "D", "Toggle double sided mode");
    html += endTable();

    html << "</tr></table>";

    label->setText(html.join(""));
}

QStringList HelpDialog::addTable(const QString &title)
{
    QStringList lines;
    lines << "<td><table cellspacing=2 cellpadding=2>";
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
    quotedKeys.replaceInStrings(QRegExp("(^.+$)"), "<key>\\1</key>");

    return QStringList() << "<tr><td>" + quotedKeys.join(" + ") + "</td><td>" + description + "</td></tr>";
}

QStringList HelpDialog::fromStandardKey(QKeySequence::StandardKey key)
{
    QKeySequence ks(key);
    QStringList alternatives = ks.toString().split(",");
    if (!ks.isEmpty()) {
        QString keys = alternatives.first();
        keys.replace("++", "+&#43;");
        return keys.split("+");
    } else
        return QStringList();
}
