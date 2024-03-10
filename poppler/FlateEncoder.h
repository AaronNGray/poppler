//========================================================================
//
// FlateEncoder.h
//
// Copyright (C) 2016, William Bader <williambader@hotmail.com>
// Copyright (C) 2018, 2019, 2021 Albert Astals Cid <aacid@kde.org>
//
// This file is under the GPLv2 or later license
//
//========================================================================

#ifndef FLATEENCODE_H
#define FLATEENCODE_H

#include "poppler-config.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <cstring>
#include <cctype>
#include "goo/gmem.h"
#include "goo/gfile.h"
#include "Error.h"
#include "Object.h"
#include "Decrypt.h"

#include "Stream.h"

extern "C" {
#include <zlib.h>
}

//------------------------------------------------------------------------
// FlateEncoder
//------------------------------------------------------------------------

class FlateEncoder : public FilterStream
{
public:
    explicit FlateEncoder(Stream *strA);
    ~FlateEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int psLevel, const char *indent) override { return nullptr; }
    bool isBinary(bool last = true) const override { return true; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar() { return (outBufPtr >= outBufEnd && !fillBuf()) ? EOF : (*outBufPtr++ & 0xff); }

    static const int inBufSize = 16384;
    static const int outBufSize = inBufSize;
    unsigned char inBuf[inBufSize];
    unsigned char outBuf[outBufSize];
    unsigned char *outBufPtr;
    unsigned char *outBufEnd;
    bool inBufEof;
    bool outBufEof;
    z_stream zlib_stream;

    bool fillBuf();
};

#endif
