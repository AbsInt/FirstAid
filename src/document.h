#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <poppler-qt5.h>

#include <QList>
#include <QString>

class Document
{
public:
    Document();
    ~Document();

    void setDocument(const QString & fileName,QString *errorMessage);

    int numPages() {return m_numPages;}
    Poppler::Page* page(int page);
    const QDomDocument *toc();
    Poppler::LinkDestination *linkDestination(const QString &destination);

    bool isValid() {return m_document!=nullptr;}

    QString title();

private:
    int m_numPages = 0;
    QList<Poppler::Page*> m_pages;
    QString m_fileName;

    Poppler::Document *m_document = nullptr;
};

#endif // DOCUMENT_H
