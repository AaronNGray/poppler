/* poppler-output-stream.h: glib interface to poppler
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
#include "config.h"
#include <cstring>

#include <glib.h>
#include <gio/gio.h>
#include <span>

#include "poppler-output-stream.h"

#ifndef __GI_SCANNER__
#    include <Stream.h>
#endif

GLibOutStream::GLibOutStream(GOutputStream *stream_, GCancellable *cancellable_, GError **error_) : stream(stream_), pos(0), inbuf(0), cancellable(cancellable_), error(error_)
{
    g_object_ref(G_OBJECT(stream));
    if (cancellable) {
        g_object_ref(G_OBJECT(cancellable));
    }
}

GLibOutStream::~GLibOutStream()
{
    g_object_unref(G_OBJECT(stream));
    if (cancellable) {
        g_object_ref(G_OBJECT(cancellable));
    }
}

void GLibOutStream::close()
{
    flush();
    if (!error || !*error) {
        g_output_stream_close(stream, cancellable, error);
    }
}

Goffset GLibOutStream::getPos()
{
    return pos;
}

/* For some reasons writing a single char to the stream may be very slow
   for a GOutputStream. Therefore, these writes are buffered. (PDF streams may
   be several MB and are written char by char). */
void GLibOutStream::put(char c)
{
    if (inbuf == 1024) {
        flush();
    }
    pos += 1;
    buffer[inbuf] = c;
    inbuf++;
}

void GLibOutStream::flush()
{
    if (inbuf > 0) {
        if (!error || !*error) {
            g_output_stream_write(stream, buffer, inbuf, cancellable, error);
        }
        inbuf = 0;
    }
}

size_t GLibOutStream::write(std::span<unsigned char> data)
{
    if (!error || !*error) {
        flush();
        gsize written = g_output_stream_write(stream, data.data(), data.size_bytes(), cancellable, error);
        pos += written;
        return written;
    }
    return 0;
}

void GLibOutStream::printf(const char *format, ...)
{
    flush();
    gsize written = 0;
    va_list argptr;
    va_start(argptr, format);

    if (!error || !*error) {
        g_output_stream_vprintf(stream, &written, cancellable, error, format, argptr);
    }

    va_end(argptr);
    pos += written;

    // it seems that the behavior of g_output_stream_printf is different from standard printf, null characters are filtered out, so we force them.
    if (written == 0 && !g_strcmp0(format, "%c")) {
        put('\0');
    }
}