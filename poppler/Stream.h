//========================================================================
//
// Stream.h
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
// Copyright (C) 2005 Jeff Muizelaar <jeff@infidigm.net>
// Copyright (C) 2008 Julien Rebetez <julien@fhtagn.net>
// Copyright (C) 2008, 2010, 2011, 2016-2022, 2024 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2009 Stefan Thomas <thomas@eload24.com>
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2011, 2012, 2016, 2020 William Bader <williambader@hotmail.com>
// Copyright (C) 2012, 2013 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2012, 2013 Fabio D'Urso <fabiodurso@hotmail.it>
// Copyright (C) 2013, 2017 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2013 Peter Breitenlohner <peb@mppmu.mpg.de>
// Copyright (C) 2013, 2018 Adam Reichold <adamreichold@myopera.com>
// Copyright (C) 2013 Pino Toscano <pino@kde.org>
// Copyright (C) 2019 Volker Krause <vkrause@kde.org>
// Copyright (C) 2019 Alexander Volkov <a.volkov@rusbitech.ru>
// Copyright (C) 2020-2022 Oliver Sander <oliver.sander@tu-dresden.de>
// Copyright (C) 2020 Philipp Knechtges <philipp-dev@knechtges.com>
// Copyright (C) 2021 Hubert Figuiere <hub@figuiere.net>
// Copyright (C) 2021 Christian Persch <chpe@src.gnome.org>
// Copyright (C) 2021 Georgiy Sgibnev <georgiy@sgibnev.com>. Work sponsored by lab50.net.
// Copyright (C) 2024 g10 Code GmbH, Author: Sune Stolborg Vuorela <sune@vuorela.dk>
// Copyright (C) 2024 Fernando Herrera <fherrera@onirica.com>
// Copyright (C) 2024 Nelson Benítez León <nbenitezl@gmail.com>
// Copyright (C) 2023 Jonathan Hähne <jonathan.haehne@tum.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef STREAM_H
#define STREAM_H

#include <atomic>
#include <cstdio>
#include <vector>
#include <span>

#include "poppler-config.h"
#include "poppler_private_export.h"
#include "Object.h"

class GooFile;
class BaseStream;
class CachedFile;
class SplashBitmap;

//------------------------------------------------------------------------

enum StreamKind
{
    strFile,
    strCachedFile,
    strASCIIHex,
    strASCII85,
    strLZW,
    strRunLength,
    strCCITTFax,
    strDCT,
    strFlate,
    strJBIG2,
    strJPX,
    strWeird, // internal-use stream types
    strCrypt // internal-use to detect decode streams
};

enum StreamColorSpaceMode
{
    streamCSNone,
    streamCSDeviceGray,
    streamCSDeviceRGB,
    streamCSDeviceCMYK
};

//------------------------------------------------------------------------

// This is in Stream.h instead of Decrypt.h to avoid really annoying
// include file dependency loops.
enum CryptAlgorithm
{
    cryptRC4,
    cryptAES,
    cryptAES256,
    cryptNone
};

//------------------------------------------------------------------------

typedef struct _ByteRange
{
    size_t offset;
    unsigned int length;
} ByteRange;

//------------------------------------------------------------------------
// getChars polyfill for classes that only implement the legacy getChar
// interface.
//------------------------------------------------------------------------

#define polyfillGetSomeChars(getChar)                                                                                                                                                                                                          \
    int getSomeChars(int nChars, unsigned char *dest) override                                                                                                                                                                                 \
    {                                                                                                                                                                                                                                          \
        int i;                                                                                                                                                                                                                                 \
        for (i = 0; i < nChars; i++) {                                                                                                                                                                                                         \
            int next = getChar();                                                                                                                                                                                                              \
            if (next == EOF)                                                                                                                                                                                                                   \
                break;                                                                                                                                                                                                                         \
            dest[i] = next;                                                                                                                                                                                                                    \
        }                                                                                                                                                                                                                                      \
        return i;                                                                                                                                                                                                                              \
    }

//------------------------------------------------------------------------
// Stream (base class)
//------------------------------------------------------------------------

#define streamBufSize 8096

class POPPLER_PRIVATE_EXPORT Stream
{
public:
    // Constructor.
    Stream() : ref(1), bufPtr(buf), bufEnd(buf) { }

    // Destructor.
    virtual ~Stream() = default;

    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &other) = delete;

    // Get kind of stream.
    virtual StreamKind getKind() const = 0;

    // Reset stream to beginning.
    virtual void reset() = 0;

    // Close down the stream.
    virtual void close();

    inline int getChar() { return (bufPtr >= bufEnd && !fillCacheBuf()) ? EOF : (*bufPtr++ & 0xff); }
    inline int lookChar() { return (bufPtr >= bufEnd && !fillCacheBuf()) ? EOF : (*bufPtr & 0xff); }

    // Rename to getChars
    inline int doGetChars(int nchars, unsigned char *buffer)
    {
        int got = bufEnd - bufPtr;
        if (nchars <= got) {
            memcpy(buffer, bufPtr, nchars);
            bufPtr += nchars;
            return nchars;
        } else {
            memcpy(buffer, bufPtr, got);
            bufPtr = bufEnd;
            int added = 0;
            while ((added = getSomeChars(nchars - got, buffer + got)) != 0) {
                got += added;
            }
            return got;
        }
    }

    inline void fillString(std::string &s)
    {
        unsigned char readBuf[4096];
        int readChars;
        reset();
        while ((readChars = doGetChars(4096, readBuf)) != 0) {
            s.append((const char *)readBuf, readChars);
        }
    }

    inline void fillGooString(GooString *s) { fillString(s->toNonConstStr()); }

    inline std::vector<unsigned char> toUnsignedChars(int initialSize = 4096, int sizeIncrement = 4096)
    {
        std::vector<unsigned char> res(initialSize);

        int readChars;
        int size = initialSize;
        int length = 0;
        int charsToRead = initialSize;
        bool continueReading = true;
        reset();
        while (continueReading && (readChars = doGetChars(charsToRead, res.data() + length)) != 0) {
            length += readChars;
            if (readChars == charsToRead) {
                if (lookChar() != EOF) {
                    if (unlikely(checkedAdd(size, sizeIncrement, &size))) {
                        error(errInternal, -1, "toUnsignedChars size grew too much");
                        return {};
                    }
                    charsToRead = sizeIncrement;
                    if (unlikely(static_cast<size_t>(size) > res.max_size())) {
                        error(errInternal, -1, "toUnsignedChars size grew too much");
                        return {};
                    }
                    res.resize(size);
                } else {
                    continueReading = false;
                }
            } else {
                continueReading = false;
            }
        }

        res.resize(length);
        return res;
    }

    // Get next char directly from stream source, without filtering it
    virtual int getUnfilteredChar() = 0;

    // Resets the stream without reading anything (even not the headers)
    // WARNING: Reading the stream with something else than getUnfilteredChar
    // may lead to unexcepted behaviour until you call reset ()
    virtual void unfilteredReset() = 0;

    // Get next line from stream.
    virtual char *getLine(char *buf, int size);

    // Discard the next <n> bytes from stream.  Returns the number of
    // bytes discarded, which will be less than <n> only if EOF is
    // reached.
    virtual unsigned int discardChars(unsigned int n);

    virtual Goffset getPos() final
    {
        // Account for unread buffered data.
        return getRawPos() - (bufEnd - bufPtr);
    }

    // Go to a position in the stream.  If <dir> is negative, the
    // position is from the end of the file; otherwise the position is
    // from the start of the file.
    virtual void setPos(Goffset pos, int dir = 0) = 0;

    // Get PostScript command for the filter(s).
    virtual GooString *getPSFilter(int psLevel, const char *indent);

    // Does this stream type potentially contain non-printable chars?
    virtual bool isBinary(bool last = true) const = 0;

    // Get the BaseStream of this stream.
    virtual BaseStream *getBaseStream() = 0;

    // Get the stream after the last decoder (this may be a BaseStream
    // or a DecryptStream).
    virtual Stream *getUndecodedStream() = 0;

    // Get the dictionary associated with this stream.
    virtual Dict *getDict() = 0;
    virtual Object *getDictObject() = 0;

    // Is this an encoding filter?
    virtual bool isEncoder() const { return false; }

    // Get image parameters which are defined by the stream contents.
    virtual void getImageParams(int * /*bitsPerComponent*/, StreamColorSpaceMode * /*csMode*/, bool * /*hasAlpha*/) { }

    // Return the next stream in the "stack".
    virtual Stream *getNextStream() const { return nullptr; }

    // Add filters to this stream according to the parameters in <dict>.
    // Returns the new stream.
    Stream *addFilters(Dict *dict, int recursion = 0);

    // Returns true if this stream includes a crypt filter.
    bool isEncrypted() const;

    // May always return less than nChars, only a return of 0 indicates an EOF.
    virtual int getSomeChars(int nChars, unsigned char *buffer) = 0;

    virtual Goffset getRawPos() = 0;

private:
    friend class Object; // for incRef/decRef

    // Reference counting.
    int incRef() { return ++ref; }
    int decRef() { return --ref; }

    bool fillCacheBuf()
    {
        assert(bufPtr == bufEnd);
        bufPtr = buf;
        bufEnd = bufPtr + getSomeChars(streamBufSize, buf);
        return bufEnd > bufPtr;
    }

    Stream *makeFilter(const char *name, Stream *str, Object *params, int recursion = 0, Dict *dict = nullptr);

    std::atomic_int ref; // reference count

protected:
    unsigned char buf[streamBufSize];
    unsigned char *bufPtr = nullptr;
    unsigned char *bufEnd = nullptr;

    void purgeBuffer() { bufPtr = bufEnd = buf; }
};

//------------------------------------------------------------------------
// OutStream
//
// This is the base class for all streams that output to a file
//------------------------------------------------------------------------
class POPPLER_PRIVATE_EXPORT OutStream
{
public:
    // Constructor.
    OutStream();

    // Desctructor.
    virtual ~OutStream();

    OutStream(const OutStream &) = delete;
    OutStream &operator=(const OutStream &other) = delete;

    // Close the stream
    virtual void close() = 0;

    // Return position in stream
    virtual Goffset getPos() = 0;

    // Put a char in the stream
    virtual void put(char c) = 0;

    virtual size_t write(std::span<unsigned char> data) = 0;

    virtual void printf(const char *format, ...) GCC_PRINTF_FORMAT(2, 3) = 0;
};

//------------------------------------------------------------------------
// FileOutStream
//------------------------------------------------------------------------
class POPPLER_PRIVATE_EXPORT FileOutStream : public OutStream
{
public:
    FileOutStream(FILE *fa, Goffset startA);

    ~FileOutStream() override;

    void close() override;

    Goffset getPos() override;

    void put(char c) override;

    size_t write(std::span<unsigned char> data) override;

    void printf(const char *format, ...) override GCC_PRINTF_FORMAT(2, 3);

private:
    FILE *f;
    Goffset start;
};

//------------------------------------------------------------------------
// BaseStream
//
// This is the base class for all streams that read directly from a file.
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT BaseStream : public Stream
{
public:
    BaseStream(Object &&dictA, Goffset lengthA);
    ~BaseStream() override;
    virtual BaseStream *copy() = 0;
    virtual Stream *makeSubStream(Goffset start, bool limited, Goffset length, Object &&dict) = 0;
    void setPos(Goffset pos, int dir = 0) override = 0;
    bool isBinary(bool last = true) const override { return last; }
    BaseStream *getBaseStream() override { return this; }
    Stream *getUndecodedStream() override { return this; }
    Dict *getDict() override { return dict.getDict(); }
    Object *getDictObject() override { return &dict; }
    virtual GooString *getFileName() { return nullptr; }
    virtual Goffset getLength() { return length; }

    // Get/set position of first byte of stream within the file.
    virtual Goffset getStart() = 0;
    virtual void moveStart(Goffset delta) = 0;

protected:
    Goffset length;
    Object dict;
};

//------------------------------------------------------------------------
// BaseInputStream
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT BaseSeekInputStream : public BaseStream
{
public:
    // This enum is used to tell the seek() method how it must reposition
    // the stream offset.
    enum SeekType
    {
        SeekSet, // the offset is set to offset bytes
        SeekCur, // the offset is set to its current location plus offset bytes
        SeekEnd // the offset is set to the size of the stream plus offset bytes
    };

    BaseSeekInputStream(Goffset startA, bool limitedA, Goffset lengthA, Object &&dictA);
    ~BaseSeekInputStream() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    void close() override;

    void setPos(Goffset pos, int dir = 0) override;
    Goffset getStart() override { return start; }
    void moveStart(Goffset delta) override;

    int getUnfilteredChar() override { return getChar(); }
    void unfilteredReset() override { reset(); }

    int getSomeChars(int nChars, unsigned char *buffer) override;
    Goffset getRawPos() override { return bufPos; }

protected:
    Goffset start;
    bool limited;

private:
    virtual Goffset currentPos() const = 0;
    virtual void setCurrentPos(Goffset offset) = 0;
    virtual Goffset read(char *buf, Goffset size) = 0;

    Goffset bufPos;
    Goffset savePos;
    bool saved;
};

//------------------------------------------------------------------------
// FilterStream
//
// This is the base class for all streams that filter another stream.
//------------------------------------------------------------------------

class FilterStream : public Stream
{
public:
    // If asPredictedStream returns a new object, that one then owns the previous one.
    Stream *asPredictedStream(int predictor, int columns, int colors, int bits);

    explicit FilterStream(Stream *strA);
    ~FilterStream() override;
    void close() override;
    void setPos(Goffset pos, int dir = 0) override;
    BaseStream *getBaseStream() override { return str->getBaseStream(); }
    Stream *getUndecodedStream() override { return str->getUndecodedStream(); }
    Dict *getDict() override { return str->getDict(); }
    Object *getDictObject() override { return str->getDictObject(); }
    Stream *getNextStream() const override { return str; }

    int getUnfilteredChar() override { return str->getUnfilteredChar(); }
    void unfilteredReset() override { str->unfilteredReset(); }

    Goffset getRawPos() override { return str->getRawPos(); }

protected:
    Stream *str;
};

//------------------------------------------------------------------------
// ImageStream
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT ImageStream
{
public:
    // Create an image stream object for an image with the specified
    // parameters.  Note that these are the actual image parameters,
    // which may be different from the predictor parameters.
    ImageStream(Stream *strA, int widthA, int nCompsA, int nBitsA);

    ~ImageStream();

    ImageStream(const ImageStream &) = delete;
    ImageStream &operator=(const ImageStream &other) = delete;

    // Reset the stream.
    void reset();

    // Close the stream previously reset
    void close();

    // Gets the next pixel from the stream.  <pix> should be able to hold
    // at least nComps elements.  Returns false at end of file.
    bool getPixel(unsigned char *pix);

    // Returns a pointer to the next line of pixels.  Returns NULL at
    // end of file.
    unsigned char *getLine();

    // Skip an entire line from the image.
    void skipLine();

private:
    Stream *str; // base stream
    int width; // pixels per line
    int nComps; // components per pixel
    int nBits; // bits per component
    int nVals; // components per line
    int inputLineSize; // input line buffer size
    unsigned char *inputLine; // input line buffer
    unsigned char *imgLine; // line buffer
    int imgIdx; // current index in imgLine
};

//------------------------------------------------------------------------
// StreamPredictor
//------------------------------------------------------------------------

// In contrast to the normal FilterStream, the StreamPredictor owns its wrapped stream, mostly to make it compatible with the old callers.
class StreamPredictor : public FilterStream
{
public:
    StreamKind getKind() const override { return str->getKind(); }

    // Create a predictor object.  Note that the parameters are for the
    // predictor, and may not match the actual image parameters.
    StreamPredictor(Stream *strA, int predictorA, int widthA, int nCompsA, int nBitsA);

    ~StreamPredictor() override;

    StreamPredictor(const StreamPredictor &) = delete;
    StreamPredictor &operator=(const StreamPredictor &) = delete;

    bool isOk() { return ok; }

    void setPos(Goffset pos, int dir = 0) override;
    void reset() override { str->reset(); }

    bool isBinary(bool last = true) const override { return str->isBinary(last); };

    int getSomeChars(int nChars, unsigned char *buffer) override;
    Goffset getRawPos() override { return str->getPos(); }

private:
    bool getNextLine();

    int predictor; // predictor
    int width; // pixels per line
    int nComps; // components per pixel
    int nBits; // bits per component
    int nVals; // components per line
    int pixBytes; // bytes per pixel
    int rowBytes; // bytes per line
    unsigned char *predLine; // line buffer
    int predIdx; // current index in predLine
    bool ok;
};

//------------------------------------------------------------------------
// FileStream
//------------------------------------------------------------------------

#define fileStreamBufSize 256

class POPPLER_PRIVATE_EXPORT FileStream : public BaseStream
{
public:
    FileStream(GooFile *fileA, Goffset startA, bool limitedA, Goffset lengthA, Object &&dictA);
    ~FileStream() override;
    BaseStream *copy() override;
    Stream *makeSubStream(Goffset startA, bool limitedA, Goffset lengthA, Object &&dictA) override;
    StreamKind getKind() const override { return strFile; }
    void reset() override;
    void close() override;

    void setPos(Goffset pos, int dir = 0) override;
    Goffset getStart() override { return start; }
    void moveStart(Goffset delta) override;

    int getUnfilteredChar() override { return getChar(); }
    void unfilteredReset() override { reset(); }

    bool getNeedsEncryptionOnSave() const { return needsEncryptionOnSave; }
    void setNeedsEncryptionOnSave(bool needsEncryptionOnSaveA) { needsEncryptionOnSave = needsEncryptionOnSaveA; }

    int getSomeChars(int nChars, unsigned char *buffer) override;
    Goffset getRawPos() override { return offset; }

private:
    GooFile *file;
    Goffset offset;
    Goffset start;
    bool limited;
    Goffset savePos;
    bool saved;
    bool needsEncryptionOnSave; // Needed for FileStreams that point to "external" files
                                // and thus when saving we can't do a raw copy
};

//------------------------------------------------------------------------
// CachedFileStream
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT CachedFileStream : public BaseStream
{
public:
    CachedFileStream(CachedFile *ccA, Goffset startA, bool limitedA, Goffset lengthA, Object &&dictA);
    ~CachedFileStream() override;
    BaseStream *copy() override;
    Stream *makeSubStream(Goffset startA, bool limitedA, Goffset lengthA, Object &&dictA) override;
    StreamKind getKind() const override { return strCachedFile; }
    void reset() override;
    void close() override;

    void setPos(Goffset pos, int dir = 0) override;
    Goffset getStart() override { return start; }
    void moveStart(Goffset delta) override;

    int getUnfilteredChar() override { return getChar(); }
    void unfilteredReset() override { reset(); }

    int getSomeChars(int nChars, unsigned char *buffer) override;
    Goffset getRawPos() override { return strPos; }

private:
    CachedFile *cc;
    Goffset start;
    bool limited;
    unsigned int strPos;
    int savePos;
    bool saved;
};

//------------------------------------------------------------------------
// MemStream
//------------------------------------------------------------------------

template<typename T>
class BaseMemStream : public BaseStream
{
public:
    BaseMemStream(T *bufA, Goffset startA, Goffset lengthA, Object &&dictA) : BaseStream(std::move(dictA), lengthA)
    {
        mem = bufA;
        start = startA;
        length = lengthA;
        memEnd = mem + start + length;
        memPtr = mem + start;
    }

    BaseStream *copy() override { return new BaseMemStream(mem, start, length, dict.copy()); }

    Stream *makeSubStream(Goffset startA, bool limited, Goffset lengthA, Object &&dictA) override
    {
        Goffset newLength;

        if (!limited || startA + lengthA > start + length) {
            newLength = start + length - startA;
        } else {
            newLength = lengthA;
        }
        return new BaseMemStream(mem, startA, newLength, std::move(dictA));
    }

    StreamKind getKind() const override { return strWeird; }

    void reset() override
    {
        memPtr = mem + start;
        purgeBuffer();
    }

    void close() override { }

    void setPos(Goffset pos, int dir = 0) override
    {
        Goffset i;

        if (dir >= 0) {
            i = pos;
        } else {
            i = start + length - pos;
        }
        if (i < start) {
            i = start;
        } else if (i > start + length) {
            i = start + length;
        }
        memPtr = mem + i;
        purgeBuffer();
    }

    Goffset getStart() override { return start; }

    void moveStart(Goffset delta) override
    {
        start += delta;
        length -= delta;
        memPtr = mem + start;
    }

    int getUnfilteredChar() override { return getChar(); }

    void unfilteredReset() override { reset(); }

    int getSomeChars(int nChars, unsigned char *buffer) override
    {
        if (unlikely(nChars <= 0)) {
            return 0;
        }
        if (unlikely(memPtr >= memEnd)) {
            return 0;
        }
        int remaining = memEnd - memPtr;
        if (remaining < nChars) {
            nChars = remaining;
        }
        memcpy(buffer, memPtr, nChars);
        memPtr += nChars;
        return nChars;
    }
    Goffset getRawPos() override { return (int)(memPtr - mem); }

protected:
    T *mem;

private:
    Goffset start;
    T *memEnd;
    T *memPtr;
};

class POPPLER_PRIVATE_EXPORT MemStream : public BaseMemStream<const char>
{
public:
    MemStream(const char *bufA, Goffset startA, Goffset lengthA, Object &&dictA) : BaseMemStream(bufA, startA, lengthA, std::move(dictA)) { }
    ~MemStream() override;
};

class AutoFreeMemStream : public BaseMemStream<char>
{
    bool filterRemovalForbidden = false;

public:
    // AutoFreeMemStream takes ownership over the buffer.
    // The buffer should be created using gmalloc().
    AutoFreeMemStream(char *bufA, Goffset startA, Goffset lengthA, Object &&dictA) : BaseMemStream(bufA, startA, lengthA, std::move(dictA)) { }
    ~AutoFreeMemStream() override;

    // A hack to deal with the strange behaviour of PDFDoc::writeObject().
    bool isFilterRemovalForbidden() const;
    void setFilterRemovalForbidden(bool forbidden);
};

//------------------------------------------------------------------------
// EmbedStream
//
// This is a special stream type used for embedded streams (inline
// images).  It reads directly from the base stream -- after the
// EmbedStream is deleted, reads from the base stream will proceed where
// the BaseStream left off.  Note that this is very different behavior
// that creating a new FileStream (using makeSubStream).
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT EmbedStream : public BaseStream
{
public:
    EmbedStream(Stream *strA, Object &&dictA, bool limitedA, Goffset lengthA, bool reusableA = false);
    ~EmbedStream() override;
    BaseStream *copy() override;
    Stream *makeSubStream(Goffset start, bool limitedA, Goffset lengthA, Object &&dictA) override;
    StreamKind getKind() const override { return str->getKind(); }
    void reset() override;

    void setPos(Goffset pos, int dir = 0) override;
    Goffset getStart() override;
    void moveStart(Goffset delta) override;

    int getUnfilteredChar() override { return str->getUnfilteredChar(); }
    void unfilteredReset() override { str->unfilteredReset(); }

    void rewind();
    void restore();

    int getSomeChars(int nChars, unsigned char *buffer) override;
    Goffset getRawPos() override;

private:
    Stream *str;
    bool limited;
    bool reusable;
    bool record;
    bool replay;
    unsigned char *bufData;
    long bufMax;
    long bufLen;
    long bufPos;
    Goffset start;
};

//------------------------------------------------------------------------
// ASCIIHexStream
//------------------------------------------------------------------------

class ASCIIHexStream : public FilterStream
{
public:
    explicit ASCIIHexStream(Stream *strA);
    ~ASCIIHexStream() override;
    StreamKind getKind() const override { return strASCIIHex; }
    void reset() override;

    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar();
};

//------------------------------------------------------------------------
// ASCII85Stream
//------------------------------------------------------------------------

class ASCII85Stream : public FilterStream
{
public:
    explicit ASCII85Stream(Stream *strA);
    ~ASCII85Stream() override;
    StreamKind getKind() const override { return strASCII85; }
    void reset() override;
    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar();

    int c[5];
    int b[4];
    int index, n;
    bool eof;
};

//------------------------------------------------------------------------
// LZWStream
//------------------------------------------------------------------------

class LZWStream : public FilterStream
{
public:
    LZWStream(Stream *strA, int columns, int colors, int bits, int earlyA);
    ~LZWStream() override;
    StreamKind getKind() const override { return strLZW; }
    void reset() override;
    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    int getSomeChars(int nChars, unsigned char *buffer) override;

private:
    int early; // early parameter
    bool eof; // true if at eof
    unsigned int inputBuf; // input buffer
    int inputBits; // number of bits in input buffer
    struct
    { // decoding table
        int length;
        int head;
        unsigned char tail;
    } table[4097];
    int nextCode; // next code to be used
    int nextBits; // number of bits in next code word
    int prevCode; // previous code used in stream
    int newChar; // next char to be added to table
    unsigned char seqBuf[4097]; // buffer for current sequence
    int seqLength; // length of current sequence
    int seqIndex; // index into current sequence
    bool first; // first code after a table clear

    bool processNextCode();
    void clearTable();
    int getCode();
};

//------------------------------------------------------------------------
// RunLengthStream
//------------------------------------------------------------------------

class RunLengthStream : public FilterStream
{
public:
    explicit RunLengthStream(Stream *strA);
    ~RunLengthStream() override;
    StreamKind getKind() const override { return strRunLength; }
    void reset() override;

    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    int getSomeChars(int nChars, unsigned char *buffer) override;

private:
    char buf[128]; // buffer
    char *bufPtr; // next char to read
    char *bufEnd; // end of buffer
    bool eof;

    bool fillBuf();
};

//------------------------------------------------------------------------
// CCITTFaxStream
//------------------------------------------------------------------------

struct CCITTCodeTable;

class CCITTFaxStream : public FilterStream
{
public:
    CCITTFaxStream(Stream *strA, int encodingA, bool endOfLineA, bool byteAlignA, int columnsA, int rowsA, bool endOfBlockA, bool blackA, int damagedRowsBeforeErrorA);
    ~CCITTFaxStream() override;
    StreamKind getKind() const override { return strCCITTFax; }
    void reset() override;
    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    void unfilteredReset() override;

    int getEncoding() { return encoding; }
    bool getEndOfLine() { return endOfLine; }
    bool getEncodedByteAlign() { return byteAlign; }
    bool getEndOfBlock() { return endOfBlock; }
    int getColumns() { return columns; }
    bool getBlackIs1() { return black; }
    int getDamagedRowsBeforeError() { return damagedRowsBeforeError; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar();

    void ccittReset(bool unfiltered);
    int encoding; // 'K' parameter
    bool endOfLine; // 'EndOfLine' parameter
    bool byteAlign; // 'EncodedByteAlign' parameter
    int columns; // 'Columns' parameter
    int rows; // 'Rows' parameter
    bool endOfBlock; // 'EndOfBlock' parameter
    bool black; // 'BlackIs1' parameter
    int damagedRowsBeforeError; // 'DamagedRowsBeforeError' parameter
    bool eof; // true if at eof
    bool nextLine2D; // true if next line uses 2D encoding
    int row; // current row
    unsigned int inputBuf; // input buffer
    int inputBits; // number of bits in input buffer
    int *codingLine; // coding line changing elements
    int *refLine; // reference line changing elements
    int a0i; // index into codingLine
    bool err; // error on current line
    int outputBits; // remaining ouput bits

    void addPixels(int a1, int blackPixels);
    void addPixelsNeg(int a1, int blackPixels);
    short getTwoDimCode();
    short getWhiteCode();
    short getBlackCode();
    short lookBits(int n);
    void eatBits(int n)
    {
        if ((inputBits -= n) < 0) {
            inputBits = 0;
        }
    }
};

#ifndef ENABLE_LIBJPEG
//------------------------------------------------------------------------
// DCTStream
//------------------------------------------------------------------------

// DCT component info
struct DCTCompInfo
{
    int id; // component ID
    int hSample, vSample; // horiz/vert sampling resolutions
    int quantTable; // quantization table number
    int prevDC; // DC coefficient accumulator
};

struct DCTScanInfo
{
    bool comp[4]; // comp[i] is set if component i is
                  //   included in this scan
    int numComps; // number of components in the scan
    int dcHuffTable[4]; // DC Huffman table numbers
    int acHuffTable[4]; // AC Huffman table numbers
    int firstCoeff, lastCoeff; // first and last DCT coefficient
    int ah, al; // successive approximation parameters
};

// DCT Huffman decoding table
struct DCTHuffTable
{
    unsigned char firstSym[17]; // first symbol for this bit length
    unsigned short firstCode[17]; // first code for this bit length
    unsigned short numCodes[17]; // number of codes of this bit length
    unsigned char sym[256]; // symbols
};

class DCTStream : public FilterStream
{
public:
    DCTStream(Stream *strA, int colorXformA, Dict *dict, int recursion);
    ~DCTStream() override;
    StreamKind getKind() const override { return strDCT; }
    void reset() override;
    void close() override;
    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;

    void unfilteredReset() override;

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar();

    void dctReset(bool unfiltered);
    bool progressive; // set if in progressive mode
    bool interleaved; // set if in interleaved mode
    int width, height; // image size
    int mcuWidth, mcuHeight; // size of min coding unit, in data units
    int bufWidth, bufHeight; // frameBuf size
    DCTCompInfo compInfo[4]; // info for each component
    DCTScanInfo scanInfo; // info for the current scan
    int numComps; // number of components in image
    int colorXform; // color transform: -1 = unspecified
                    //                   0 = none
                    //                   1 = YUV/YUVK -> RGB/CMYK
    bool gotJFIFMarker; // set if APP0 JFIF marker was present
    bool gotAdobeMarker; // set if APP14 Adobe marker was present
    int restartInterval; // restart interval, in MCUs
    unsigned short quantTables[4][64]; // quantization tables
    int numQuantTables; // number of quantization tables
    DCTHuffTable dcHuffTables[4]; // DC Huffman tables
    DCTHuffTable acHuffTables[4]; // AC Huffman tables
    int numDCHuffTables; // number of DC Huffman tables
    int numACHuffTables; // number of AC Huffman tables
    unsigned char *rowBuf[4][32]; // buffer for one MCU (non-progressive mode)
    int *frameBuf[4]; // buffer for frame (progressive mode)
    int comp, x, y, dy; // current position within image/MCU
    int restartCtr; // MCUs left until restart
    int restartMarker; // next restart marker
    int eobRun; // number of EOBs left in the current run
    int inputBuf; // input buffer for variable length codes
    int inputBits; // number of valid bits in input buffer

    void restart();
    bool readMCURow();
    void readScan();
    bool readDataUnit(DCTHuffTable *dcHuffTable, DCTHuffTable *acHuffTable, int *prevDC, int data[64]);
    bool readProgressiveDataUnit(DCTHuffTable *dcHuffTable, DCTHuffTable *acHuffTable, int *prevDC, int data[64]);
    void decodeImage();
    void transformDataUnit(unsigned short *quantTable, int dataIn[64], unsigned char dataOut[64]);
    int readHuffSym(DCTHuffTable *table);
    int readAmp(int size);
    int readBit();
    bool readHeader();
    bool readBaselineSOF();
    bool readProgressiveSOF();
    bool readScanInfo();
    bool readQuantTables();
    bool readHuffmanTables();
    bool readRestartInterval();
    bool readJFIFMarker();
    bool readAdobeMarker();
    bool readTrailer();
    int readMarker();
    int read16();
};

#endif

#ifndef ENABLE_ZLIB_UNCOMPRESS
//------------------------------------------------------------------------
// FlateStream
//------------------------------------------------------------------------

#    define flateWindow 32768 // buffer size
#    define flateMask (flateWindow - 1)
#    define flateMaxHuffman 15 // max Huffman code length
#    define flateMaxCodeLenCodes 19 // max # code length codes
#    define flateMaxLitCodes 288 // max # literal codes
#    define flateMaxDistCodes 30 // max # distance codes

// Huffman code table entry
struct FlateCode
{
    unsigned short len; // code length, in bits
    unsigned short val; // value represented by this code
};

struct FlateHuffmanTab
{
    const FlateCode *codes;
    int maxLen;
};

// Decoding info for length and distance code words
struct FlateDecode
{
    int bits; // # extra bits
    int first; // first length/distance
};

class FlateStream : public FilterStream
{
public:
    FlateStream(Stream *strA, int columns, int colors, int bits);
    ~FlateStream() override;
    StreamKind getKind() const override { return strFlate; }
    void reset() override;

    GooString *getPSFilter(int psLevel, const char *indent) override;
    bool isBinary(bool last = true) const override;
    void unfilteredReset() override;

    polyfillGetSomeChars(doGetRawChar);

private:
    void flateReset(bool unfiltered);

    inline int doGetRawChar()
    {
        int c;

        while (remain == 0) {
            if (endOfBlock && eof) {
                return EOF;
            }
            readSome();
        }
        c = buf[index];
        index = (index + 1) & flateMask;
        --remain;
        return c;
    }

    unsigned char buf[flateWindow]; // output data buffer
    int index; // current index into output buffer
    int remain; // number valid bytes in output buffer
    int codeBuf; // input buffer
    int codeSize; // number of bits in input buffer
    int // literal and distance code lengths
            codeLengths[flateMaxLitCodes + flateMaxDistCodes];
    FlateHuffmanTab litCodeTab; // literal code table
    FlateHuffmanTab distCodeTab; // distance code table
    bool compressedBlock; // set if reading a compressed block
    int blockLen; // remaining length of uncompressed block
    bool endOfBlock; // set when end of block is reached
    bool eof; // set when end of stream is reached

    static const int // code length code reordering
            codeLenCodeMap[flateMaxCodeLenCodes];
    static const FlateDecode // length decoding info
            lengthDecode[flateMaxLitCodes - 257];
    static const FlateDecode // distance decoding info
            distDecode[flateMaxDistCodes];
    static FlateHuffmanTab // fixed literal code table
            fixedLitCodeTab;
    static FlateHuffmanTab // fixed distance code table
            fixedDistCodeTab;

    void readSome();
    bool startBlock();
    void loadFixedCodes();
    bool readDynamicCodes();
    FlateCode *compHuffmanCodes(const int *lengths, int n, int *maxLen);
    int getHuffmanCodeWord(FlateHuffmanTab *tab);
    int getCodeWord(int bits);
};
#endif

//------------------------------------------------------------------------
// EOFStream
//------------------------------------------------------------------------

class EOFStream : public FilterStream
{
public:
    explicit EOFStream(Stream *strA);
    ~EOFStream() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override { }

    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return false; }

    int getSomeChars(int nChars, unsigned char *buffer) override { return 0; }
};

//------------------------------------------------------------------------
// BufStream
//------------------------------------------------------------------------

class BufStream : public FilterStream
{
public:
    BufStream(Stream *strA, int bufSizeA);
    ~BufStream() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int psLevel, const char *indent) override { return nullptr; }
    bool isBinary(bool last = true) const override;

    int lookAheadChar(int idx);

    int getSomeChars(int nChars, unsigned char *buffer) override { return str->getSomeChars(nChars, buffer); }
};

//------------------------------------------------------------------------
// FixedLengthEncoder
//------------------------------------------------------------------------

class FixedLengthEncoder : public FilterStream
{
public:
    FixedLengthEncoder(Stream *strA, int lengthA);
    ~FixedLengthEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;

    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override;
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar();

    int length;
    int count;
};

//------------------------------------------------------------------------
// ASCIIHexEncoder
//------------------------------------------------------------------------

class ASCIIHexEncoder : public FilterStream
{
public:
    explicit ASCIIHexEncoder(Stream *strA);
    ~ASCIIHexEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return false; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar() { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff); }

    char buf[4];
    char *bufPtr;
    char *bufEnd;
    int lineLen;
    bool eof;

    bool fillBuf();
};

//------------------------------------------------------------------------
// ASCII85Encoder
//------------------------------------------------------------------------

class ASCII85Encoder : public FilterStream
{
public:
    explicit ASCII85Encoder(Stream *strA);
    ~ASCII85Encoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return false; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar() { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff); }

    char buf[8];
    char *bufPtr;
    char *bufEnd;
    int lineLen;
    bool eof;

    bool fillBuf();
};

//------------------------------------------------------------------------
// RunLengthEncoder
//------------------------------------------------------------------------

class RunLengthEncoder : public FilterStream
{
public:
    explicit RunLengthEncoder(Stream *strA);
    ~RunLengthEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return true; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar() { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff); }

    char buf[131];
    char *bufPtr;
    char *bufEnd;
    char *nextEnd;
    bool eof;

    bool fillBuf();
};

//------------------------------------------------------------------------
// LZWEncoder
//------------------------------------------------------------------------

struct LZWEncoderNode
{
    int byte;
    LZWEncoderNode *next; // next sibling
    LZWEncoderNode *children; // first child
};

class LZWEncoder : public FilterStream
{
public:
    explicit LZWEncoder(Stream *strA);
    ~LZWEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int psLevel, const char *indent) override { return nullptr; }
    bool isBinary(bool last = true) const override { return true; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar();

    LZWEncoderNode table[4096];
    int nextSeq;
    int codeLen;
    unsigned char inBuf[4096];
    int inBufLen;
    int outBuf;
    int outBufLen;
    bool needEOD;

    void fillBuf();
};

//------------------------------------------------------------------------
// CMYKGrayEncoder
//------------------------------------------------------------------------

class CMYKGrayEncoder : public FilterStream
{
public:
    explicit CMYKGrayEncoder(Stream *strA);
    ~CMYKGrayEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return false; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar() { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff); }

    char buf[2];
    char *bufPtr;
    char *bufEnd;
    bool eof;

    bool fillBuf();
};

//------------------------------------------------------------------------
// RGBGrayEncoder
//------------------------------------------------------------------------

class RGBGrayEncoder : public FilterStream
{
public:
    explicit RGBGrayEncoder(Stream *strA);
    ~RGBGrayEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return false; }
    bool isEncoder() const override { return true; }

    polyfillGetSomeChars(getRawChar);

private:
    int getRawChar() { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff); }

    char buf[2];
    char *bufPtr;
    char *bufEnd;
    bool eof;

    bool fillBuf();
};

//------------------------------------------------------------------------
// SplashBitmapCMYKEncoder
//
// This stream helps to condense SplashBitmaps (mostly of DeviceN8 type) into
// pure CMYK colors. In particular for a DeviceN8 bitmap it redacts the spot colorants.
//------------------------------------------------------------------------

class SplashBitmapCMYKEncoder : public Stream
{
public:
    explicit SplashBitmapCMYKEncoder(SplashBitmap *bitmapA);
    ~SplashBitmapCMYKEncoder() override;
    StreamKind getKind() const override { return strWeird; }
    void reset() override;
    GooString *getPSFilter(int /*psLevel*/, const char * /*indent*/) override { return nullptr; }
    bool isBinary(bool /*last = true*/) const override { return true; }

    // Although we are an encoder, we return false here, since we do not want do be auto-deleted by
    // successive streams.
    bool isEncoder() const override { return false; }

    int getUnfilteredChar() override { return getChar(); }
    void unfilteredReset() override { reset(); }

    BaseStream *getBaseStream() override { return nullptr; }
    Stream *getUndecodedStream() override { return this; }

    Dict *getDict() override { return nullptr; }
    Object *getDictObject() override { return nullptr; }

    void setPos(Goffset pos, int dir = 0) override;

    polyfillGetSomeChars(getRawChar);
    Goffset getRawPos() override;

private:
    int getRawChar();

    SplashBitmap *bitmap;
    size_t width;
    int height;

    std::vector<unsigned char> buf;
    size_t bufPtr;
    int curLine;

    bool fillBuf();
};

//------------------------------------------------------------------------
// Object Stream accessors.
//------------------------------------------------------------------------

inline void Object::streamReset()
{
    OBJECT_TYPE_CHECK(objStream);
    stream->reset();
}

inline void Object::streamClose()
{
    OBJECT_TYPE_CHECK(objStream);
    stream->close();
}

inline int Object::streamGetChar()
{
    OBJECT_TYPE_CHECK(objStream);
    return stream->getChar();
}

inline int Object::streamGetChars(int nChars, unsigned char *buffer)
{
    OBJECT_TYPE_CHECK(objStream);
    return stream->doGetChars(nChars, buffer);
}

inline Dict *Object::streamGetDict() const
{
    OBJECT_TYPE_CHECK(objStream);
    return stream->getDict();
}

#endif
