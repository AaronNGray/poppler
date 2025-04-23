//========================================================================
//
// pdfunite.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright (C) 2011-2015, 2017 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2012 Arseny Solokha <asolokha@gmx.com>
// Copyright (C) 2012 Fabio D'Urso <fabiodurso@hotmail.it>
// Copyright (C) 2012, 2014, 2017-2019, 2021, 2022 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2013 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2013 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2015 Arthur Stavisky <vovodroid@gmail.com>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
// Copyright (C) 2018 Adam Reichold <adam.reichold@t-online.de>
// Copyright (C) 2019 Marek Kasik <mkasik@redhat.com>
// Copyright (C) 2019, 2023 Oliver Sander <oliver.sander@tu-dresden.de>
// Copyright (C) 2022 crt <chluo@cse.cuhk.edu.hk>
//
//========================================================================

#include <PDFDoc.h>
#include <GlobalParams.h>
#include "parseargs.h"
#include "config.h"
#include <poppler-config.h>
#include <vector>
#include <iostream>
#include <map>

static bool printVersion = false;
static bool printHelp = false;

static const ArgDesc argDesc[] = { { "-v", argFlag, &printVersion, 0, "print copyright and version info" }, { "-h", argFlag, &printHelp, 0, "print usage information" }, { "-help", argFlag, &printHelp, 0, "print usage information" },
                                   { "--help", argFlag, &printHelp, 0, "print usage information" },         { "-?", argFlag, &printHelp, 0, "print usage information" }, {} };

static int loadDocs(int argc, char *argv[], int &majorVersion, int &minorVersion, std::vector<std::unique_ptr<PDFDoc>> &docs, bool &retFlag)
{
    retFlag = true;
    for (int i = 1; i < argc - 1; i++) {
        std::unique_ptr<PDFDoc> doc = std::make_unique<PDFDoc>(std::make_unique<GooString>(argv[i]));
        if (doc->isOk() && !doc->isEncrypted() && doc->getXRef()->getCatalog().isDict()) {
            if (doc->getPDFMajorVersion() > majorVersion) {
                majorVersion = doc->getPDFMajorVersion();
                minorVersion = doc->getPDFMinorVersion();
            } else if (doc->getPDFMajorVersion() == majorVersion) {
                if (doc->getPDFMinorVersion() > minorVersion) {
                    minorVersion = doc->getPDFMinorVersion();
                }
            }
            docs.push_back(std::move(doc));
        } else if (doc->isOk()) {
            if (doc->isEncrypted()) {
                error(errUnimplemented, -1, "Could not merge encrypted files ('{0:s}')", argv[i]);
                return -1;
            } else if (!doc->getXRef()->getCatalog().isDict()) {
                error(errSyntaxError, -1, "XRef's Catalog is not a dictionary ('{0:s}')", argv[i]);
                return -1;
            }
        } else {
            error(errSyntaxError, -1, "Could not merge damaged documents ('{0:s}')", argv[i]);
            return -1;
        }
    }
    retFlag = false;
    return {};
}

///////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
///////////////////////////////////////////////////////////////////////////
// Merge PDF files given by arguments 1 to argc-2 and write the result
// to the file specified by argument argc-1.
// This does not work with encrypted documents. PDF version is the maximum
// of every PDF version of files to merge.
///////////////////////////////////////////////////////////////////////////
{
    std::vector<Object> pages;
    std::vector<unsigned int> offsets;
    FILE *f;
    OutStream *outStr;
    std::vector<std::unique_ptr<PDFDoc>> docs;
    std::set<std::string> docFilenames;
    int majorVersion = 0;
    int minorVersion = 0;
    char *fileName = argv[argc - 1];

    const bool ok = parseArgs(argDesc, &argc, argv);
    if (!ok || argc < 3 || printVersion || printHelp) {
        fprintf(stderr, "pdfunite version %s\n", PACKAGE_VERSION);
        fprintf(stderr, "%s\n", popplerCopyright);
        fprintf(stderr, "%s\n", xpdfCopyright);
        if (!printVersion) {
            printUsage("pdfunite", "<PDF-sourcefile-1>..<PDF-sourcefile-n> <PDF-destfile>", argDesc);
        }
        if (printVersion || printHelp) {
            return 0;
        }
        return 99;
    }
    globalParams = std::make_unique<GlobalParams>();

    /* Step 1: loading all documents which will be merged, fail if a document is encrypted,
     * damaged, or if its XRef's Catalog is not a dictionary */
    bool retFlag;
    int retVal = loadDocs(argc, argv, majorVersion, minorVersion, docs, retFlag);
    if (retFlag) {
        return retVal;
    }

    if (!(f = fopen(fileName, "wb"))) {
        error(errIO, -1, "Could not open file '{0:s}'", fileName);
        return -1;
    }

    for (size_t i = 1; i < docs.size(); i++) {
        std::optional<std::map<Ref, Ref>> refMap = {};
        for (int j = 1; j <= docs[i]->getNumPages(); j++) {
            refMap = docs[0]->getCatalog()->insertPage(docs[i]->getPage(j), -1, refMap);
        }
    }

    outStr = new FileOutStream(f, 0);
    docs[0]->saveAs(outStr);
    outStr->close();
    fclose(f);
    delete outStr;

    return 0;
}
