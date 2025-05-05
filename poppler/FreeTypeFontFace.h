//========================================================================
//
// FreeTypeFontFace.h
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2019 Oliver Sander <oliver.sander@tu-dresden.de>
//
//========================================================================

#ifndef FREETYPEFONTFACE_H
#define FREETYPEFONTFACE_H

class Gfx8BitFont;

// TODO: These are the FreeType-internal names of FT_Library and FT_Face.
// It is preferable not to use them directly, but how do we then do forward declarations?
struct FT_LibraryRec_;
struct FT_FaceRec_;

/** \brief Gather and store metric information about FreeType fonts */
class FreeTypeFontFace
{
public:
    FreeTypeFontFace(Gfx8BitFont *gf, const std::string &fileName);
    FreeTypeFontFace(Gfx8BitFont *gf, const char *buffer, int bufferLen);

    ~FreeTypeFontFace();

    std::vector<double> getFontMetrics() const;

private:
    FT_LibraryRec_ *ftLibrary;
    FT_FaceRec_ *ftFace;

    Gfx8BitFont *gfxFont;
    std::unique_ptr<FoFiTrueType> ff;
};

#endif
