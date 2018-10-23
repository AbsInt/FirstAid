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
#include <QRegularExpression>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent, Qt::Popup)
{
    QVBoxLayout *vbl = new QVBoxLayout(this);
    vbl->setContentsMargins(0, 0, 0, 0);

    QLabel *label = new QLabel(this);
    label->setStyleSheet(QStringLiteral("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #000000, stop: 0.3 #505050, stop: 1 #000000);"));
    vbl->addWidget(label);

    QStringList html;
    html << QStringLiteral("<style>") << QStringLiteral("table { color: white; }") << QStringLiteral("cap { font-weight: bold; }") << QStringLiteral("key { color: yellow; font-weight: bold; }") << QStringLiteral("</style>")
         << QStringLiteral("<table bgcolor=#f0f0f0 width=100%><tr><td align=center><font color=#000000 size=+1><b>FirstAid Shortcuts</b></font></td></tr></table>") << QStringLiteral("<table cellspacing=5 cellpadding=5><tr>");

    html += addTable(QStringLiteral("Document Handling"));
    html += addShortcut(fromStandardKey(QKeySequence::Open), tr("Open file"));
    html += addShortcut(fromStandardKey(QKeySequence::Refresh), tr("Reload document"));
    html += addShortcut(fromStandardKey(QKeySequence::Find), tr("Find text in document"));
    html += addShortcut(QStringList() << QStringLiteral("Crtl") << QStringLiteral("E"), tr("Open in external application"));
    html += addShortcut(fromStandardKey(QKeySequence::Print), tr("Print document"));
#if defined(Q_OS_WIN)
    html += addShortcut(QStringList() << QStringLiteral("Alt") << QStringLiteral("F4"), tr("Quit application"));
#else
    html += addShortcut(fromStandardKey(QKeySequence::Quit), tr("Quit application"));
#endif
    html += addShortcut(QStringList() << QStringLiteral("RightClick") << QStringLiteral("Drag"), tr("Copy text area"));
    html += addShortcut(QStringList() << QStringLiteral("Shift") << QStringLiteral("LeftClick") << QStringLiteral("Drag"), tr("Copy text area"));
    html += endTable();

    html += addTable(QStringLiteral("Navigation"));

    html += addShortcut(fromStandardKey(QKeySequence::MoveToStartOfLine), tr("Go to first page"));
    html += addShortcut(fromStandardKey(QKeySequence::MoveToEndOfLine), tr("Go to last page"));
    html += addShortcut(fromStandardKey(QKeySequence::MoveToNextPage), tr("Go to next page"));
    html += addShortcut(fromStandardKey(QKeySequence::MoveToPreviousPage), tr("Go to previous page"));
    html += addShortcut(QStringList() << QStringLiteral("Space"), tr("Scroll to next part of document"));
    html += addShortcut(QStringList() << QStringLiteral("Backspace"), tr("Scroll to previous part of document"));
    html += addShortcut(fromStandardKey(QKeySequence::Back), tr("Go back in history"));
    html += addShortcut(fromStandardKey(QKeySequence::Forward), tr("Go forward in history"));
    html += addShortcut(QStringList() << QStringLiteral("Ctrl") << QStringLiteral("G"), tr("Go to page"));
    html += addShortcut(QStringList() << QStringLiteral("F3"), tr("Next match"));
    html += addShortcut(QStringList() << QStringLiteral("Shift") << QStringLiteral("F3"), tr("Previous match"));
    html += endTable();

    html += addTable(QStringLiteral("View"));
    html += addShortcut(QStringList() << QStringLiteral("W"), tr("Fit page width"));
    html += addShortcut(QStringList() << QStringLiteral("F"), tr("Fit full page"));
    html += addShortcut(QStringList() << QStringLiteral("Crtl") << QStringLiteral("0"), tr("Zoom 100%"));
    html += addShortcut(fromStandardKey(QKeySequence::ZoomIn), tr("Zoom in"));
    html += addShortcut(fromStandardKey(QKeySequence::ZoomOut), tr("Zoom out"));
    html += addShortcut(QStringList() << QStringLiteral("F7"), tr("Toggle table of contents"));
    html += addShortcut(QStringList() << QStringLiteral("D"), tr("Toggle double sided mode"));
    html += endTable();

    html << QStringLiteral("</tr></table>");

    label->setText(html.join(QString()));
}

QStringList HelpDialog::addTable(const QString &title)
{
    QStringList lines;
    lines << QStringLiteral("<td><table cellspacing=2 cellpadding=2>");
    lines << QStringLiteral("<tr><td colspan=2 align=center border-style=solid border-width=1 border-color=white><cap>") + title + QStringLiteral("</cap></td></tr>");
    return lines;
}

QStringList HelpDialog::endTable()
{
    QStringList lines;
    lines << QStringLiteral("<tr></tr>");
    lines << QStringLiteral("</table></td>");
    return lines;
}

QStringList HelpDialog::addShortcut(const QStringList &keys, const QString &description)
{
    QStringList quotedKeys = keys;
    quotedKeys.replaceInStrings(QRegularExpression(QStringLiteral("(^.+$)")), QStringLiteral("<key>\\1</key>"));

    return QStringList() << QStringLiteral("<tr><td>") + quotedKeys.join(QLatin1String(" + ")) + QStringLiteral("</td><td>") + description + QStringLiteral("</td></tr>");
}

QStringList HelpDialog::fromStandardKey(QKeySequence::StandardKey key)
{
    QKeySequence ks(key);
    QStringList alternatives = ks.toString().split(QLatin1Char(','));
    if (!ks.isEmpty()) {
        QString keys = alternatives.first();
        keys.replace(QLatin1String("++"), QLatin1String("+&#43;"));
        return keys.split(QLatin1Char('+'));
    } else
        return QStringList();
}
