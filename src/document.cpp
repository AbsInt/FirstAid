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

#include "document.h"


Document::Document()
{

}

Document::~Document()
{
    reset();
}

void Document::setDocument(const QString &fileName, QString *errorMessage)
{
    // reset old content
    reset();

    m_fileName = fileName;
    if (errorMessage)
        *errorMessage = QString();

    Poppler::Document *newdoc = Poppler::Document::load(fileName);
    if (!newdoc || newdoc->isLocked()) {
        *errorMessage = QString("Cannot open file '%1'.").arg(fileName);
        if (newdoc)
            delete newdoc;
        return;
    }

    m_document = newdoc;

    m_document->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_document->setRenderHint(Poppler::Document::Antialiasing, true);
    m_document->setRenderBackend(Poppler::Document::SplashBackend);

    m_pages.reserve(newdoc->numPages());
    m_annotations.reserve(newdoc->numPages());
    for (int i = 0; i < newdoc->numPages(); ++i)
    {
        Poppler::Page *page = newdoc->page(i);
        m_pages.append(page);
        m_annotations.append(page->annotations(QSet<Poppler::Annotation::SubType>() << Poppler::Annotation::ALink));
    }
}

Poppler::Page *Document::page(int page)
{
    return m_pages.value(page,nullptr);
}

const QDomDocument *Document::toc()
{
    if (!m_document)
        return nullptr;

    return m_document->toc();
}

Poppler::LinkDestination *Document::linkDestination(const QString &destination)
{
    return m_document->linkDestination(destination);
}

const QList<Poppler::Annotation *> &Document::annotations(int page)
{
    Q_ASSERT(page < m_annotations.length());
    return m_annotations.at(page);
}

void Document::reset()
{
    for (int i = 0; i < m_annotations.length();++i)
    {
        const QList<Poppler::Annotation *> & list=  m_annotations.at(i);
        for (int j = 0; j < list.length();++j)
            delete (list.at(j));
    }
    m_annotations.clear();
    qDeleteAll(m_pages);
    m_pages.clear();
    m_fileName.clear();
    delete m_document;
    m_document = nullptr;
}

QString Document::title()
{
    if (!m_document)
        return QString();

    return m_document->title();
}

