//========================================================================
//
// FreeTypeFontFace.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2019 Oliver Sander <oliver.sander@tu-dresden.de>
//
//========================================================================

#include <config.h>

#include <vector>
#include <memory>
#include <iostream>

#include <fofi/FoFiTrueType.h>
#include <poppler/FreeTypeFontFace.h>
#include <poppler/GfxFont.h>

#include <ft2build.h>
#include FT_FREETYPE_H

FreeTypeFontFace::FreeTypeFontFace(Gfx8BitFont *gf, const std::string &fileName) : gfxFont(gf)
{
    ff = FoFiTrueType::load(fileName.c_str());

    // Set up FreeType library  TODO: Error handling
    auto ftError = FT_Init_FreeType(&ftLibrary);
    if (ftError) {
        error(errInternal, -1, "FT_Init_FreeType returned error code {0:d}", ftError);
    }

    // TODO: Is 0 always the correct choice here?
    int faceIndex = 0; // Get the zero-th face, in case the font source contains more than one

    if (FT_New_Face(ftLibrary, fileName.c_str(), faceIndex, &ftFace))
        std::cout << "FT_New_Face returned error code!" << std::endl;

    std::cout << "FreeType font family name: " << ftFace->family_name << std::endl;
    std::cout << "   ... number of glyphs: " << ftFace->num_glyphs << std::endl;

    // TODO: Set this to something reasonable
    // TODO: Error handling
    ftError = FT_Set_Char_Size(ftFace, /* handle to face object           */
                               0, /* char_width in 1/64th of points, 0 means: same as height  */
                               16 * 64, /* char_height in 1/64th of points */
                               200, /* horizontal device resolution    */
                               200); /* vertical device resolution      */

    if (ftError) {
        error(errInternal, -1, "FT_Set_Char_Size returned error code {0:d}", ftError);
    }
}

FreeTypeFontFace::FreeTypeFontFace(Gfx8BitFont *gf, const char *buffer, int bufferLen) : gfxFont(gf)
{
    ff = FoFiTrueType::make(buffer, bufferLen);

    // Set up FreeType library  TODO: Error handling
    auto ftError = FT_Init_FreeType(&ftLibrary);
    if (ftError) {
        error(errInternal, -1, "FT_Init_FreeType returned error code {0:d}", ftError);
    }

    // TODO: Is 0 always the correct choice here?
    int faceIndex = 0; // Get the zero-th face, in case the font source contains more than one

    if (FT_New_Memory_Face(ftLibrary, (const FT_Byte *)buffer, bufferLen, faceIndex, &ftFace))
        std::cout << "FT_New_Memory_Face returned error code!" << std::endl;

    std::cout << "FreeType font family name: " << ftFace->family_name << std::endl;
    std::cout << "   ... number of glyphs: " << ftFace->num_glyphs << std::endl;

    // TODO: Set this to something reasonable
    // TODO: Error handling
    ftError = FT_Set_Char_Size(ftFace, /* handle to face object           */
                               0, /* char_width in 1/64th of points, 0 means: same as height  */
                               16 * 64, /* char_height in 1/64th of points */
                               200, /* horizontal device resolution    */
                               200); /* vertical device resolution      */

    if (ftError) {
        error(errInternal, -1, "FT_Set_Char_Size returned error code {0:d}", ftError);
    }
}

FreeTypeFontFace::~FreeTypeFontFace()
{
    FT_Done_Face(ftFace);
    FT_Done_FreeType(ftLibrary);
}

std::vector<double> FreeTypeFontFace::getFontMetrics() const
{
    std::vector<double> freeTypeWidths(256);
    int *codeToGID = gfxFont->getCodeToGIDMap(ff.get());

    for (int c = 0; c < 256; c++) {
        FT_UInt gid;

        /*
         * SplashFTFont::makeGlyph is doing something similar

        Type1 fonts: map from name to gid, use FT_Get_Name_Index
        CID fonts: see getCIDToGID

        // if we're substituting for a non-TrueType font, we need to mark
        // all notdef codes as "do not draw" (rather than drawing TrueType
        // notdef glyphs)

        if (ff->codeToGID && c < ff->codeToGIDLen && c >= 0) {
          gid = (FT_UInt)ff->codeToGID[c];
        } else { */
        // gid = (FT_UInt)c;
        gid = codeToGID[c];
        /* } */
        FT_Error ftError = FT_Load_Glyph(ftFace,
                                         gid, // glyph_index: The index of the glyph in the font file. For CID-keyed fonts (either in PS or in CFF format) this argument specifies the CID value.
                                         FT_LOAD_NO_SCALE); // Load flags, TODO: 0 may not be the best choice
        if (ftError) {
            error(errInternal, -1, "FT_Load_Glyph returned error code {0:d} when loading glyph with gid {1:d}", ftError, gid);
            std::cout << "c: " << c << ",  gid: " << gid << std::endl;
        }

        freeTypeWidths[c] = (double)(ftFace->glyph->metrics.horiAdvance) / (double)ftFace->units_per_EM; //* 0.001;
    }

    return freeTypeWidths;
}
