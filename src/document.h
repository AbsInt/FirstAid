/*
 * Copyright (C) 2016, Jan Pohland <pohland@absint.com>
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

#include <poppler-qt5.h>

#include <QObject>

class Document : public QObject
{
    Q_OBJECT

public:
    Document();
    ~Document();

    void setDocument(const QString & fileName,QString *errorMessage);

    int numPages() {return m_pages.size();}
    Poppler::Page* page(int page);
    const QDomDocument *toc();
    Poppler::LinkDestination *linkDestination(const QString &destination);
    const QList<Poppler::Annotation *> &annotations(int page);

    bool isValid() {return m_document!=nullptr;}

    void reset();

    QString title();

private:
    /**
     * current open file
     */
    QString m_fileName;

    /**
     * current open poppler document
     */
    Poppler::Document *m_document = nullptr;

    /**
     * list of cached poppler pages, index == page
     */
    QList<Poppler::Page*> m_pages;

    /**
     * list of cached poppler annotations, index == page
     */
    QList<QList<Poppler::Annotation *>> m_annotations;
};
