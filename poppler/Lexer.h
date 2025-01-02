//========================================================================
//
// Lexer.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2006, 2007, 2010, 2013, 2017-2019 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2006 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
// Copyright (C) 2013 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2013 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2019 Adam Reichold <adam.reichold@t-online.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef LEXER_H
#define LEXER_H

#include "Object.h"
#include "Stream.h"

class XRef;

#define tokBufSize 128 // size of token buffer

//------------------------------------------------------------------------
// Lexer
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT Lexer
{
public:
    // Construct a lexer for a single stream.  Deletes the stream when
    // lexer is deleted.
    Lexer(XRef *xrefA, Stream *str);

    // Construct a lexer for a stream or array of streams (assumes obj
    // is either a stream or array of streams).
    Lexer(XRef *xrefA, Object *obj);

    // Destructor.
    ~Lexer();

    Lexer(const Lexer &) = delete;
    Lexer &operator=(const Lexer &) = delete;

    // Get the next object from the input stream.
    Object getObj(int objNum = -1);
    Object getObj(const char *cmdA, int objNum);
    template<typename T>
    Object getObj(T) = delete;

    // Skip to the beginning of the next line in the input stream.
    void skipToNextLine();

    // Skip over one character.
    void skipChar() { getChar(); }

    // Get stream.
    Stream *getStream() { return curStr; }

    // Get current position in file.  This is only used for error
    // messages.
    Goffset getPos() const { return curStr->getPos(); }

    // Set position in file.
    void setPos(Goffset pos) { curStr->setPos(pos); }

    // Returns true if <c> is a whitespace character.
    static bool isSpace(int c);

    XRef *getXRef() const { return xref; }
    bool hasXRef() const { return xref != nullptr; }

private:
    inline int getChar()
    {
        int c;

        while ((c = curStr->getChar()) == EOF) {
            if (nextStreamIdx == streams.size()) {
                return EOF;
            }
            nextStream();
        }
        return c;
    }

    inline int lookChar() { return curStr->lookChar(); }
    void nextStream();

    std::vector<Object> streams;
    size_t nextStreamIdx;
    Stream *curStr;

    char tokBuf[tokBufSize]; // temporary token buffer

    XRef *xref;
};

#endif
