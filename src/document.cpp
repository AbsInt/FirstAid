#include "document.h"


Document::Document()
{

}

Document::~Document()
{
    qDeleteAll(m_pages);
    delete m_document;
}

void Document::setDocument(const QString &fileName, QString *errorMessage)
{
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
    m_numPages = newdoc->numPages();

    m_pages = QList<Poppler::Page*>();
    m_pages.reserve(m_numPages);
    for (int i = 0; i < m_numPages; ++i)
    {
        m_pages.append(newdoc->page(i));

    }
}

Poppler::Page *Document::page(int page)
{
    if (page>=m_pages.length())
        return nullptr;
    return m_pages.at(page);
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

QString Document::title()
{
    if (!m_document)
        return QString();

    return m_document->title();
}

