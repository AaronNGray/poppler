//========================================================================
//
// FlateStream.h
//
// Copyright (C) 2005, Jeff Muizelaar <jeff@infidigm.net>
// Copyright (C) 2010, 2011, 2019, 2021, Albert Astals Cid <aacid@kde.org>
//
// This file is under the GPLv2 or later license
//
//========================================================================

#ifndef FLATESTREAM_H
#define FLATESTREAM_H

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

class FlateStream : public FilterStream
{
public:
    FlateStream(Stream *strA, int columns, int colors, int bits);
    virtual ~FlateStream();
    StreamKind getKind() const override { return strFlate; }
    void reset() override;

    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    int getSomeChars(int nChars, unsigned char *buffer) override;

private:
    z_stream d_stream;
    int status;
    unsigned char in_buf[8192];
};

#endif
