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
#include "CairoOutputDev.h"
#include "cairo.h"
#include "cairo-svg.h"
#include <sstream>
#include <fstream>
#include <streambuf>

static int loadDocs(int argc, const char *argv[], int &majorVersion, int &minorVersion, std::vector<std::unique_ptr<PDFDoc>> &docs, bool &retFlag)
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

bool draw_twice = false; // triggers a bug with deeply-copied streams

static cairo_surface_t *doc_to_image(PDFDoc *doc, int page)
{
    /* Render a page from the first document to check everything is fine */
    auto output_dev = CairoOutputDev();
    output_dev.startDoc(doc);

    auto image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 500, 500);
    auto cr = cairo_create(image);

    auto font_options = cairo_font_options_create();
    cairo_get_font_options(cr, font_options);
    cairo_set_font_options(cr, font_options);
    cairo_font_options_destroy(font_options);

    output_dev.setCairo(cr);
    if (draw_twice) {
        doc->displayPage(&output_dev, page, 72., 72., 0, false, true, false);
        doc->displayPage(&output_dev, page, 72., 72., 0, false, true, false);
    } else {
        doc->displayPage(&output_dev, page, 72., 72., 0, false, true, false);
    }

    cairo_destroy(cr);
    cairo_surface_flush(image);
#if 0
    static int i = 0;
    i++;
    char s[10];
    sprintf(s, "%d.png", i);
    cairo_surface_write_to_png(image, s);
#endif
    return image;
}

static bool test_doc_draw_equal(PDFDoc *doc, int page, PDFDoc *doc2, int page2)
{
    auto img1 = doc_to_image(doc, page);
    auto img2 = doc_to_image(doc2, page2);
    int size = cairo_image_surface_get_stride(img1);
    auto data1 = cairo_image_surface_get_data(img1);
    auto data2 = cairo_image_surface_get_data(img2);

    for (int i = 0; i < 500 * size; i++) {
        if (data1[i] != data2[i]) {
            return false;
        }
    }

    cairo_surface_destroy(img1);
    cairo_surface_destroy(img2);
    return true;
}

static int test_for_files(const int argc, const char *argv[], std::string const &out = "test_combine.pdf", std::set<std::pair<int, int>> const &to_remove = std::set<std::pair<int, int>>())
{
    std::vector<unsigned int> offsets;
    FILE *f;
    OutStream *outStr;
    std::vector<std::unique_ptr<PDFDoc>> docs;
    std::set<std::string> docFilenames;
    int majorVersion = 0;
    int minorVersion = 0;
    const char *fileName = out.c_str();
    bool removeFirst = false;

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

    /* Let's cache a few things */

    int numPages = docs[0]->getNumPages();

    for (int j = 1; j <= numPages; j++) {
        GooString label1;
        docs[0]->getCatalog()->indexToLabel(j - 1, &label1);
        assert(docs[0]->getCatalog()->getPage(j)->getNum() == j);
    }

    if (removeFirst) {
        for (size_t i = 1; i < docs.size(); i++) {
            std::optional<std::map<Ref, Ref>> refMap = {};
            int removed = 0;
            int npages = docs[i]->getNumPages();
            for (int j = 1; j <= npages; j++) {
                if (to_remove.contains(std::pair(i, j))) {
                    docs[i]->removePage(docs[i]->getPage(j - removed));
                    removed++;
                }
            }
        }
    }

    int latest_added = -1;

    for (size_t i = 1; i < docs.size(); i++) {
        std::optional<std::map<Ref, Ref>> refMap = {};
        for (int j = 1; j <= docs[i]->getNumPages(); j++) {
            if (removeFirst || !to_remove.contains(std::pair(i, j))) {
                latest_added = j;
                docs[0]->insertPage(docs[i]->getPage(j), 1, refMap);
                numPages++;
            }
        }
    }

    assert(numPages == docs[0]->getNumPages());

    /* Check that rendering the inserted page in the target document is equal to rendering in the source document */

    if (!test_doc_draw_equal(docs[0].get(), 1, docs[docs.size() - 1].get(), latest_added)) {
        return EXIT_FAILURE;
    }

    outStr = new FileOutStream(f, 0);
    docs[0]->saveAs(outStr, writeForceRewrite);
    outStr->close();
    fclose(f);
    delete outStr;

    /* 2. Tests that internal data was correctly updated */

    std::unique_ptr<PDFDoc> doc_final = std::make_unique<PDFDoc>(std::make_unique<GooString>(fileName));
    assert(doc_final->isOk());
    assert(numPages == doc_final->getNumPages());

    /* 2.1  Page labels */
    for (int j = 1; j <= numPages; j++) {
        GooString label1;
        GooString label2;
        docs[0]->getCatalog()->indexToLabel(j - 1, &label1);
        doc_final->getCatalog()->indexToLabel(j - 1, &label2);
        assert(label1.toStr() == label2.toStr());
        assert(docs[0]->getCatalog()->getPage(j)->getNum() == j);
    }

    return EXIT_SUCCESS;
}

static int test_remove(std::string const &name, std::string const &out = "test_combine.pdf", std::set<std::pair<int, int>> const &pages = std::set<std::pair<int, int>>())
{
    std::vector<unsigned int> offsets;
    FILE *f;
    OutStream *outStr;
    std::set<std::string> docFilenames;
    const char *fileName = out.c_str();

    globalParams = std::make_unique<GlobalParams>();

    std::unique_ptr<PDFDoc> doc = std::make_unique<PDFDoc>(std::make_unique<GooString>(name));

    if (!(f = fopen(fileName, "wb"))) {
        error(errIO, -1, "Could not open file '{0:s}'", fileName);
        return -1;
    }

    int numPages = doc->getNumPages();

    doc->removePage(doc->getPage(2));
    numPages--;

    assert(numPages == doc->getNumPages());

    /* Write and reopen */
    outStr = new FileOutStream(f, 0);
    doc->saveAs(outStr);
    outStr->close();
    fclose(f);
    delete outStr;

    std::unique_ptr<PDFDoc> doc_final = std::make_unique<PDFDoc>(std::make_unique<GooString>(fileName));
    assert(doc_final->isOk());
    assert(numPages == doc_final->getNumPages());

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    const char *argv1[] = { "", TESTDATADIR "/unittestcases/WithActualText.pdf", TESTDATADIR "/unittestcases/WithActualText.pdf" };
    assert(test_for_files(4, argv1) == EXIT_SUCCESS);

    const char *argv11[] = { "", TESTDATADIR "/unittestcases/NestedLayers.pdf", TESTDATADIR "/unittestcases/WithActualText.pdf" };
    assert(test_for_files(4, argv11) == EXIT_SUCCESS);

    /* Linearized */
    const char *argv2[] = { "", TESTDATADIR "/unittestcases/doublepage.pdf", TESTDATADIR "/unittestcases/WithActualText.pdf" };
    assert(test_for_files(4, argv2) == EXIT_SUCCESS);

    /* Merging only one file */
    const char *argv3[] = { "", TESTDATADIR "/unittestcases/utf16le-annot.pdf", TESTDATADIR "/unittestcases/utf16le-annot.pdf" };
    assert(test_for_files(4, argv3, "test_twopageannot.pdf") == EXIT_SUCCESS);
    const char *argv4[] = { "", TESTDATADIR "/unittestcases/doublepage.pdf", "test_twopageannot.pdf" };
    assert(test_for_files(4, argv4, "test_stream.pdf", { std::pair(1, 2) }) == EXIT_SUCCESS);

    assert(test_remove("test_twopageannot.pdf") == EXIT_SUCCESS);

    std::unique_ptr<PDFDoc> doc = std::make_unique<PDFDoc>(std::make_unique<GooString>(TESTDATADIR "/unittestcases/NestedLayers.pdf"));
    doc->insertBlankPage(1);

    FILE *f;

    if (!(f = fopen("test_blank_page.pdf", "wb"))) {
        return -1;
    }

    auto outStr = new FileOutStream(f, 0);
    doc->saveAs(outStr);

    return EXIT_SUCCESS;
}