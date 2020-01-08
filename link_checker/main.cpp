/*
 * Author(s): Jan Pohland <pohland@absint.com>
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

#include <QCoreApplication>


#include <poppler-qt5.h>
#include <QCommandLineParser>

#include <QFileInfo>

/**
 * main
 * @param argc Number of command line arguments.
 * @param argv Command line vector.
 * @return 0 in case of success, 1 otherwise.
 */
int main(int argc, char **argv)
{
    /*  determine command line options */
    QCoreApplication app(argc, argv);

    /* initialize commandline parser */
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("FirstAid - Link checker - PDF Help Viewer"));
    parser.addPositionalArgument(QStringLiteral("file"), QCoreApplication::translate("main", "PDF file to open"));
    parser.addOption(QCommandLineOption(QStringLiteral("link-file"), QCoreApplication::translate("main", "File with the links to check."),QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringLiteral("output-file"), QCoreApplication::translate("main", "File to which the invalid links are printed."),QStringLiteral("file")));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    QString fileName = (args.empty() ? QString() : args.at(0));

    if (fileName.isEmpty()){
        printf("No pdf file specified.\n");
        return 1;
    }

    QString outputFileName;
    if (parser.isSet(QStringLiteral("output-file"))){
        outputFileName = parser.value(QStringLiteral("output-file"));
    }

    QString linkFileName;
    if (parser.isSet(QStringLiteral("link-file"))){
        linkFileName = parser.value(QStringLiteral("link-file"));
    } else {
        printf("No link file specified.\n");
        return 1;
    }

    // load pdf
    Poppler::Document *document = nullptr;
    QFile pdfFile(fileName);
    if (pdfFile.exists() && pdfFile.size()) {
        document = Poppler::Document::load(fileName);
        if (!document || document->isLocked()) {
            delete document;
            printf("Cannot open file %s.\n", qPrintable(pdfFile.fileName()));
            return 5;
        }

    }else {
        printf("File %s does not exist or is empty.\n", qPrintable(pdfFile.fileName()));
        return 2;
    }

    QFile outputFile(outputFileName);
    bool write = false;
    if (!outputFileName.isEmpty()) {
        write = outputFile.open(QFile::WriteOnly);
    }

    QFile linkFile(linkFileName);
    if (linkFile.exists() && linkFile.size()) {
        printf("Processing link file...\n");
        if (linkFile.open(QFile::ReadOnly)) {
            while (!linkFile.atEnd()) {
                QString line = QString::fromUtf8(linkFile.readLine());
                printf("  Processing link '%s'...", qPrintable(line.trimmed()));
                bool valid = false;
                if (Poppler::LinkDestination *linkDest = (document->linkDestination(line.trimmed()))) {
                    valid = linkDest->pageNumber() > 0;
                    delete linkDest;
                }
                if (valid)
                    printf(" ok\n");
                else {
                    printf(" INVALID\n");
                    if (write)
                        outputFile.write(line.toUtf8().constData());
                }
            }
                           
            linkFile.close();
            if (write)
                outputFile.close();
            delete document;
        } else {
            delete document;
            if (write)
                outputFile.close();
            printf("Cannot open file %s for reading.\n", qPrintable(linkFile.fileName()));
        return 3;
        }
    } else {
        delete document;
        if (write)
            outputFile.close();
        printf("File %s does not exist or is empty.\n", qPrintable(linkFile.fileName()));
        return 4;
    }
    return 0;
}
