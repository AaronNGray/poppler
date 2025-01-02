//========================================================================
//
// FlateStream.cc
//
// Copyright (C) 2005, Jeff Muizelaar <jeff@infidigm.net>
// Copyright (C) 2010, 2021, Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2016, William Bader <williambader@hotmail.com>
// Copyright (C) 2017, Adrian Johnson <ajohnson@redneon.com>
//
// This file is under the GPLv2 or later license
//
//========================================================================

#include <config.h>

#include "poppler-config.h"

#ifdef ENABLE_ZLIB_UNCOMPRESS

#    include "FlateStream.h"

FlateStream::FlateStream(Stream *strA, int columns, int colors, int bits) : FilterStream(strA)
{
    memset(&d_stream, 0, sizeof(d_stream));
    inflateInit(&d_stream);
}

FlateStream::~FlateStream()
{
    flushBackToParent(str, d_stream.avail_in, d_stream.next_in);
    inflateEnd(&d_stream);

    delete str;
}

void FlateStream::reset()
{
    // FIXME: what are the semantics of reset?
    // i.e. how much initialization has to happen in the constructor?

    /* reinitialize zlib */
    inflateEnd(&d_stream);
    memset(&d_stream, 0, sizeof(d_stream));
    inflateInit(&d_stream);

    str->reset();
    purgeBuffer();
    d_stream.avail_in = 0;
    status = Z_OK;
}

int FlateStream::getSomeChars(int nChars, unsigned char *buffer)
{
    // Z_BUF_ERROR happens if avail_in or avail_out is 0 and isn't fatal.
    if (status != Z_OK && status != Z_BUF_ERROR) {
        return 0;
    }

    if (d_stream.avail_in == 0) {
        d_stream.next_in = in_buf;
        d_stream.avail_in = str->doGetChars(sizeof(in_buf), in_buf);
        if (d_stream.avail_in == 0) {
            status = Z_STREAM_END;
            return 0;
        }
    }

    d_stream.avail_out = nChars;
    d_stream.next_out = buffer;

    status = inflate(&d_stream, Z_SYNC_FLUSH);
    return nChars - d_stream.avail_out;
}

GooString *FlateStream::getPSFilter(int psLevel, const char *indent)
{
    GooString *s;

    if (psLevel < 3) {
        return NULL;
    }
    if (!(s = str->getPSFilter(psLevel, indent))) {
        return NULL;
    }
    s->append(indent)->append("<< >> /FlateDecode filter\n");
    return s;
}

bool FlateStream::isBinary(bool last) const
{
    return str->isBinary(true);
}

#endif
